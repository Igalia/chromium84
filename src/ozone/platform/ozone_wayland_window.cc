// Copyright 2014 Intel Corporation. All rights reserved.
// Copyright 2017 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include "ozone/platform/ozone_wayland_window.h"

#include <vector>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/memory/ref_counted_memory.h"
#include "base/message_loop/message_loop_current.h"
#include "base/threading/thread_restrictions.h"
#include "ozone/platform/messages.h"
#include "ozone/platform/ozone_gpu_platform_support_host.h"
#include "ozone/platform/window_manager_wayland.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/ozone/public/cursor_factory_ozone.h"
#include "ui/platform_window/neva/window_group_configuration.h"
#include "ui/platform_window/platform_window_delegate.h"

#if defined(OS_WEBOS)
#include "ozone/platform/webos_constants.h"

namespace webos {

// LSM defines hotspot for hide cursor (blank cursor)
const gfx::Point lsm_cursor_hide_location(254, 254);
// LSM defines hotspot for restoring to default webOS cursor
const gfx::Point lsm_cursor_restore_location(255, 255);

}  // namespace webos
#endif

namespace ui {
namespace {

scoped_refptr<base::RefCountedBytes> ReadFileData(const base::FilePath& path) {
  if (path.empty())
    return nullptr;

  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid())
    return nullptr;

  int64_t length = file.GetLength();
  if (length > 0 && length < INT_MAX) {
    int size = static_cast<int>(length);
    std::vector<unsigned char> raw_data;
    raw_data.resize(size);
    char* data = reinterpret_cast<char*>(&(raw_data.front()));
    if (file.ReadAtCurrentPos(data, size) == length)
      return base::RefCountedBytes::TakeVector(&raw_data);
  }
  return nullptr;
}

void CreateBitmapFromPng(
    neva_app_runtime::CustomCursorType type,
    const std::string& path,
    int hotspot_x,
    int hotspot_y,
    bool allowed_cursor_overriding,
    const base::Callback<void(neva_app_runtime::CustomCursorType,
                              SkBitmap*,
                              int,
                              int,
                              bool)> callback) {
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  scoped_refptr<base::RefCountedBytes> memory(
      ReadFileData(base::FilePath(path)));
  if (!memory.get()) {
    LOG(INFO) << "Unable to read file path = " << path;
    return;
  }

  SkBitmap* bitmap = new SkBitmap();
  if (!gfx::PNGCodec::Decode(memory->front(), memory->size(), bitmap)) {
    LOG(INFO) << "Unable to decode image path = " << path;
    delete bitmap;
    return;
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, type, bitmap, hotspot_x, hotspot_y,
                            allowed_cursor_overriding));
}

}  // namespace

OzoneWaylandWindow::OzoneWaylandWindow(PlatformWindowDelegate* delegate,
                                       OzoneGpuPlatformSupportHost* sender,
                                       WindowManagerWayland* window_manager,
                                       const gfx::Rect& bounds)
    : delegate_(delegate),
      sender_(sender),
      window_manager_(window_manager),
      transparent_(false),
      bounds_(bounds),
      resize_enabled_(true),
      parent_(0),
      type_(WidgetType::WINDOWFRAMELESS),
      state_(WidgetState::UNINITIALIZED),
      region_(NULL),
      init_window_(false),
      weak_factory_(this) {
  static int opaque_handle = 0;
  opaque_handle++;
  handle_ = opaque_handle;
  delegate_->OnAcceleratedWidgetAvailable(opaque_handle);

  PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
  sender_->AddChannelObserver(this);
  window_manager_->OnRootWindowCreated(this);
}

OzoneWaylandWindow::~OzoneWaylandWindow() {
  sender_->RemoveChannelObserver(this);
  PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
  sender_->Send(new WaylandDisplay_DestroyWindow(handle_));
  if (region_)
    delete region_;
}

void OzoneWaylandWindow::InitPlatformWindow(
    neva::PlatformWindowType type, gfx::AcceleratedWidget parent_window) {
  switch (type) {
    case neva::PlatformWindowType::kPopup:
    case neva::PlatformWindowType::kMenu: {
      parent_ = parent_window;
      if (!parent_ && window_manager_->GetActiveWindow(display_id_))
        parent_ = window_manager_->GetActiveWindow(display_id_)->GetHandle();
      type_ = ui::WidgetType::POPUP;
      ValidateBounds();
      break;
    }
    case neva::PlatformWindowType::kTooltip: {
      parent_ = parent_window;
      if (!parent_ && window_manager_->GetActiveWindow(display_id_))
        parent_ = window_manager_->GetActiveWindow(display_id_)->GetHandle();
      type_ = ui::WidgetType::TOOLTIP;
      bounds_.set_origin(gfx::Point(0, 0));
      break;
    }
    case neva::PlatformWindowType::kBubble:
    case neva::PlatformWindowType::kWindow:
      parent_ = 0;
      type_ = ui::WidgetType::WINDOW;
      break;
    case neva::PlatformWindowType::kWindowFrameless:
      NOTIMPLEMENTED();
      break;
    default:
      break;
  }

  init_window_ = true;

  if (!sender_->IsConnected())
    return;

  sender_->Send(
      new WaylandDisplay_InitWindow(handle_, parent_, bounds_, type_));
}

void OzoneWaylandWindow::SetTitle(const base::string16& title) {
  title_ = title;
  if (!sender_->IsConnected())
    return;

  sender_->Send(new WaylandDisplay_Title(handle_, title_));
}

void OzoneWaylandWindow::SetWindowShape(const SkPath& path) {
  ResetRegion();
  if (transparent_)
    return;

  region_ = new SkRegion();
  SkRegion clip_region;
  clip_region.setRect({0, 0, bounds_.width(), bounds_.height()});
  region_->setPath(path, clip_region);
  AddRegion();
}

void OzoneWaylandWindow::SetOpacity(float opacity) {
  if (opacity == 1.f) {
    if (transparent_) {
      AddRegion();
      transparent_ = false;
    }
  } else if (!transparent_) {
    ResetRegion();
    transparent_ = true;
  }
}

void OzoneWaylandWindow::RequestDragData(const std::string& mime_type) {
  sender_->Send(new WaylandDisplay_RequestDragData(mime_type));
}

void OzoneWaylandWindow::RequestSelectionData(const std::string& mime_type) {
  sender_->Send(new WaylandDisplay_RequestSelectionData(mime_type));
}

void OzoneWaylandWindow::DragWillBeAccepted(uint32_t serial,
                                            const std::string& mime_type) {
  sender_->Send(new WaylandDisplay_DragWillBeAccepted(serial, mime_type));
}

void OzoneWaylandWindow::DragWillBeRejected(uint32_t serial) {
  sender_->Send(new WaylandDisplay_DragWillBeRejected(serial));
}

gfx::Rect OzoneWaylandWindow::GetBounds() {
  return bounds_;
}

void OzoneWaylandWindow::SetBounds(const gfx::Rect& bounds) {
  int original_x = bounds_.x();
  int original_y = bounds_.y();
  bounds_ = bounds;
  if (type_ == ui::WidgetType::TOOLTIP)
    ValidateBounds();

  if ((original_x != bounds_.x()) || (original_y  != bounds_.y())) {
    sender_->Send(new WaylandDisplay_MoveWindow(handle_, parent_,
                                                type_, bounds_));
  }

  delegate_->OnBoundsChanged(bounds_);
}

void OzoneWaylandWindow::Show(bool inactive) {
  state_ = WidgetState::SHOW;
  SendWidgetState();
}

void OzoneWaylandWindow::Hide() {
  state_ = WidgetState::HIDE;

  if (type_ == ui::WidgetType::TOOLTIP)
    delegate_->OnCloseRequest();
  else
    SendWidgetState();
}

void OzoneWaylandWindow::Close() {
  window_manager_->OnRootWindowClosed(this);
}

bool OzoneWaylandWindow::IsVisible() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return true;
}

void OzoneWaylandWindow::PrepareForShutdown() {}

void OzoneWaylandWindow::SetCapture() {
  window_manager_->GrabEvents(handle_);
}

void OzoneWaylandWindow::ReleaseCapture() {
  window_manager_->UngrabEvents(handle_);
}

bool OzoneWaylandWindow::HasCapture() const {
  return false;
}

void OzoneWaylandWindow::ToggleFullscreen() {
  display::Screen *screen = display::Screen::GetScreen();
  if (!screen)
    NOTREACHED() << "Unable to retrieve valid display::Screen";

  VLOG(1) << __PRETTY_FUNCTION__;
  SetBounds(screen->GetPrimaryDisplay().bounds());
  state_ = WidgetState::FULLSCREEN;
  SendWidgetState();
}

void OzoneWaylandWindow::ToggleFullscreenWithSize(const gfx::Size& size) {
  if (size.width() == 0 || size.height() == 0) {
    ToggleFullscreen();
    return;
  }
  VLOG(1) << __PRETTY_FUNCTION__;
  SetBounds(gfx::Rect(size.width(), size.height()));
  state_ = WidgetState::FULLSCREEN;
  SendWidgetState();
}

void OzoneWaylandWindow::Maximize() {
  display::Screen *screen = display::Screen::GetScreen();
  if (!screen)
    NOTREACHED() << "Unable to retrieve valid display::Screen";
  VLOG(1) << __PRETTY_FUNCTION__;
  SetBounds(screen->GetPrimaryDisplay().bounds());
  state_ = WidgetState::MAXIMIZED;
  SendWidgetState();
}

void OzoneWaylandWindow::Minimize() {
  VLOG(1) << __PRETTY_FUNCTION__;
  SetBounds(gfx::Rect());
  state_ = WidgetState::MINIMIZED;
  SendWidgetState();
}

void OzoneWaylandWindow::Restore() {
  VLOG(1) << __PRETTY_FUNCTION__;
  window_manager_->Restore(this);
  state_ = WidgetState::RESTORE;
  SendWidgetState();
}

PlatformWindowState OzoneWaylandWindow::GetPlatformWindowState() const {
  NOTIMPLEMENTED();
  return PlatformWindowState::kUnknown;
}

void OzoneWaylandWindow::Activate() {
  NOTIMPLEMENTED();
}

void OzoneWaylandWindow::Deactivate() {
  NOTIMPLEMENTED();
}

void OzoneWaylandWindow::SetUseNativeFrame(bool use_native_frame) {}

bool OzoneWaylandWindow::ShouldUseNativeFrame() const {
  return false;
}

void OzoneWaylandWindow::SetCursor(PlatformCursor cursor) {
  // Forbid to change cursor if it was overridden or if the same cursor is
  // already used.
  if (allowed_cursor_overriding_ ||
      window_manager_->GetPlatformCursor() == cursor)
    return;

  scoped_refptr<BitmapCursorOzone> bitmap =
      BitmapCursorFactoryOzone::GetBitmapCursor(cursor);
  bitmap_ = bitmap;
  window_manager_->SetPlatformCursor(cursor);
  if (!sender_->IsConnected())
    return;

  SetCursor();
}

void OzoneWaylandWindow::MoveCursorTo(const gfx::Point& location) {
  sender_->Send(new WaylandDisplay_MoveCursor(location));
}

void OzoneWaylandWindow::ConfineCursorToBounds(const gfx::Rect& bounds) {
}

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHostDelegateWayland, ui::PlatformEventDispatcher implementation:
bool OzoneWaylandWindow::CanDispatchEvent(const ui::PlatformEvent& ne) {
  if (ne->IsTouchEvent()) {
    unsigned touch_button_grabber =
        window_manager_->TouchButtonGrabber(ne->source_device_id());
    return touch_button_grabber == handle_;
  }
  unsigned device_event_grabber =
      window_manager_->DeviceEventGrabber(ne->source_device_id());
  if (device_event_grabber != 0)
    return device_event_grabber == handle_;
  return window_manager_->event_grabber() == gfx::AcceleratedWidget(handle_);
}

uint32_t OzoneWaylandWindow::DispatchEvent(
    const ui::PlatformEvent& ne) {
  DispatchEventFromNativeUiEvent(
      ne, base::Bind(&PlatformWindowDelegate::DispatchEvent,
                     base::Unretained(delegate_)));
  return POST_DISPATCH_STOP_PROPAGATION;
}

void OzoneWaylandWindow::OnGpuProcessLaunched() {
  if (sender_->IsConnected())
    DeferredSendingToGpu();
}

void OzoneWaylandWindow::DeferredSendingToGpu() {
  sender_->Send(new WaylandDisplay_Create(handle_));
  if (init_window_)
    sender_->Send(
        new WaylandDisplay_InitWindow(handle_, parent_, bounds_, type_));

  if (state_ != WidgetState::UNINITIALIZED)
    sender_->Send(new WaylandDisplay_State(handle_, state_));

  if (title_.length())
    sender_->Send(new WaylandDisplay_Title(handle_, title_));

  AddRegion();
  if (bitmap_)
    SetCursor();
}

void OzoneWaylandWindow::OnChannelDestroyed() {
}

void OzoneWaylandWindow::SendWidgetState() {
  if (!sender_->IsConnected())
    return;

  sender_->Send(new WaylandDisplay_State(handle_, state_));
}

void OzoneWaylandWindow::AddRegion() {
  if (sender_->IsConnected() && region_ && !region_->isEmpty()) {
     const SkIRect& rect = region_->getBounds();
     sender_->Send(new WaylandDisplay_AddRegion(handle_,
                                                rect.left(),
                                                rect.top(),
                                                rect.right(),
                                                rect.bottom()));
  }
}

void OzoneWaylandWindow::ResetRegion() {
  if (region_) {
    if (sender_->IsConnected() && !region_->isEmpty()) {
      const SkIRect& rect = region_->getBounds();
      sender_->Send(new WaylandDisplay_SubRegion(handle_,
                                                 rect.left(),
                                                 rect.top(),
                                                 rect.right(),
                                                 rect.bottom()));
    }

    delete region_;
    region_ = NULL;
  }
}

void OzoneWaylandWindow::SetCursor() {
  if (bitmap_) {
    sender_->Send(new WaylandDisplay_CursorSet(bitmap_->bitmaps(),
                                               bitmap_->hotspot()));
  } else {
    sender_->Send(new WaylandDisplay_CursorSet(std::vector<SkBitmap>(),
                                               gfx::Point()));
  }
}

void OzoneWaylandWindow::ValidateBounds() {
  DCHECK(parent_);
  if (!parent_) {
    LOG(INFO) << "Validate bounds will not do, parent is null";
    return;
  }

  gfx::Rect parent_bounds = window_manager_->GetWindow(parent_)->GetBounds();
  int x = bounds_.x() - parent_bounds.x();
  int y = bounds_.y() - parent_bounds.y();

  if (x < parent_bounds.x()) {
    x = parent_bounds.x();
  } else {
    int width = x + bounds_.width();
    if (width > parent_bounds.width())
      x -= width - parent_bounds.width();
  }

  if (y < parent_bounds.y()) {
    y = parent_bounds.y();
  } else {
    int height = y + bounds_.height();
    if (height > parent_bounds.height())
      y -= height - parent_bounds.height();
  }

  bounds_.set_origin(gfx::Point(x, y));
}

void OzoneWaylandWindow::SetRestoredBoundsInPixels(const gfx::Rect& bounds) {
  // TODO: https://crbug.com/848131
  NOTIMPLEMENTED();
}

gfx::Rect OzoneWaylandWindow::GetRestoredBoundsInPixels() const {
  // TODO: https://crbug.com/848131
  NOTIMPLEMENTED();
  return gfx::Rect();
}

void OzoneWaylandWindow::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                        const gfx::ImageSkia& app_icon) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void OzoneWaylandWindow::SizeConstraintsChanged() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void OzoneWaylandWindow::SetWindowProperty(const std::string& name,
                                           const std::string& value) {
  // FIXME : We should have separated API for set display ID.
  if (name == "displayAffinity" && display_id_ != value) {
    std::string prev_display_id = display_id_;
    display_id_ = value;
    window_manager_->OnRootWindowDisplayChanged(prev_display_id, display_id_,
                                                this);
  }

  sender_->Send(new WaylandDisplay_SetWindowProperty(handle_, name, value));
}

void OzoneWaylandWindow::ResetCustomCursor() {
  if (bitmap_) {
    sender_->Send(new WaylandDisplay_CursorSet(bitmap_->bitmaps(),
                                               bitmap_->hotspot()));
  }
#if defined(OS_WEBOS)
  else if (cursor_type_ == neva_app_runtime::CustomCursorType::kBlank) {
    // BLANK : Disable cursor(hiding cursor)
    sender_->Send(
        new WaylandDisplay_CursorSet(std::vector<SkBitmap>(),
                                     webos::lsm_cursor_hide_location));
  } else {
    // NOT_USE : Restore cursor(wayland cursor or IM's cursor)
    sender_->Send(
        new WaylandDisplay_CursorSet(std::vector<SkBitmap>(),
                                     webos::lsm_cursor_restore_location));
  }
#endif
}

void OzoneWaylandWindow::SetLocationHint(gfx::LocationHint value) {
  sender_->Send(new WaylandDisplay_SetLocationHint(handle_, value));
}

void OzoneWaylandWindow::CreateGroup(
    const ui::WindowGroupConfiguration& config) {
  sender_->Send(new WaylandDisplay_CreateWindowGroup(handle_, config));
}

void OzoneWaylandWindow::AttachToGroup(const std::string& group,
                                       const std::string& layer) {
  sender_->Send(new WaylandDisplay_AttachToWindowGroup(handle_, group, layer));
}

void OzoneWaylandWindow::FocusGroupOwner() {
  sender_->Send(new WaylandDisplay_FocusWindowGroupOwner(handle_));
}

void OzoneWaylandWindow::FocusGroupLayer() {
  sender_->Send(new WaylandDisplay_FocusWindowGroupLayer(handle_));
}

void OzoneWaylandWindow::DetachGroup() {
  sender_->Send(new WaylandDisplay_DetachWindowGroup(handle_));
}

std::string OzoneWaylandWindow::GetDisplayId() {
  return display_id_;
}

void OzoneWaylandWindow::ShowInputPanel() {
  sender_->Send(new WaylandDisplay_ShowInputPanel(handle_));
}

void OzoneWaylandWindow::HideInputPanel(ImeHiddenType hidden_type) {
  sender_->Send(new WaylandDisplay_HideInputPanel(hidden_type, handle_));
}

void OzoneWaylandWindow::SetTextInputInfo(
    const ui::TextInputInfo& text_input_info) {
  sender_->Send(new WaylandDisplay_SetTextInputInfo(text_input_info, handle_));
}

void OzoneWaylandWindow::SetSurroundingText(const std::string& text,
                                            size_t cursor_position,
                                            size_t anchor_position) {
  sender_->Send(new WaylandDisplay_SetSurroundingText(
      handle_, text, cursor_position, anchor_position));
}

void OzoneWaylandWindow::SetResizeEnabled(bool enabled) {
  resize_enabled_ = enabled;
}

void OzoneWaylandWindow::XInputActivate(const std::string& type) {
  sender_->Send(new WaylandDisplay_XInputActivate(type));
}

void OzoneWaylandWindow::XInputDeactivate() {
  sender_->Send(new WaylandDisplay_XInputDeactivate());
}

void OzoneWaylandWindow::XInputInvokeAction(uint32_t keysym,
                                            ui::XInputKeySymbolType symbol_type,
                                            ui::XInputEventType event_type) {
  sender_->Send(
      new WaylandDisplay_XInputInvokeAction(keysym, symbol_type, event_type));
}

void OzoneWaylandWindow::SetCustomCursor(neva_app_runtime::CustomCursorType type,
                                         const std::string& path,
                                         int hotspot_x,
                                         int hotspot_y,
                                         bool allowed_cursor_overriding) {
  // There are two possible states:
  // 1. Each html element could use its own cursor.
  // 2. One cursor is used for whole application.
  // Switching from state 1 to state 2 is a valid scenario only.
  if (allowed_cursor_overriding_ && !allowed_cursor_overriding)
    return;
  if (type != neva_app_runtime::CustomCursorType::kPath && type == cursor_type_ &&
      window_manager_->GetPlatformCursor() == nullptr)
    return;

  cursor_type_ = type;
  window_manager_->SetPlatformCursor(nullptr);
  allowed_cursor_overriding_ = allowed_cursor_overriding;

  if (type == neva_app_runtime::CustomCursorType::kPath) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&CreateBitmapFromPng, type, path, hotspot_x, hotspot_y,
                   allowed_cursor_overriding,
                   base::Bind(&OzoneWaylandWindow::SetCustomCursorFromBitmap,
                              weak_factory_.GetWeakPtr())));
  }
#if defined(OS_WEBOS)
  else if (type == neva_app_runtime::CustomCursorType::kBlank) {
    // BLANK : Disable cursor(hiding cursor)
    sender_->Send(
        new WaylandDisplay_CursorSet(std::vector<SkBitmap>(),
                                     webos::lsm_cursor_hide_location));
    bitmap_ = nullptr;
  } else {
    // NOT_USE : Restore cursor(wayland cursor or IM's cursor)
    sender_->Send(
        new WaylandDisplay_CursorSet(std::vector<SkBitmap>(),
                                     webos::lsm_cursor_restore_location));
    bitmap_ = nullptr;
  }
#endif
}

void OzoneWaylandWindow::SetCustomCursorFromBitmap(
    neva_app_runtime::CustomCursorType type,
    SkBitmap* cursor_image,
    int hotspot_x,
    int hotspot_y,
    bool allowed_cursor_overriding) {
  if (!cursor_image) {
    SetCustomCursor(neva_app_runtime::CustomCursorType::kNotUse, "", 0, 0,
        allowed_cursor_overriding);
    return;
  }

  PlatformCursor cursor = CursorFactoryOzone::GetInstance()->CreateImageCursor(
      *cursor_image, gfx::Point(hotspot_x, hotspot_y), 0);

  bitmap_ = BitmapCursorFactoryOzone::GetBitmapCursor(cursor);
  window_manager_->SetPlatformCursor(nullptr);
  if (sender_->IsConnected())
    SetCursor();

  delete cursor_image;
}

void OzoneWaylandWindow::SetInputRegion(const std::vector<gfx::Rect>& region) {
  sender_->Send(new WaylandDisplay_SetInputRegion(handle_, region));
}

void OzoneWaylandWindow::SetGroupKeyMask(ui::KeyMask key_mask) {
  sender_->Send(new WaylandDisplay_SetGroupKeyMask(handle_, key_mask));
}

void OzoneWaylandWindow::SetKeyMask(ui::KeyMask key_mask, bool set) {
  sender_->Send(new WaylandDisplay_SetKeyMask(handle_, key_mask, set));
}

}  // namespace ui
