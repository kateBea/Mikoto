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

#include <ranges>
#include <filesystem>

#include <EASTL/unique_ptr.h>

#include <FileWatch.hh>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Exception.hh>

#include <Filesystem/FileSystem.hh>
#include <Filesystem/FileWatcher.hh>
#include <Logging/Logger.hh>

namespace mikoto::filesystem {

    using namespace mikoto::core;

    MKT_NODISCARD static auto ConverEventType( filewatch::Event action ) -> FileWatchEvent {
        switch (action) {
            case filewatch::Event::added: return FileWatchEvent::eCreated;
            case filewatch::Event::removed: return FileWatchEvent::eDeleted;
            case filewatch::Event::modified: return FileWatchEvent::eModified;

            case filewatch::Event::renamed_old:
            case filewatch::Event::renamed_new:
                return FileWatchEvent::eMoved;
            default:
                MKT_CORE_LOGGER_ERROR( "Error undefined backend file watch action!" );
        }

        // Make compiler happy
        return FileWatchEvent::eInvalid;
    }

    FileWatcher::FileWatcher( const Path &path, u32 timeOut )
        : mWatchedPath{ path }, mDebounceTime{ timeOut }
    {
        mWatcher = eastl::make_unique<filewatch::FileWatch<std::string>>(
                mWatchedPath.GetPathTyped<std::string>(), [this]( const std::string &, const filewatch::Event event ) -> void {
                    const auto eventType{ ConverEventType( event ) };

                    // TODO: handle multiple triggers of same event in very short intervals
                    // Could address this issue using debouncing i.e. ignore same type of event
                    // within a set interval (i.e.: cannot handle multiple modified event that happen within 500ms)

                    // Run handlers
                    for (const auto& callback : mCallbacks) {
                        callback(mWatchedPath, eventType);
                    }
                });
    }

    auto FileWatcher::RegisterWatchCallback( WatcherCallback &&callback ) -> void {
        mCallbacks.emplace_back( std::move( callback ) );
    }
}