//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MIKOTO_VIDEO_VIDEO_PREVIEW_HH
#define MIKOTO_VIDEO_VIDEO_PREVIEW_HH

#include <Assets/Image.hh>
#include <Filesystem/Path.hh>

namespace mikoto::video {

    /**
     * @brief Decodes a representative video frame into owned, top-down RGBA8 pixels.
     * @param path Local media file supported by the platform decoder.
     * @returns An image near the beginning of the clip, without starting playback or audio.
     * @throws core::RuntimeException If decoding is unavailable, fails, or exceeds the 8192-pixel axis limit.
     * @details CPU-only and worker-safe. The Windows implementation owns its COM/Media Foundation
     * lifetime, so it does not depend on the Video service, scene, graphics API or render thread.
     */
    MKT_NODISCARD auto DecodeVideoPreview( const filesystem::Path& path ) -> asset::ImageHandle;

}// namespace mikoto::video

#endif// MIKOTO_VIDEO_VIDEO_PREVIEW_HH
