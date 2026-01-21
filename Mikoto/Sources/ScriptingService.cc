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

#include <sol/sol.hpp>

#include <Core/Exception.hh>
#include <Core/Profiler.hh>
#include <Filesystem/FileService.hh>
#include <Logging/Logger.hh>
#include <Scripting/ScriptingService.hh>

#include "Scripting/InputBinding.hh"
#include "Scripting/MathBindings.hh"
#include "Scripting/SceneBinding.hh"
#include "Scripting/SystemBindings.hh"
#include "Filesystem/FileWatcherService.hh"

namespace Mikoto {

    ScriptingService::ScriptingService( const ScriptingServiceDescription & ) {
    }

    auto ScriptingService::Init() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_INFO("Initializing LuaService...");

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
        for (auto& script : m_ScriptPool | std::ranges::views::values) {
            script.As<Script>()->Update( timeStep );
        }
    }

    auto ScriptingService::LoadScript( const Path &path, Entity* entity ) -> ScriptHandle {
        ScriptHandle handle{ ScriptHandle::CreateEmpty() };

        if (const File* file{ FileService::Get()->LoadFile( path ) }) {
            try {
                handle = m_ScriptPool.Allocate( file,  m_LuaState, entity );

                // Mark lambda as mutable to modify "its members"
                (void)FileWatcherService::Get()->Watch( file->GetPath(), [handle, this](const Path& pathCallable, FileWatchEvent event) mutable -> void {
                    if (event == FileWatchEvent::MODIFIED) {
                        handle->ReloadScript(m_LuaState);
                        MKT_CORE_LOGGER_INFO( "File at path {} was modified", pathCallable.string());
                    }
                } );

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