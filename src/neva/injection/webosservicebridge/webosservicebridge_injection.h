// Copyright 2019 LG Electronics, Inc.
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

#ifndef NEVA_INJECTION_WEBOSSERVICEBRIDGE_WEBOSSERVICEBRIDGE_INJECTION_H_
#define NEVA_INJECTION_WEBOSSERVICEBRIDGE_WEBOSSERVICEBRIDGE_INJECTION_H_

#include <memory>

#include "base/component_export.h"
#include "gin/arguments.h"

namespace blink {
class WebLocalFrame;
}

namespace injections {

class WebOSServiceBridgeProperties;

class COMPONENT_EXPORT(INJECTION) WebOSServiceBridgeWebAPI {
 public:
  static const char kInjectionName[];
  static const char kObsoleteName[];

  static void Install(blink::WebLocalFrame* frame);
  static void Uninstall(blink::WebLocalFrame* frame);

  static bool HasWaitingRequests();
  static bool IsClosing();
  static void SetAppInClosing(bool closing);

 private:
  static void WebOSServiceBridgeConstructorCallback(
      WebOSServiceBridgeProperties* properties,
      gin::Arguments* args);
};

}  // namespace injections

#endif  // NEVA_INJECTION_WEBOSSERVICEBRIDGE_WEBOSSERVICEBRIDGE_INJECTION_H_
