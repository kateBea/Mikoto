//
// Created by zanet on 3/27/2025.
//

#include <Logging/Logger.hh>
#include <Memory/MemoryService.hh>

namespace Mikoto {

    MemoryService::MemoryService( const MemoryServiceCreateInfo &options )
    {}

    auto MemoryService::Init() -> void {
        MKT_CORE_LOGGER_INFO("Initializing MemoryService...");

        m_IsInitialized = true;
    }

    auto MemoryService::Shutdown() -> void {
        if (!m_IsInitialized) {
            return;
        }

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down MemoryService..." );
    }
}// namespace Mikoto