
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


#include <chrono>
#include <filesystem>

#include <Core/Profiler.hh>
#include <Filesystem/FileWatch.hh>
#include <Filesystem/FileWatcherService.hh>

namespace Mikoto {

    MKT_NODISCARD static auto ConvertEvent( const filewatch::Event change ) -> FileWatchEvent {
        switch (change) {
            case filewatch::Event::added: return FileWatchEvent::CREATED;
            case filewatch::Event::removed: return FileWatchEvent::DELETED;

            case filewatch::Event::modified:
            case filewatch::Event::renamed_old:
            case filewatch::Event::renamed_new: return FileWatchEvent::MODIFIED;
        }

        return FileWatchEvent::UNDEFINED;
    }

    FileWatcherService::FileWatcherService( const FileWatcherServiceCreateInfo& ) {}

    auto FileWatcherService::Watch( const Path &path, FileWatchCallback &&callback ) -> void {
        const std::string strPath{ path.string() };

        m_WatchedPaths[strPath].emplace_back( std::move( callback ) );

        filewatch::FileWatch<std::string> watch(
            strPath,
            [this]( const std::string &path, const filewatch::Event change ) -> void {
                const auto iter{ m_WatchedPaths.find( path ) };

                if (iter != m_WatchedPaths.end()) {
                    auto &watchers{ iter->second };

                    for (const auto &watcher: watchers) {
                        watcher(path, ConvertEvent(change));
                    }
                }
            } );
    }

    auto FileWatcherService::Init() -> void {
        MKT_CORE_LOGGER_INFO( "Initializing FileWatcherService..." );

        m_IsInitialized = true;
    }

    auto FileWatcherService::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!m_IsInitialized) { return; }

        MKT_CORE_LOGGER_INFO( "Shutting down FileWatcherService..." );

        m_WatchedPaths.clear();
    }
}
