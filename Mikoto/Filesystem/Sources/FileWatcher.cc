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

#include <mutex>
#include <filesystem>

#include <EASTL/utility.h>
#include <EASTL/unique_ptr.h>

#include <Platform/Windows/Windows.hh>

#include <FileWatch.hh>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Logging/Logger.hh>

#include <Filesystem/FileSystem.hh>
#include <Filesystem/FileWatcher.hh>

namespace mikoto::filesystem {

    using namespace mikoto::core;

    MKT_NODISCARD static auto ConverEventType( filewatch::Event action ) -> FileWatchEvent {
        switch ( action ) {
            case filewatch::Event::added:
                return FileWatchEvent::eCreated;
            case filewatch::Event::removed:
                return FileWatchEvent::eDeleted;
            case filewatch::Event::modified:
                return FileWatchEvent::eModified;

            case filewatch::Event::renamed_old:
            case filewatch::Event::renamed_new:
                return FileWatchEvent::eMoved;
            default:
                MKT_CORE_LOGGER_ERROR( "Error undefined backend file watch action!" );
        }

        // Make compiler happy
        return FileWatchEvent::eInvalid;
    }

    FileWatcher::FileWatcher( const Path &path, core::u32 timeOut )
        : mWatchedPath{ path }, mDebounceTime{ timeOut } {

        // Because filewatch::FileWatch is the object that manages path watch
        mWatcher = eastl::make_unique<filewatch::FileWatch<std::string>>(
            mWatchedPath.GetPathTyped<std::string>(), [
                this,
                directory = std::filesystem::is_directory( path.GetPathTyped<std::filesystem::path>() )]
                ( const std::string &changed, const filewatch::Event event ) -> void {
            const auto eventType{ ConverEventType( event ) };

            // TODO: handle multiple triggers of same event in very short intervals
            // Could address this issue using debouncing i.e. ignore same type of event
            // within a set interval (i.e.: cannot handle multiple modified event that happen within 500ms)

            // Registration may append/reallocate while the OS watcher
            // is dispatching. Free callback queue to let other callers
            // register more watches.
            eastl::vector<WatcherCallback> callbacks{};
            {
                std::lock_guard lock{ mCallbacksMutex };
                callbacks = mCallbacks;
            }

            for ( const auto &callback: callbacks ) {
                const Path eventPath{ directory ?
                    mWatchedPath.GetPathTyped<std::filesystem::path>() / std::filesystem::path{ changed } :
                    mWatchedPath.GetPathTyped<std::filesystem::path>() };
                callback( eventPath, eventType );
            }
        } );
    }

    auto FileWatcher::RegisterWatchCallback( WatcherCallback &&callback ) -> void {
        std::lock_guard lock{ mCallbacksMutex };
        mCallbacks.emplace_back( eastl::move( callback ) );
    }
}// namespace mikoto::filesystem
