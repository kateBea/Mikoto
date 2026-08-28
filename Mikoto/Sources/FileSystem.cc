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

#include <EASTL/vector.h>
#include <EASTL/fixed_vector.h>

#include <nfd.hpp>

#include <portable-file-dialogs.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Platform.hh>

#include <Logging/Logger.hh>

#include <Filesystem/FileSystem.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )
#include <Platform/PlatformWin32.hh>
#include <shlobj.h>
#include <windows.h>
#endif

namespace mikoto::filesystem {

    using namespace mikoto::core;

#if defined( MIKOTO_PLATFORM_WINDOWS )
    auto OpenAndSelectFile( const eastl::wstring &filePath ) -> void {
        PIDLIST_ABSOLUTE pidl{ ILCreateFromPathW( filePath.c_str() ) };
        if ( pidl ) {
            SHOpenFolderAndSelectItems( pidl, 0, nullptr, 0 );
            ILFree( pidl );
        }
    }
#endif

    auto GetProcessPath() -> Path {
        return Path{ std::filesystem::current_path() };
    }

    auto GetGetAbsolutePath( std::string_view path ) -> Path {
        Path absolutePath{ std::filesystem::absolute( path ) };
        return absolutePath;
    }

    auto CreateIfNotExistsDirectory( const Path &path ) -> bool {
        std::error_code ec{};
        const bool created{ std::filesystem::create_directories( path.GetPathTyped<std::string>(), ec ) };

        if ( ec ) {
            return false;
        }

        return created;
    }

    auto DisplayPopUp( eastl::string_view title, eastl::string_view message, PopUpChoice choice, PopUpIcon icon, core::i32 timeOut ) -> void {
        constexpr auto kChoiceConvert{
            []( PopUpChoice choice ) -> pfd::choice {
                switch ( choice ) {
                    case PopUpChoice::eOk:
                        return pfd::choice::ok;
                    case PopUpChoice::eOkCancel:
                        return pfd::choice::ok_cancel;
                    case PopUpChoice::eYesNo:
                        return pfd::choice::yes_no;
                    case PopUpChoice::eYesNoCancel:
                        return pfd::choice::yes_no_cancel;
                    case PopUpChoice::eRetryCancel:
                        return pfd::choice::retry_cancel;
                    case PopUpChoice::eAbortRetryIgnore:
                        return pfd::choice::abort_retry_ignore;
                }

                return pfd::choice::ok;
            }
        };

        constexpr auto kIconConvert{
            []( PopUpIcon choice ) -> pfd::icon {
                switch ( choice ) {
                    case PopUpIcon::eError:
                        return pfd::icon::error;
                    case PopUpIcon::eInfo:
                        return pfd::icon::info;
                    case PopUpIcon::eQuestion:
                        return pfd::icon::question;
                    case PopUpIcon::eWarning:
                        return pfd::icon::warning;
                }

                return pfd::icon::info;
            }
        };

        auto m{ pfd::message( title.data(),
                              message.data(),
                              kChoiceConvert( choice ),
                              kIconConvert( icon ) ) };

        ( void )m.ready( timeOut );
    }

    auto OpenFolderDialog() -> Path {
        NFD::Guard nfdGuard{};
        NFD::UniquePath outPath{};

        nfdresult_t result{ NFD::PickFolder( outPath ) };
        if ( result == NFD_OKAY ) {
            MKT_CORE_LOGGER_INFO( "Success on save folder dialog: {}", outPath.get() );
        } else if ( result == NFD_CANCEL ) {
            MKT_CORE_LOGGER_INFO( "User canceled open folder dialog." );
        } else {
            MKT_CORE_LOGGER_ERROR( "Error open folder dialog {}", NFD::GetError() );
        }

        return Path{ outPath.get() };
    }

    auto OpenFileDialog( std::initializer_list<FileDialogPair> filters ) -> Path {
        constexpr usize kDefaultFilterLimit{ 10 };
        eastl::fixed_vector<nfdfilteritem_t, kDefaultFilterLimit> filterItems{};
        for ( const auto& [filterName, filterExtensions]: filters ) {
            filterItems.emplace_back( nfdfilteritem_t{ filterName.data(), filterExtensions.data() } );
        }

        NFD::Guard nfdGuard{};
        NFD::UniquePath outPath{};

        const Path currentWorkingDir{ GetProcessPath() };
        const nfdresult_t result{ NFD::OpenDialog( outPath,
            filterItems.data(),
            filterItems.size(),
            currentWorkingDir.GetC_Str() ) };

        if ( result == NFD_OKAY ) {
            MKT_CORE_LOGGER_INFO( "Success on open file dialog: {}", outPath.get() );
        } else if ( result == NFD_CANCEL ) {
            MKT_CORE_LOGGER_INFO( "User canceled open file dialog" );
        } else {
            MKT_CORE_LOGGER_ERROR( "Error open file dialog: {}", NFD::GetError() );
        }

        return Path{ outPath.get() };
    }

    auto SaveFileDialog( eastl::string_view defaultName, std::initializer_list<FileDialogPair> filters ) -> Path {
        constexpr usize kDefaultFilterLimit{ 10 };
        eastl::fixed_vector<nfdfilteritem_t, kDefaultFilterLimit> filterItems{};
        for ( const auto& [filterName, filterExtensions]: filters ) {
            filterItems.emplace_back( nfdfilteritem_t{ filterName.data(), filterExtensions.data() } );
        }

        NFD::Guard nfdGuard{};
        NFD::UniquePath outPath{};

        const nfdresult_t result{ NFD::SaveDialog(
            outPath,
            filterItems.data(),
            filterItems.size(),
            nullptr,
            defaultName.data() ) };

        if ( result == NFD_OKAY ) {
            MKT_CORE_LOGGER_INFO( "Success on save file dialog: {}", outPath.get() );
        } else if ( result == NFD_CANCEL ) {
            MKT_CORE_LOGGER_INFO( "User canceled save file dialog" );
        } else {
            MKT_CORE_LOGGER_ERROR( "Error save file dialog: {}", NFD::GetError() );
        }

        return Path{ outPath.get() };
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
}// namespace mikoto::filesystem