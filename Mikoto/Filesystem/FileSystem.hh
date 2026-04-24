//    Copyright 2025 ケイト
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

#ifndef MIKOTO_FILESYSTEM_HH
#define MIKOTO_FILESYSTEM_HH

#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Filesystem/Path.hh>

namespace mikoto::filesystem {

    auto OpenInExplorer( const Path& path ) -> void;

    MKT_NODISCARD auto IsRegularFile(const Path& path) -> bool;
    MKT_NODISCARD auto IsDirectory(const Path& path) -> bool;

    // Process current working directory
    MKT_NODISCARD auto GetProcessPath() -> Path;

    MKT_NODISCARD auto StripFileName(eastl::string_view path) -> eastl::string;

    MKT_NODISCARD auto GetGetAbsolutePath(std::string_view path) -> Path;
    MKT_NODISCARD auto GetGetAbsolutePath(const Path& path) -> Path;
    MKT_NODISCARD auto GetGetAbsolutePathString(std::string_view path) -> std::string;
    MKT_NODISCARD auto GetGetAbsolutePathString(const Path& path) -> std::string;

    // Creates parent directories if they do not exist
    MKT_NODISCARD auto CreateIfNotExistsDirectory(const Path& path) -> bool;

    MKT_NODISCARD auto IsAbsolutePath(const Path& path) -> bool;
    MKT_NODISCARD auto IsRelativePath(const Path& path) -> bool;

    auto Notify( std::string_view message ) -> void;

    //MKT_NODISCARD auto OpenFile( /**/ ) -> filesystem::Path;
    //MKT_NODISCARD auto SaveFile( /**/ ) -> filesystem::Path;
    //MKT_NODISCARD auto SelectFolder( /**/ ) -> filesystem::Path;
}

#endif //MIKOTO_FILESYSTEM_HH