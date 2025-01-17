// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/process_map.h"

#include <tuple>

#include "content/public/browser/child_process_security_policy.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_map_factory.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature.h"

namespace extensions {

// Item
struct ProcessMap::Item {
  Item(const std::string& extension_id, int process_id,
       int site_instance_id)
      : extension_id(extension_id),
        process_id(process_id),
        site_instance_id(site_instance_id) {
  }

  ~Item() {
  }

  Item(ProcessMap::Item&&) = default;
  Item& operator=(ProcessMap::Item&&) = default;

  bool operator<(const ProcessMap::Item& other) const {
    return std::tie(extension_id, process_id, site_instance_id) <
           std::tie(other.extension_id, other.process_id,
                    other.site_instance_id);
  }

  std::string extension_id;
  int process_id = 0;
  int site_instance_id = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Item);
};


// ProcessMap
ProcessMap::ProcessMap() {
}

ProcessMap::~ProcessMap() {
}

// static
ProcessMap* ProcessMap::Get(content::BrowserContext* browser_context) {
  return ProcessMapFactory::GetForBrowserContext(browser_context);
}

bool ProcessMap::Insert(const std::string& extension_id, int process_id,
                        int site_instance_id) {
  return items_.insert(Item(extension_id, process_id, site_instance_id)).second;
}

bool ProcessMap::Remove(const std::string& extension_id, int process_id,
                        int site_instance_id) {
  return items_.erase(Item(extension_id, process_id, site_instance_id)) > 0;
}

int ProcessMap::RemoveAllFromProcess(int process_id) {
  int result = 0;
  for (auto iter = items_.begin(); iter != items_.end();) {
    if (iter->process_id == process_id) {
      items_.erase(iter++);
      ++result;
    } else {
      ++iter;
    }
  }
  return result;
}

bool ProcessMap::Contains(const std::string& extension_id,
                          int process_id) const {
  for (auto iter = items_.cbegin(); iter != items_.cend(); ++iter) {
    if (iter->process_id == process_id && iter->extension_id == extension_id)
      return true;
  }
  return false;
}

bool ProcessMap::Contains(int process_id) const {
  for (auto iter = items_.cbegin(); iter != items_.cend(); ++iter) {
    if (iter->process_id == process_id)
      return true;
  }
  return false;
}

std::set<std::string> ProcessMap::GetExtensionsInProcess(int process_id) const {
  std::set<std::string> result;
  for (auto iter = items_.cbegin(); iter != items_.cend(); ++iter) {
    if (iter->process_id == process_id)
      result.insert(iter->extension_id);
  }
  return result;
}

Feature::Context ProcessMap::GetMostLikelyContextType(
    const Extension* extension,
    int process_id,
    const GURL* url) const {
#if defined(USE_NEVA_EXTENSIONS)
  return Feature::BLESSED_EXTENSION_CONTEXT;
#else
  // WARNING: This logic must match ScriptContextSet::ClassifyJavaScriptContext,
  // as much as possible.

  // TODO(crbug.com/1055168): Move this into the !extension if statement below
  // or document why we want to return WEBUI_CONTEXT for content scripts in
  // WebUIs.
  // TODO(crbug.com/1055656): HasWebUIBindings does not always return true for
  // WebUIs. This should be changed to use something else.
  if (content::ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
          process_id)) {
    return Feature::WEBUI_CONTEXT;
  }

  if (!extension) {
    // Note that blob/filesystem schemes associated with an inner URL of
    // chrome-untrusted will be considered regular pages.
    if (url && url->SchemeIs(content::kChromeUIUntrustedScheme))
      return Feature::WEBUI_UNTRUSTED_CONTEXT;

    return Feature::WEB_PAGE_CONTEXT;
  }

  if (!Contains(extension->id(), process_id)) {
    // This could equally be UNBLESSED_EXTENSION_CONTEXT, but we don't record
    // which processes have extension frames in them.
    // TODO(kalman): Investigate this.
    return Feature::CONTENT_SCRIPT_CONTEXT;
  }

  if (extension->is_hosted_app() &&
      extension->location() != Manifest::COMPONENT) {
    return Feature::BLESSED_WEB_PAGE_CONTEXT;
  }

  return is_lock_screen_context_ ? Feature::LOCK_SCREEN_EXTENSION_CONTEXT
                                 : Feature::BLESSED_EXTENSION_CONTEXT;
#endif
}

}  // namespace extensions
