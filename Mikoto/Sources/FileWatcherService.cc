//
// Created by kate on 1/17/26.
//

#include <Filesystem/FileWatcherService.hh>
#include <filesystem>
#include <chrono>

#include "Core/Profiler.hh"

namespace Mikoto {
    FileWatcherService::FileWatcherService( const FileWatcherServiceCreateInfo &info ) {
    }

    auto FileWatcherService::Watch( Path path ) -> void {
        if (!m_WatchedPaths.contains( path.string() )) {
            m_WatchedPaths.try_emplace( path.string(), FileMetaData{
                .WatchEvent{ FileWatchEvent::UNDEFINED },
                .LastModifiedTime{ std::filesystem::last_write_time(path) }
            } );
        }
    }

    auto FileWatcherService::CheckStatus( std::string_view path, FileWatchEvent watchEvent ) -> bool {
        const auto it{ m_WatchedPaths.find( std::string{ path } ) };

        if (it != m_WatchedPaths.end()) {
            return it->second.WatchEvent == watchEvent;
        }

        return false;
    }

    auto FileWatcherService::Init() -> void {
        MKT_CORE_LOGGER_INFO("Initializing FileWatcherService...");

        m_IsInitialized = true;
    }

    auto FileWatcherService::Update( float dt ) -> void {
        for (auto& [path, metaData] : m_WatchedPaths) {
            // Reset the event because we do not know the current status
            metaData.WatchEvent = FileWatchEvent::UNDEFINED;

            // Check if modified
            auto directory{ std::filesystem::path{ path } };
            auto lastTime { last_write_time(directory) };

            if (lastTime != metaData.LastModifiedTime) {
                metaData.LastModifiedTime = lastTime;
                MKT_CORE_LOGGER_DEBUG( "Modified file: {}", path );
            }
        }
    }

    auto FileWatcherService::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!m_IsInitialized) {
            return;
        }

        MKT_CORE_LOGGER_INFO( "Shutting down FileWatcherService..." );

        m_WatchedPaths.clear();
    }
}
