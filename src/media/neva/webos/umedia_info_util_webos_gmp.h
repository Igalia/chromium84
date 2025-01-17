// Copyright (c) 2019-2020 LG Electronics, Inc.
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

#ifndef MEDIA_NEVA_WEBOS_UMEDIA_INFO_UTIL_WEBOS_GMP_H_
#define MEDIA_NEVA_WEBOS_UMEDIA_INFO_UTIL_WEBOS_GMP_H_

#include <uMediaClient.h>

namespace media {

std::string UMSSourceInfoToJsonString(const std::string&,
                                      const struct ums::source_info_t&);
std::string UMSVideoInfoToJsonString(const std::string&,
                                     const struct ums::video_info_t&);
std::string UMSAudioInfoToJsonString(const std::string&,
                                     const struct ums::audio_info_t&);

}  // namespace media

#endif  // MEDIA_NEVA_WEBOS_UMEDIA_INFO_UTIL_WEBOS_GMP_H_
