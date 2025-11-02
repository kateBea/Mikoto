//
// Created by zanet on 3/27/2025.
//

#include <Core/Profiler.hh>
#include <Logging/Logger.hh>
#include <Memory/MemoryService.hh>

namespace Mikoto {

    MemoryService::MemoryService( const MemoryServiceCreateInfo& )
    {}

    auto MemoryService::Init() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_INFO("Initializing MemoryService...");

        m_IsInitialized = true;
    }

    auto MemoryService::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!m_IsInitialized) {
            return;
        }

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down MemoryService..." );

        m_IsInitialized = false;
    }
}// namespace Mikoto