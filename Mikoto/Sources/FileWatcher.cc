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
#include <iostream>
#include <filesystem>

#include <FileWatch.hpp>

#include <Logging/Logger.hh>
#include <Core/Exception.hh>
#include <Filesystem/FileWatcher.hh>
#include <Filesystem/FileSystem.hh>

namespace Mikoto {

    MKT_NODISCARD static auto ConverEventType( filewatch::Event action ) -> FileWatchEvent {
        switch (action) {
            case filewatch::Event::added:
                return FileWatchEvent::CREATED;
            case filewatch::Event::removed:
                return FileWatchEvent::DELETED;
            case filewatch::Event::modified:
                return FileWatchEvent::MODIFIED;

            case filewatch::Event::renamed_old:
            case filewatch::Event::renamed_new:
                return FileWatchEvent::MOVED;
            default:
                MKT_CORE_LOGGER_ERROR( "Error undefined backend file watch action!" );
        }

        // Make compiler happy
        return FileWatchEvent::CREATED;
    }

    FileWatcher::FileWatcher( const Path &path ) {
        m_WatchedPathAbsolute = Filesystem::GetGetAbsolutePath( path.string() );
        const std::string absolutePathStr{ m_WatchedPathAbsolute.string() };

        filewatch::FileWatch<std::string> watch(
                absolutePathStr, [this]( const std::string &p, const filewatch::Event event ) -> void {
                    MKT_CORE_LOGGER_ERROR( "MODIFIED" );
                    for (const auto& callback : m_Callbacks) {
                        callback(m_WatchedPathAbsolute, ConverEventType( event ));
                    }
                });
    }

    auto FileWatcher::RegisterWatchCallback( WatcherCallback &&callback ) -> void {
        m_Callbacks.emplace_back( std::move( callback ) );
    }
}