//
// Created by zanet on 4/16/2025.
//

#include <Core/Exception.hh>
#include <Core/Profiler.hh>
#include <Filesystem/FileService.hh>
#include <Logging/Logger.hh>
#include <Scripting/ScriptingService.hh>
#include <ranges>
#include <sol/sol.hpp>

#include "Filesystem/FileWatcherService.hh"
#include "Scripting/InputBinding.hh"
#include "Scripting/MathBindings.hh"
#include "Scripting/SceneBinding.hh"
#include "Scripting/SystemBindings.hh"

namespace Mikoto {

    static auto TestCode() -> void {
        sol::state lua;

        // Open basic libs
        lua.open_libraries(
            sol::lib::base,    // print, etc.
            sol::lib::package, // require
            sol::lib::string,  // string.* functions
            sol::lib::table,   // table.* functions
            sol::lib::math,    // math.* functions
            sol::lib::os       // os.* functions
        );

        // Load and execute the Lua script from file
        try {
            const File* file{ FileService::Get()->LoadFile( "./Resources/Script-Examples/hello_world.lua" ) };

            lua.script_file( file->GetPathCStr() );

        } catch ( const sol::error &e ) {
            MKT_THROW_RUNTIME_ERROR( fmt::format( "Lua exception: {}", e.what() ) );
        }
    }


    ScriptingService::ScriptingService( const ScriptingServiceDescription & ) {
    }

    auto ScriptingService::Init() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_INFO("Initializing LuaService...");

        TestCode();

        InitState();

        InitBindings();

        m_IsInitialized = true;
    }
    auto ScriptingService::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!m_IsInitialized) {
            return;
        }

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down LuaService..." );
    }

    auto ScriptingService::Update( const float timeStep ) -> void {
        for (auto& [path, scripts] : m_Scripts) {
            if ( FileWatcherService::Get()->CheckStatus( path, FileWatchEvent::MODIFIED ) ) {
                // Update all scripts using the file in this path
                for (auto& scripts : scripts) {
                    //scripts->Reload();
                }
            }
        }

        for (auto& script : m_ScriptPool | std::ranges::views::values) {
            script.As<Script>()->Update( timeStep );
        }
    }

    auto ScriptingService::LoadScript( const Path &path, Entity* entity ) -> ScriptHandle {
        ScriptHandle handle{ ScriptHandle::CreateEmpty() };

        if (const File* file{ FileService::Get()->LoadFile( path ) }) {
            try {
                handle = m_ScriptPool.Allocate( file,  m_LuaState, entity );

                FileWatcherService::Get()->Watch( file->GetPath() );

            } catch ( const sol::error &e ) {
                MKT_CORE_LOGGER_ERROR( "ScriptingService::LoadScript - exception: '{}'", e.what() );
                MKT_CORE_LOGGER_ERROR( "ScriptingService::LoadScript - Could create script '{}'", file->GetPath() );
            }
        }

        return handle;
    }

    auto ScriptingService::InitState() -> void {
        // Open basic libs
        m_LuaState.open_libraries(
            sol::lib::base,    // print, etc.
            sol::lib::package, // require
            sol::lib::string,  // string.* functions
            sol::lib::table,   // table.* functions
            sol::lib::math,    // math.* functions
            sol::lib::os       // os.* functions
        );
    }

    auto ScriptingService::InitBindings() -> void {
        m_Bindings.Register<InputBinding>();
        m_Bindings.Register<SceneBinding>();
        m_Bindings.Register<MathBinding>();
        m_Bindings.Register<SystemBinding>();

        for (auto& binding : m_Bindings | std::ranges::views::values) {
            binding->Init( m_LuaState );
        }
    }
}// namespace Mikoto