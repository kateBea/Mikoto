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

#ifndef MIKOTO_CMAKE_TRACY_VIEWER_HH
#define MIKOTO_CMAKE_TRACY_VIEWER_HH

#include <string>
#include <filesystem>

namespace mikoto::editor::tracy_viewer {

    struct Capture;

    using SelectFile = std::string ( * )( bool save, const char* extension );
    using OpenUrl = void ( * )( const char* url );

    enum class OpenError {
        eNone,
        eReadFailed,
        eInvalidTrace,
        eUnsupportedVersion,
        eConnectFailed,
    };

    [[nodiscard]] auto Open( const std::filesystem::path& path, const char* utf8Path, OpenError& error ) -> Capture*;
    [[nodiscard]] auto Connect() -> Capture*;
    [[nodiscard]] auto CanStop( const Capture& capture ) -> bool;

    auto Initialize( SelectFile selectFile, OpenUrl openUrl ) -> void;
    auto CollectFrameTextures() -> void;
    auto Stop( Capture& capture ) -> void;
    auto Close( Capture* capture ) -> void;
    [[nodiscard]] auto Draw( Capture& capture ) -> bool;

}

#endif// MIKOTO_CMAKE_TRACY_VIEWER_HH
