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

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Logging/Logger.hh>

#include <Filesystem/FileService.hh>
#include <Filesystem/FileWatcherService.hh>

namespace mikoto::filesystem {

    using namespace mikoto::core;

    FileService::FileService( const FileServiceCreateInfo& ) {
    }

    auto FileService::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mFileLoadMutex };
        mIsInitialized = true;
    }

    auto FileService::Shutdown() -> void {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mFileLoadMutex };
        mFiles.clear();
        mIsInitialized = false;
    }

    auto FileService::LoadFile( const Path& path ) -> FileHandle {
        MKT_BEGIN_PROFILER();
        return OpenFile( path, false );
    }

    auto FileService::CreateNewFile( eastl::string_view path ) -> FileHandle {
        MKT_BEGIN_PROFILER();
        return CreateNewFile( Path{ path } );
    }

    auto FileService::CreateNewFile( const Path& path ) -> FileHandle {
        MKT_BEGIN_PROFILER();
        return OpenFile( path, true );
    }

    auto FileService::OpenFile( const Path& path, bool create ) -> FileHandle {
        MKT_BEGIN_PROFILER_NAMED();

        if ( path.IsEmpty() ) {
            return FileHandle{};
        }

        try {
            const auto absolutePath{ path.GetAbsolute() };
            // Serialize insertion and first load so two callers cannot publish
            // different File snapshots or register duplicate watches for one key.
            std::lock_guard lock{ mFileLoadMutex };

            if ( const auto found{ mFiles.find( absolutePath ) }; found != mFiles.end() ) {
                found->second->RefreshIfDirty();
                return found->second;
            }

            const auto itInsert{
                mFiles.try_emplace( absolutePath, FileHandle::New( Path{ absolutePath }, create ) )
            };

            if ( itInsert.second ) {
                WatchFile( itInsert.first->second );
            }

            return itInsert.first->second;
        } catch ( const std::exception& error ) {
            MKT_CORE_LOGGER_ERROR( "Cannot {} file '{}': {}", create ? "create" : "load", path.GetC_Str(), error.what() );
            return FileHandle{};
        }
    }

    auto FileService::WatchFile( FileHandle file ) -> void {
        MKT_BEGIN_PROFILER();
        auto* watcher{ FileWatcherService::GetPtr() };

        if ( !watcher || !watcher->IsInitialized() ) {
            return;
        }

        try {
            // Callbacks only invalidate. Reloading on the watcher thread could
            // free a buffer while the renderer or importer is still reading it.
            watcher->Watch( file->GetPath(),
                [dirty = eastl::weak_ptr<eastl::atomic<bool>>{ file->mDiskContentsDirty }]( const Path&, FileWatchEvent event ) {
                if ( event == FileWatchEvent::eModified || event == FileWatchEvent::eCreated ||
                     event == FileWatchEvent::eMoved || event == FileWatchEvent::eDeleted ) {
                    if ( const auto state{ dirty.lock() } ) {
                        state->store( true, eastl::memory_order_relaxed );
                    }
                }
            } );
        } catch ( const std::exception& error ) {
            // A successfully read file remains usable even if OS watching fails.
            MKT_CORE_LOGGER_WARN( "Cannot watch file '{}': {}", file->GetPath().GetC_Str(), error.what() );
        }
    }

    auto FileService::GetFile( const Path& path ) -> FileHandle {
        MKT_BEGIN_PROFILER();
        return static_cast<const FileService&>( *this ).GetFile( path );
    }

    auto FileService::GetFile( const Path& path ) const -> FileHandle {
        MKT_BEGIN_PROFILER();

        if ( path.IsEmpty() ) {
            return FileHandle{};
        }

        try {
            const auto absolutePath{ path.GetAbsolute() };
            std::lock_guard lock{ mFileLoadMutex };

            if ( const auto found{ mFiles.find( absolutePath ) }; found != mFiles.end() ) {
                FileHandle file{ found->second };

                // Give caller new contents if any
                file->RefreshIfDirty();

                return file;
            }
        } catch ( const std::exception& error ) {
            MKT_CORE_LOGGER_ERROR( "Cannot refresh file '{}': {}", path.GetC_Str(), error.what() );
        }

        return FileHandle{};
    }
}// namespace mikoto::filesystem
