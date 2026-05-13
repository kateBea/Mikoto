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

#include <nfd.hpp>

#include <EASTL/memory.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Exception.hh>
#include <Core/Profiler.hh>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

#include <Filesystem/FileSystem.hh>
#include <Filesystem/FileService.hh>
#include <Filesystem/FileWatcherService.hh>

namespace mikoto::filesystem {

    FileService::FileService( const FileServiceCreateInfo& ) {}

    auto FileService::SaveDialog( const eastl::string& defaultName, const std::initializer_list<eastl::pair<eastl::string, eastl::string>>& filters ) -> Path {
        // Use method from mikoto::filesystem instead for file dialogs

        std::string saveFilePath{};

        // Process filters
        std::vector<nfdfilteritem_t> filterItems{};

        for ( const auto& [filterName, filterExtensions]: filters ) {
            filterItems.emplace_back( nfdfilteritem_t{ filterName.data(), filterExtensions.data() } );
        }

        // initialize NFD
        NFD::Guard nfdGuard{};

        // auto-freeing memory
        NFD::UniquePath outPath{};

        // show the dialog
        nfdresult_t result{ NFD::SaveDialog( outPath, filterItems.data(), filterItems.size(), nullptr, defaultName.data() ) };

        if ( result == NFD_OKAY ) {
            saveFilePath = outPath.get();
        } else if ( result == NFD_CANCEL ) {
            MKT_CORE_LOGGER_INFO( "Filesystem::SaveDialog - User canceled File open dialog" );
        } else {
            MKT_CORE_LOGGER_ERROR( "Filesystem::SaveDialog - Error in  File open dialog: {}", NFD::GetError() );
        }

        // NFD::Guard will automatically quit NFD.

        return Path{ saveFilePath.c_str() };
    }

    auto FileService::OpenDialog( const std::initializer_list<eastl::pair<eastl::string, eastl::string>>& filters ) -> Path {
        std::string filePath{};

        // Process filters
        std::vector<nfdfilteritem_t> filterItems{};

        for ( const auto& [filterName, filterExtensions]: filters ) {
            filterItems.emplace_back( nfdfilteritem_t{ filterName.data(), filterExtensions.data() } );
        }

        // initialize NFD
        NFD::Guard nfdGuard{};

        // auto-freeing memory
        NFD::UniquePath outPath{};

        // show the dialog
        nfdresult_t result{ NFD::OpenDialog( outPath, filterItems.data(), filterItems.size() ) };

        if ( result == NFD_OKAY ) {
            filePath = outPath.get();
        } else if ( result == NFD_CANCEL ) {
            MKT_CORE_LOGGER_INFO( "User canceled File open dialog" );
        } else {
            MKT_CORE_LOGGER_ERROR( "Error in  File open dialog: {}", NFD::GetError() );
        }

        // NFD::Guard will automatically quit NFD.

        return Path{ filePath };
    }

    auto FileService::Initialize() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_INFO( "Initializing FileService..." );

        if ( const auto result{ NFD::Init() == NFD_OKAY }; !result ) {
            MKT_THROW_RUNTIME_ERROR( "FileManager - Failed to initialized File dialog library NFD." );
        }

        mIsInitialized = true;
    }

    auto FileService::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !mIsInitialized ) {
            return;
        }

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down FileService..." );

        NFD::Quit();
    }

    auto FileService::LoadFile( const Path& path ) -> FileHandle {
        MKT_BEGIN_PROFILER_NAMED();

        const auto& absolutePath{ path.GetAbsolute() };
        const auto findIt{ mFiles.find( absolutePath ) };

        if ( findIt != mFiles.end() ) {
            return findIt->second;
        }

        if ( !path.Exists() ) {
            return FileHandle::CreateEmpty();
        }

        FileHandle result{};
        try {
            result = FileHandle::Spawn( absolutePath, false );
        } catch ( const std::exception& e ) {
            MKT_CORE_LOGGER_ERROR( "CreateNewFile exception. e.what()", e.what() );
            return FileHandle::CreateEmpty();
        }

        {
            std::lock_guard lock{ mFileLoadMutex };
            const auto [insertIt, success]{
                mFiles.try_emplace( absolutePath, result )
            };

            if ( success ) {
                //If we managed to load the file listen on update notifications to update the file contents
                FileWatcherService::Get()->Watch( result->GetPath(),
                    [result]( const Path& pathCallable, FileWatchEvent event ) mutable -> void {
                    if ( event == FileWatchEvent::eModified ) {
                    result->UpdateContentsFromDisk();
                    MKT_CORE_LOGGER_INFO( "File at path {} was modified. Updating it's contents", pathCallable.GetC_Str() );
                    }
                } );
            }
        }

        return result;
    }

    auto FileService::CreateNewFile( eastl::string_view path ) -> FileHandle {
        return CreateNewFile( Path{ path } );
    }

    auto FileService::CreateNewFile( const Path& path ) -> FileHandle {
        MKT_BEGIN_PROFILER_NAMED();

        const auto& absolutePath{ path.GetAbsolute() };
        const auto findIt{ mFiles.find( absolutePath ) };

        if ( findIt != mFiles.end() ) {
            return findIt->second;
        }

        FileHandle result{};

        try {
            result = FileHandle::Spawn( absolutePath, true );
        } catch ( const std::exception& e ) {
            MKT_CORE_LOGGER_ERROR( "CreateNewFile exception. e.what()", e.what() );
        }

        {
            std::lock_guard lock{ mFileLoadMutex };
            const auto [insertIt, success]{
                mFiles.try_emplace( absolutePath, result )
            };

            if ( success ) {
                //If we managed to load the file listen on update notifications to update the file contents
                FileWatcherService::Get()->Watch( result->GetPath(),
                    [result]( const Path& pathCallable, FileWatchEvent event ) mutable -> void {
                    if ( event == FileWatchEvent::eModified ) {
                    result->UpdateContentsFromDisk();
                    MKT_CORE_LOGGER_INFO( "File at path {} was modified. Updating it's contents", pathCallable.GetC_Str() );
                    }
                } );
            }
        }

        return result;
    }

    auto FileService::GetFile( const Path &path ) -> FileHandle {
        return FileHandle{};
    }

    auto FileService::GetFile( const Path& path ) const -> const FileHandle {
        return FileHandle{};
    }
}// namespace mikoto::filesystem