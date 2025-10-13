//
// Created by zanet on 4/16/2025.
//

#include "Scripting/ScriptingService.hh"

#include <Filesystem/FileService.hh>
#include <Logging/Logger.hh>

namespace Mikoto {
    static auto TestCode() -> void {
        sol::state lua;
        lua.open_libraries( sol::lib::base );// open basic Lua libraries

        // Load and execute the Lua script from file
        try {
            const File* file{ FileService::Get()->LoadFile( "./Resources/Script-Examples/hello_world.lua" ) };

            lua.script_file( file->GetPathCStr() );

        } catch ( const sol::error &e ) {
            MKT_THROW_RUNTIME_ERROR( fmt::format( "Lua exception: {}", e.what() ) );
        }
    }

    ScriptingService::ScriptingService( const ScriptingServiceDescription &config ) {
    }

    auto ScriptingService::Init() -> void {
        MKT_CORE_LOGGER_INFO("Initializing LuaService...");

        TestCode();

        m_IsInitialized = true;
    }
    auto ScriptingService::Shutdown() -> void {
        if (!m_IsInitialized) {
            return;
        }

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down LuaService..." );
    }
}// namespace Mikoto