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

#include <Core/Platform.hh>
#include <Filesystem/FileSystem.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )
#include <windows.h>
#include <shlobj.h>
#endif

namespace Mikoto {

#if defined( MIKOTO_PLATFORM_WINDOWS )
    auto OpenAndSelectFile( const std::wstring &filePath ) -> void {
        PIDLIST_ABSOLUTE pidl{ ILCreateFromPathW( filePath.c_str() ) };
        if ( pidl ) {
            SHOpenFolderAndSelectItems( pidl, 0, nullptr, 0 );
            ILFree( pidl );
        }
    }
#endif

    auto Filesystem::StripFileName( std::string_view path ) -> std::string {
        Path rootPath{ path };
        rootPath.remove_filename();

        return rootPath.string();
    }

    auto Filesystem::GetProcessPath() -> Path {
        return std::filesystem::current_path();
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

    auto Filesystem::CreateIfNotExistsDirectory( const Path &path ) -> bool {
        std::error_code ec{};
        const bool created{ std::filesystem::create_directories(path, ec) };

        if (ec) {
            return false;
        }

        return created;
    }

    auto Filesystem::OpenInExplorer( const Path &path ) -> void {
#if defined( MIKOTO_PLATFORM_WINDOWS )
        if ( std::filesystem::is_regular_file( path ) ) {
            OpenAndSelectFile(path.wstring());
        } else {
            std::wstring widePath{ path.wstring() };
            ShellExecuteW( nullptr, L"open", widePath.c_str(), nullptr, nullptr, SW_SHOWDEFAULT );
        }
#endif
    }
}