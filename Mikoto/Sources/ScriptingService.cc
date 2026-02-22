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
#include <atomic>
#include <exception>

#include <sol/sol.hpp>

#include <Common/String.hh>
#include <Core/Exception.hh>
#include <Core/Profiler.hh>
#include <Logging/Logger.hh>

#include <Filesystem/FileService.hh>
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

                FileWatcherService::Get()->Watch( file->GetPath(), 
                [handle, this](const Path& pathCallable, FileWatchEvent event) mutable -> void {
                    if (event == FileWatchEvent::MODIFIED) {
                        handle->ReloadScript(m_LuaState);
                    }
                } );

            } catch ( const std::exception &e ) {
                MKT_CORE_LOGGER_ERROR( "ScriptingService::LoadScript. File {}. Exception: '{}'", file->GetPath(), e.what() );
            }
        }

        return handle;
    }

    auto ScriptingService::CreateScript(Entity* entity) -> ScriptHandle {
        std::atomic_uint64_t scriptCount{};

        std::string scriptName{ StringUtil::Format( "{}/Script.lua", m_ScriptsDirectory.string() ) };
        if ( std::filesystem::exists( scriptName ) ) {
            scriptName = StringUtil::Format( "{}/Script-{}.lua", m_ScriptsDirectory.string(), scriptCount.load() );
            ++scriptCount;
        }
        
        ScriptHandle handle{ ScriptHandle::CreateEmpty() };

        if ( File * file{ FileService::Get()->CreateNewFile( scriptName ) } ) {
            const File* scriptBase{ FileService::Get()->LoadFile( "Resources/Script-Examples/base.lua" ) };

            file->SetContents( scriptBase->GetFileContents().c_str() );
            file->FlushContents();

            try {
                handle = m_ScriptPool.Allocate( file, m_LuaState, entity );

                FileWatcherService::Get()->Watch( file->GetPath(),
                [handle, this]( const Path& pathCallable, FileWatchEvent event ) mutable -> void {
                    if ( event == FileWatchEvent::MODIFIED ) {
                        handle->ReloadScript( m_LuaState );
                    }
                } );

            } catch ( const std::exception& e ) {
                MKT_CORE_LOGGER_ERROR( "ScriptingService::CreateScript. File {}. Exception: '{}'", file->GetPath(), e.what() );
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