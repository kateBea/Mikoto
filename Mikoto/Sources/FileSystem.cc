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

#include <filesystem>

#include <Filesystem/FileSystem.hh>

namespace Mikoto {

    auto Filesystem::StripFileName( std::string_view path ) -> std::string {
        Path rootPath{ path };
        rootPath.remove_filename();

        return rootPath.string();
    }

    auto Filesystem::GetGetAbsolutePath( std::string_view path ) -> Path {
        Path absolutePath{ std::filesystem::absolute( path ) };

        return absolutePath;
    }

    auto Filesystem::GetGetAbsolutePath( const Path &path ) -> Path {
        Path absolutePath{ std::filesystem::absolute( path ) };
        return absolutePath;
    }

    auto Filesystem::GetGetAbsolutePathString( std::string_view path ) -> std::string {
        return GetGetAbsolutePath(path).string();
    }

    auto Filesystem::GetGetAbsolutePathString( const Path &path ) -> std::string {
        return GetGetAbsolutePath(path).string();
    }
}