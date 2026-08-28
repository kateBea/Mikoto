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

#include <portable-file-dialogs.h>

#include <Core/Platform.hh>
#include <Filesystem/FileSystem.hh>

#include <Logging/Logger.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )
#include <Platform/PlatformWin32.hh>
#include <windows.h>
#include <shlobj.h>
#endif

namespace mikoto::filesystem {

#if defined( MIKOTO_PLATFORM_WINDOWS )
     auto OpenAndSelectFile( const eastl::wstring &filePath ) -> void {
         PIDLIST_ABSOLUTE pidl{ ILCreateFromPathW( filePath.c_str() ) };
         if ( pidl ) {
             SHOpenFolderAndSelectItems( pidl, 0, nullptr, 0 );
             ILFree( pidl );
         }
     }
 #endif

     auto GetGetAbsolutePath( std::string_view path ) -> Path {
         Path absolutePath{ std::filesystem::absolute( path ) };
         return absolutePath;
     }

    auto CreateIfNotExistsDirectory( const Path &path ) -> bool {
        std::error_code ec{};
        const bool created{ std::filesystem::create_directories(path.GetPathTyped<std::string>(), ec) };

        if (ec) {
            return false;
        }

        return created;
    }

    auto DisplayPopUp( eastl::string_view title, eastl::string_view message, PopUpChoice choice, PopUpIcon icon, core::i32 timeOut ) -> void {
        constexpr auto kChoiceConvert{
            [](PopUpChoice choice) -> pfd::choice {
                switch ( choice ) {
                    case PopUpChoice::eOk: return pfd::choice::ok;
                    case PopUpChoice::eOkCancel: return pfd::choice::ok_cancel;
                    case PopUpChoice::eYesNo: return pfd::choice::yes_no;
                    case PopUpChoice::eYesNoCancel: return pfd::choice::yes_no_cancel;
                    case PopUpChoice::eRetryCancel: return pfd::choice::retry_cancel;
                    case PopUpChoice::eAbortRetryIgnore: return pfd::choice::abort_retry_ignore;
                }

                return pfd::choice::ok;
            }
        };

         constexpr auto kIconConvert{
             [](PopUpIcon choice) -> pfd::icon {
                 switch ( choice ) {
                     case PopUpIcon::eError: return pfd::icon::error;
                     case PopUpIcon::eInfo: return pfd::icon::info;
                     case PopUpIcon::eQuestion: return pfd::icon::question;
                     case PopUpIcon::eWarning: return pfd::icon::warning;
                 }

                 return pfd::icon::info;
             }
         };

         auto m{ pfd::message(title.data(),
                     message.data(),
                     kChoiceConvert(choice),
                     kIconConvert(icon)) };

         (void)m.ready( timeOut );
    }

    auto SaveFileDialog( eastl::string_view fileName,  std::initializer_list<FileDialogPair> filters ) -> Path {
         std::vector<std::string> filterList{};
         for (const auto& item : filters) {
             filterList.emplace_back( item.mDescription.c_str() );
             filterList.emplace_back( item.mFilePattern.c_str() );
         }

         pfd::save_file f{ pfd::save_file("Choose file to save", pfd::path::home(), filterList, pfd::opt::none) };

         return Path{ f.result() };
    }

    auto OpenFolderDialog() -> eastl::string {
         auto result{ pfd::select_folder( "Select a folder", pfd::path::home() ).result() };
         if ( !result.empty() ) {
             return result.c_str();
         }
         return {};
    }

    auto OpenFileDialog( const FileDialogPair &filter ) -> eastl::string {
         auto result = pfd::open_file( filter.mDescription.c_str(), pfd::path::home() ).result();
         if ( !result.empty() ) {
             return result[0].c_str();
         }
         return {};
    }

    auto OpenFileDialog( std::initializer_list<FileDialogPair> filters ) -> eastl::string {
         std::vector<std::string> filterList{};
         for ( const auto &filter: filters ) {
             filterList.push_back( filter.mDescription.c_str() );
             filterList.push_back( filter.mFilePattern.c_str() );
         }
         auto result{ pfd::open_file( "Select a file", pfd::path::home(), filterList ).result() };
         if ( !result.empty() ) {
             return result[0].c_str();
         }
         return {};
    }

    auto OpenInExplorer( const Path &path ) -> void {
#if defined( MIKOTO_PLATFORM_WINDOWS )
        eastl::wstring widePath{ path.GetPathTyped<eastl::wstring>() };
        if ( std::filesystem::is_regular_file( path.GetPathTyped<std::filesystem::path>() ) ) {
            OpenAndSelectFile( widePath );
        } else {
            ShellExecuteW( nullptr, L"open", widePath.c_str(), nullptr, nullptr, SW_SHOWDEFAULT );
        }
#endif
    }
}