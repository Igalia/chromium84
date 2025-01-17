// Copyright 2016 LG Electronics, Inc.
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

#include "webos/webview_base.h"

#include <unordered_map>

#include "base/lazy_instance.h"
#include "base/unguessable_token.h"
#include "components/viz/common/switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/http/http_util.h"
#include "neva/app_runtime/common/app_runtime_user_agent.h"
#include "neva/app_runtime/public/app_runtime_event.h"
#include "neva/app_runtime/public/proxy_settings.h"
#include "neva/app_runtime/webview.h"
#include "neva/app_runtime/webview_profile.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "webos/common/webos_event.h"

namespace {

static const char kCachedDisplayTitle[] = "index.html";

}  // namespace

namespace webos {

namespace {

// <process id, routing id>
using RenderId = std::pair<int32_t, int32_t>;
using RenderIdMap =
    std::unordered_map<RenderId, WebViewBase*, base::IntPairHash<RenderId>>;
base::LazyInstance<RenderIdMap>::Leaky g_render_id_map =
    LAZY_INSTANCE_INITIALIZER;

using FrameTreeNodeIdMap = std::unordered_map<int, WebViewBase*>;
base::LazyInstance<FrameTreeNodeIdMap>::Leaky g_frame_tree_node_id_map =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

///@name DEPRECATED_API
///@{
void WebViewBase::SetFontHinting(WebViewBase::FontRenderParams hinting) {
  switch (hinting) {
    case WebViewBase::FontRenderParams::HINTING_NONE:
    case WebViewBase::FontRenderParams::HINTING_SLIGHT:
    case WebViewBase::FontRenderParams::HINTING_MEDIUM:
    case WebViewBase::FontRenderParams::HINTING_FULL:
    default:
      NOTIMPLEMENTED() << "Obsolete, see http://repo.lge.com:8080/2486/";
      return;
  }
}
///@}

void WebViewBase::SetFileAccessBlocked(bool blocked) {
  NOTIMPLEMENTED();
}

const std::string WebViewBase::kSecurityOriginPostfix("-webos");

WebViewBase::WebViewBase(bool alt_storage_path, int width, int height) {
  // If screen is rotated then initial size might be different and default
  // values may lead to incorrectly scaled view for the first rendered frame.
  // That is why the default values are subject to the overriding.
  if (display::Screen::GetScreen()->GetNumDisplays() > 0) {
    gfx::Size displaySize =
        display::Screen::GetScreen()->GetPrimaryDisplay().bounds().size();
    width = displaySize.width();
    height = displaySize.height();
  }

  webview_ = new neva_app_runtime::WebView(width, height);
  webview_->SetDelegate(this);
  webview_->SetControllerDelegate(this);

  RenderId render_id(GetWebContents()->GetMainFrame()->GetProcess()->GetID(),
                     GetWebContents()->GetRenderViewHost()->GetRoutingID());
  int frame_tree_node_id =
      GetWebContents()->GetMainFrame()->GetFrameTreeNodeId();
  g_render_id_map.Get().emplace(render_id, this);
  g_frame_tree_node_id_map.Get().emplace(frame_tree_node_id, this);
}

WebViewBase::~WebViewBase() {
  RenderId render_id(GetWebContents()->GetMainFrame()->GetProcess()->GetID(),
                     GetWebContents()->GetRenderViewHost()->GetRoutingID());
  int frame_tree_node_id =
      GetWebContents()->GetMainFrame()->GetFrameTreeNodeId();
  g_render_id_map.Get().erase(render_id);
  g_frame_tree_node_id_map.Get().erase(frame_tree_node_id);

  webview_->SetDelegate(nullptr);
  delete webview_;
}

WebViewBase* WebViewBase::FromID(int render_process_id, int render_view_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  RenderIdMap* views = g_render_id_map.Pointer();
  auto it = views->find(RenderId(render_process_id, render_view_id));
  return it == views->end() ? nullptr : it->second;
}

WebViewBase* WebViewBase::FromFrameTreeNodeId(int frame_tree_node_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  FrameTreeNodeIdMap* map = g_frame_tree_node_id_map.Pointer();
  auto it = map->find(frame_tree_node_id);
  return it == map->end() ? nullptr : it->second;
}

void WebViewBase::Initialize(const std::string& app_id,
                             const std::string& app_path,
                             const std::string& trust_level,
                             const std::string& v8_snapshot_path,
                             const std::string& v8_extra_flags,
                             bool use_native_scroll) {
  SetAppPath(app_path);
  SetTrustLevel(trust_level);
  SetAppId(app_id);
  SetV8SnapshotPath(v8_snapshot_path);
  SetV8ExtraFlags(v8_extra_flags);
  SetUseNativeScroll(use_native_scroll);

  webview_->CreateRenderView();

  NOTIMPLEMENTED() << " native scrolls, allow mouse on/off event";
}

content::WebContents* WebViewBase::GetWebContents() {
  return webview_->GetWebContents();
}

void WebViewBase::AddUserStyleSheet(const std::string& sheet) {
  webview_->AddUserStyleSheet(sheet);
}

std::string WebViewBase::DefaultUserAgent() const {
  return neva_app_runtime::GetUserAgent();
}

std::string WebViewBase::UserAgent() const {
  return webview_->UserAgent();
}

void WebViewBase::LoadUrl(const std::string& url) {
  webview_->LoadUrl(GURL(url));
}

void WebViewBase::StopLoading() {
  webview_->StopLoading();
}

void WebViewBase::LoadExtension(const std::string& name) {
  webview_->LoadExtension("v8/" + name);
}

void WebViewBase::ClearExtensions() {
  webview_->ClearExtensions();
}

void WebViewBase::EnableInspectablePage() {
  GetWebContents()->SetInspectablePage(true);
}

void WebViewBase::DisableInspectablePage() {
  GetWebContents()->SetInspectablePage(false);
}

void WebViewBase::SetInspectable(bool enable) {
  webview_->SetInspectable(enable);
}

void WebViewBase::AddAvailablePluginDir(const std::string& directory) {
  NOTIMPLEMENTED();
}

void WebViewBase::AddCustomPluginDir(const std::string& directory) {
  webview_->AddCustomPluginDir(directory);
}

void WebViewBase::SetUserAgent(const std::string& useragent) {
  GetWebContents()->SetUserAgentOverride(
      blink::UserAgentOverride::UserAgentOnly(useragent), false);
}

void WebViewBase::SetBackgroundColor(int r, int g, int b, int alpha) {
  webview_->SetBackgroundColor(r, g, b, alpha);
}

void WebViewBase::SetAllowFakeBoldText(bool allow) {
  webview_->SetAllowFakeBoldText(allow);
}

void WebViewBase::SetShouldSuppressDialogs(bool suppress) {
  webview_->SetShouldSuppressDialogs(suppress);
}

void WebViewBase::SetAppId(const std::string& app_id) {
  webview_->SetAppId(app_id);
}

void WebViewBase::SetSecurityOrigin(const std::string& identifier) {
  webview_->SetSecurityOrigin(identifier);
}

void WebViewBase::SetAcceptLanguages(const std::string& languages) {
  webview_->SetAcceptLanguages(languages);
}

void WebViewBase::SetUseLaunchOptimization(bool enabled, int delay_ms) {
  webview_->SetUseLaunchOptimization(enabled, delay_ms);
}

void WebViewBase::SetUseEnyoOptimization(bool enabled) {
  webview_->SetUseEnyoOptimization(enabled);
}

void WebViewBase::SetAppPreloadHint(bool is_preload) {
  NOTIMPLEMENTED();
}

void WebViewBase::SetUseAccessibility(bool enabled) {
  if (enabled)
    GetWebContents()->EnableWebContentsOnlyAccessibilityMode();
  else
    GetWebContents()->SetAccessibilityMode(ui::AXMode());
}

void WebViewBase::SetBlockWriteDiskcache(bool blocked) {
  webview_->SetBlockWriteDiskcache(blocked);
}

void WebViewBase::SetCacheStorageUseMode(
    neva_app_runtime::StorageUseMode mode) {
  NOTIMPLEMENTED();
}

void WebViewBase::SetTransparentBackground(bool enable) {
  webview_->SetTransparentBackground(enable);
}

void WebViewBase::SetBoardType(const std::string& board_type) {
  webview_->SetBoardType(board_type);
}

void WebViewBase::SetMediaCodecCapability(const std::string& capability) {
  webview_->SetMediaCodecCapability(capability);
}

void WebViewBase::SetMediaPreferences(const std::string& preferences) {
  webview_->SetMediaPreferences(preferences);
}

void WebViewBase::SetSearchKeywordForCustomPlayer(bool enabled) {
  webview_->SetSearchKeywordForCustomPlayer(enabled);
}

void WebViewBase::SetUseUnlimitedMediaPolicy(bool enabled) {
  webview_->SetUseUnlimitedMediaPolicy(enabled);
}

void WebViewBase::SetActiveOnNonBlankPaint(bool active) {
  webview_->SetActiveOnNonBlankPaint(active);
}

void WebViewBase::SetViewportSize(int width, int height) {
  webview_->SetViewportSize(width, height);
}

void WebViewBase::NotifyMemoryPressure(MemoryPressureLevel level) {
  base::MemoryPressureListener::MemoryPressureLevel pressure_level =
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
  if (level == MemoryPressureLevel::MEMORY_PRESSURE_LOW) {
    pressure_level =
        base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;
  } else if (level == MemoryPressureLevel::MEMORY_PRESSURE_CRITICAL) {
    pressure_level =
        base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;
  }
  webview_->NotifyMemoryPressure(pressure_level);
}

void WebViewBase::SetVisible(bool visible) {
  webview_->SetVisible(visible);
}

void WebViewBase::SetProxyServer(const ProxySettings& proxy_settings) {
  neva_app_runtime::ProxySettings app_runtime_proxy_settings;
  app_runtime_proxy_settings.enabled = proxy_settings.enabled;
  app_runtime_proxy_settings.mode = proxy_settings.mode;
  app_runtime_proxy_settings.ip = proxy_settings.ip;
  app_runtime_proxy_settings.port = proxy_settings.port;
  app_runtime_proxy_settings.username = proxy_settings.username;
  app_runtime_proxy_settings.password = proxy_settings.password;
  app_runtime_proxy_settings.bypass_list = proxy_settings.bypass_list;
  GetProfile()->SetProxyServer(app_runtime_proxy_settings);
}

neva_app_runtime::WebPageVisibilityState WebViewBase::FromNativeVisibilityState(
    WebPageVisibilityState visibility_state) {
  switch (visibility_state) {
    case WebPageVisibilityStateVisible:
      return neva_app_runtime::WebPageVisibilityState::
          WebPageVisibilityStateVisible;
    case WebPageVisibilityStateHidden:
      return neva_app_runtime::WebPageVisibilityState::
          WebPageVisibilityStateHidden;
    case WebPageVisibilityStateLaunching:
      return neva_app_runtime::WebPageVisibilityState::
          WebPageVisibilityStateLaunching;
    default:
      return neva_app_runtime::WebPageVisibilityState::
          WebPageVisibilityStateVisible;
  }
}

void WebViewBase::SetVisibilityState(WebPageVisibilityState visibility_state) {
  webview_->SetVisibilityState(FromNativeVisibilityState(visibility_state));
}

void WebViewBase::SetPrerenderState() {
  NOTIMPLEMENTED();
}

void WebViewBase::DeleteWebStorages(const std::string& identifier) {
  webview_->DeleteWebStorages(identifier);
}

std::string WebViewBase::DocumentTitle() const {
  std::string document_title = webview_->DocumentTitle();
  if (document_title == kCachedDisplayTitle)
    return std::string();
  return document_title;
}

void WebViewBase::SuspendWebPageDOM() {
  webview_->SuspendDOM();
}

void WebViewBase::ResumeWebPageDOM() {
  webview_->ResumeDOM();
}

void WebViewBase::SuspendWebPageMedia() {
  webview_->SuspendMedia();
}

void WebViewBase::ResumeWebPageMedia() {
  webview_->ResumeMedia();
}

void WebViewBase::SuspendPaintingAndSetVisibilityHidden() {
  webview_->SuspendPaintingAndSetVisibilityHidden();
}

void WebViewBase::ResumePaintingAndSetVisibilityVisible() {
  webview_->ResumePaintingAndSetVisibilityVisible();
}

void WebViewBase::CommitLoadVisually() {
  webview_->CommitLoadVisually();
}

const std::string& WebViewBase::GetUrl() {
  return webview_->GetUrl();
}

void WebViewBase::RunJavaScript(const std::string& js_code) {
  webview_->RunJavaScript(js_code);
}

void WebViewBase::RunJavaScriptInAllFrames(const std::string& js_code) {
  webview_->RunJavaScriptInAllFrames(js_code);
}

void WebViewBase::Reload() {
  webview_->Reload();
}

int WebViewBase::RenderProcessPid() const {
  return webview_->RenderProcessPid();
}

bool WebViewBase::IsDrmEncrypted(const std::string& url) {
  return webview_->IsDrmEncrypted(url);
}

std::string WebViewBase::DecryptDrm(const std::string& url) {
  return webview_->DecryptDrm(url);
}

void WebViewBase::SetFocus(bool focus) {
  webview_->SetFocus(focus);
}

double WebViewBase::GetZoomFactor() {
  return webview_->GetZoomFactor();
}

void WebViewBase::SetZoomFactor(double factor) {
  webview_->SetZoomFactor(factor);
}

void WebViewBase::SetDoNotTrack(bool dnt) {
  webview_->SetDoNotTrack(dnt);
}

void WebViewBase::ForwardAppRuntimeEvent(neva_app_runtime::AppRuntimeEvent* event) {
  webview_->ForwardAppRuntimeEvent(event);
}

void WebViewBase::ForwardWebOSEvent(WebOSEvent* event) {
  // TODO: Need refactoring
  if (event->GetType() == WebOSEvent::KeyPress ||
      event->GetType() == WebOSEvent::KeyRelease) {
    WebOSKeyEvent* key_event = static_cast<WebOSKeyEvent*>(event);
    int keycode = key_event->GetCode();

    key_event->SetCode(keycode);
  }
  webview_->ForwardAppRuntimeEvent(event);
}

bool WebViewBase::CanGoBack() const {
  return webview_->CanGoBack();
}

bool WebViewBase::IsInputMethodActive() {
  NOTIMPLEMENTED();
  return false;
}

void WebViewBase::SetAdditionalContentsScale(float scale_x, float scale_y) {
  webview_->SetAdditionalContentsScale(scale_x, scale_y);
}

void WebViewBase::GoBack() {
  webview_->GoBack();
}

void WebViewBase::SetHardwareResolution(int width, int height) {
  webview_->SetHardwareResolution(width, height);
}

void WebViewBase::SetEnableHtmlSystemKeyboardAttr(bool enable) {
  webview_->SetEnableHtmlSystemKeyboardAttr(enable);
}

void WebViewBase::DropAllPeerConnections(DropPeerConnectionReason reason) {
  neva_app_runtime::DropPeerConnectionReason app_runtime_reason;
  switch (reason) {
    case DROP_PEER_CONNECTION_REASON_PAGE_HIDDEN:
      app_runtime_reason = neva_app_runtime::DropPeerConnectionReason::
          kDropPeerConnectionReasonPageHidden;
      break;
    case DROP_PEER_CONNECTION_REASON_UNKNOWN:
    default:
      app_runtime_reason = neva_app_runtime::DropPeerConnectionReason::
          kDropPeerConnectionReasonUnknown;
  }
  webview_->DropAllPeerConnections(app_runtime_reason);
}

void WebViewBase::DidDropAllPeerConnections(
    neva_app_runtime::DropPeerConnectionReason reason) {
  DropPeerConnectionReason webos_reason;
  switch (reason) {
    case neva_app_runtime::DropPeerConnectionReason::
        kDropPeerConnectionReasonPageHidden:
      webos_reason = DROP_PEER_CONNECTION_REASON_PAGE_HIDDEN;
      break;
    case neva_app_runtime::DropPeerConnectionReason::
        kDropPeerConnectionReasonUnknown:
    default:
      webos_reason = DROP_PEER_CONNECTION_REASON_UNKNOWN;
  }
  DidDropAllPeerConnections(webos_reason);
}

void WebViewBase::ActivateRendererCompositor() {
  webview_->ActivateRendererCompositor();
}

void WebViewBase::DeactivateRendererCompositor() {
  webview_->DeactivateRendererCompositor();
}

void WebViewBase::RequestInjectionLoading(const std::string& injection_name) {
  webview_->RequestInjectionLoading(injection_name);
}

// WebPreferences
void WebViewBase::SetAllowRunningInsecureContent(bool enable) {
  webview_->UpdatePreferencesAttribute(
      neva_app_runtime::WebView::Attribute::AllowRunningInsecureContent, enable);
}

void WebViewBase::SetAllowScriptsToCloseWindows(bool enable) {
  webview_->UpdatePreferencesAttribute(
      neva_app_runtime::WebView::Attribute::AllowScriptsToCloseWindows, enable);
}

void WebViewBase::SetAllowUniversalAccessFromFileUrls(bool enable) {
  webview_->UpdatePreferencesAttribute(
      neva_app_runtime::WebView::Attribute::AllowUniversalAccessFromFileUrls,
      enable);
}

void WebViewBase::SetRequestQuotaEnabled(bool enable) {
  webview_->UpdatePreferencesAttribute(
      neva_app_runtime::WebView::Attribute::RequestQuotaEnabled,
      enable);
}

void WebViewBase::SetSuppressesIncrementalRendering(bool enable) {
  webview_->UpdatePreferencesAttribute(
      neva_app_runtime::WebView::Attribute::SuppressesIncrementalRendering, enable);
}

void WebViewBase::SetDisallowScrollbarsInMainFrame(bool enable) {
  webview_->UpdatePreferencesAttribute(
      neva_app_runtime::WebView::Attribute::DisallowScrollbarsInMainFrame, enable);
}

void WebViewBase::SetDisallowScrollingInMainFrame(bool enable) {
  webview_->UpdatePreferencesAttribute(
      neva_app_runtime::WebView::Attribute::DisallowScrollingInMainFrame, enable);
}

void WebViewBase::SetJavascriptCanOpenWindows(bool enable) {
  webview_->UpdatePreferencesAttribute(
      neva_app_runtime::WebView::Attribute::JavascriptCanOpenWindows, enable);
}

void WebViewBase::SetSpatialNavigationEnabled(bool enable) {
  webview_->UpdatePreferencesAttribute(
      neva_app_runtime::WebView::Attribute::SpatialNavigationEnabled, enable);
}

void WebViewBase::SetSupportsMultipleWindows(bool enable) {
  webview_->UpdatePreferencesAttribute(
      neva_app_runtime::WebView::Attribute::SupportsMultipleWindows, enable);
}

void WebViewBase::SetCSSNavigationEnabled(bool enable) {
  webview_->UpdatePreferencesAttribute(
      neva_app_runtime::WebView::Attribute::CSSNavigationEnabled, enable);
}

void WebViewBase::SetV8DateUseSystemLocaloffset(bool enable) {
  webview_->UpdatePreferencesAttribute(
      neva_app_runtime::WebView::Attribute::V8DateUseSystemLocaloffset, enable);
}

void WebViewBase::SetAllowLocalResourceLoad(bool enable) {
  webview_->UpdatePreferencesAttribute(
      neva_app_runtime::WebView::Attribute::AllowLocalResourceLoad, enable);
}

void WebViewBase::SetLocalStorageEnabled(bool enable) {
  webview_->UpdatePreferencesAttribute(
      neva_app_runtime::WebView::Attribute::LocalStorageEnabled,
      enable);
}

void WebViewBase::SetWebSecurityEnabled(bool enable) {
  webview_->UpdatePreferencesAttribute(
      neva_app_runtime::WebView::Attribute::WebSecurityEnabled,
      enable);
}

void WebViewBase::SetKeepAliveWebApp(bool enable) {
  webview_->UpdatePreferencesAttribute(
      neva_app_runtime::WebView::Attribute::KeepAliveWebApp,
      enable);
}

void WebViewBase::SetAdditionalFontFamilyEnabled(bool enable) {
  webview_->UpdatePreferencesAttribute(
      neva_app_runtime::WebView::Attribute::AdditionalFontFamilyEnabled, enable);
}

void WebViewBase::SetDatabaseIdentifier(const std::string& identifier) {
  webview_->SetDatabaseIdentifier(identifier);
}

void WebViewBase::SetBackHistoryAPIDisabled(const bool on) {
  webview_->UpdatePreferencesAttribute(
      neva_app_runtime::WebView::Attribute::BackHistoryKeyDisabled, on);
}

void WebViewBase::SetNetworkStableTimeout(const double timeout) {
  webview_->SetNetworkQuietTimeout(timeout);
}

// FontFamily
void WebViewBase::SetStandardFontFamily(const std::string& font) {
  webview_->SetFontFamily(neva_app_runtime::WebView::FontFamily::StandardFont, font);
}

void WebViewBase::SetFixedFontFamily(const std::string& font) {
  webview_->SetFontFamily(neva_app_runtime::WebView::FontFamily::FixedFont, font);
}

void WebViewBase::SetSerifFontFamily(const std::string& font) {
  webview_->SetFontFamily(neva_app_runtime::WebView::FontFamily::SerifFont, font);
}

void WebViewBase::SetSansSerifFontFamily(const std::string& font) {
  webview_->SetFontFamily(
      neva_app_runtime::WebView::FontFamily::SansSerifFont, font);
}

void WebViewBase::SetCursiveFontFamily(const std::string& font) {
  webview_->SetFontFamily(neva_app_runtime::WebView::FontFamily::CursiveFont, font);
}

void WebViewBase::SetFantasyFontFamily(const std::string& font) {
  webview_->SetFontFamily(neva_app_runtime::WebView::FontFamily::FantasyFont, font);
}

void WebViewBase::LoadAdditionalFont(const std::string& url,
                                     const std::string& font) {
  NOTIMPLEMENTED();
}

void WebViewBase::UpdatePreferences() {
  webview_->UpdatePreferences();
}

void WebViewBase::SetAudioGuidanceOn(bool on) {
  NOTIMPLEMENTED();
}

void WebViewBase::ResetStateToMarkNextPaint() {
  load_visually_committed_called_ = false;
  webview_->ResetStateToMarkNextPaint();
}

void WebViewBase::SetAppPath(const std::string& app_path) {
  if (app_path_ == app_path)
    return;

  app_path_ = app_path;
}

void WebViewBase::SetTrustLevel(const std::string& trust_level) {
  if (trust_level_ == trust_level)
    return;

  trust_level_ = trust_level;
}

void WebViewBase::HandleBrowserControlCommand(
    const std::string& command,
    const std::vector<std::string>& arguments) {
  NOTIMPLEMENTED();
}

void WebViewBase::HandleBrowserControlFunction(
    const std::string& command,
    const std::vector<std::string>& arguments,
    std::string* result) {
  NOTIMPLEMENTED();
}

void WebViewBase::RunCommand(
      const std::string& command,
      const std::vector<std::string>& arguments) {
  HandleBrowserControlCommand(command, arguments);
}

std::string WebViewBase::RunFunction(
    const std::string& command,
    const std::vector<std::string>& arguments) {
  std::string result;
  HandleBrowserControlFunction(command, arguments, &result);
  return result;
}

void WebViewBase::SetSSLCertErrorPolicy(
    neva_app_runtime::SSLCertErrorPolicy policy) {
  webview_->SetSSLCertErrorPolicy(policy);
}

neva_app_runtime::SSLCertErrorPolicy WebViewBase::GetSSLCertErrorPolicy() {
  return webview_->GetSSLCertErrorPolicy();
}

neva_app_runtime::WebViewProfile* WebViewBase::GetProfile() const {
  return webview_->GetProfile();
}

void WebViewBase::SetProfile(neva_app_runtime::WebViewProfile* profile) {
  webview_->SetProfile(profile);
}

void WebViewBase::SetV8SnapshotPath(const std::string& v8_snapshot_path) {
  webview_->SetV8SnapshotPath(v8_snapshot_path);
}

void WebViewBase::SetV8ExtraFlags(const std::string& v8_extra_flags) {
  webview_->SetV8ExtraFlags(v8_extra_flags);
}

void WebViewBase::DidLoadingEnd() {
  VLOG(1) << __func__;
}

void WebViewBase::DidFirstMeaningfulPaint() {
  VLOG(1) << __func__;
  CallLoadVisuallyCommitted();
}

void WebViewBase::DidNonFirstMeaningfulPaint() {
  VLOG(1) << __func__;
  CallLoadVisuallyCommitted();
}

void WebViewBase::CallLoadVisuallyCommitted() {
  if (load_visually_committed_called_)
    return;

  LoadVisuallyCommitted();
  load_visually_committed_called_ = true;
}

void WebViewBase::SetUseNativeScroll(bool use_native_scroll) {
  webview_->SetUseNativeScroll(use_native_scroll);
}

}  // namespace webos
