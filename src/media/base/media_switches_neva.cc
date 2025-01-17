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

#include "media/base/media_switches_neva.h"

namespace switches {

// Disable WebMdiaPlayerNeva integration
const char kDisableWebMediaPlayerNeva[] = "disable-web-media-player-neva";
// Use Neva Media Service to play media
const char kEnableNevaMediaService[] = "enable-neva-media-service";
const char kFakeUrlMediaDuration[] = "fake-url-media-duration";

#if defined(USE_NEVA_WEBRTC)
// Enables platforms to provide h/w accelarated video decoder.
const char kEnableWebRTCPlatformVideoDecoder[] =
    "enable-webrtc-platform-video-decoder";
// Enables platforms to provide h/w accelarated video encoder.
const char kEnableWebRTCPlatformVideoEncoder[] =
    "enable-webrtc-platform-video-encoder";
#endif

}  // namespace switches
