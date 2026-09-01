//    Copyright 2026 ケイト
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
#include <exception>

#include <EASTL/atomic.h>
#include <EASTL/vector.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <sol/sol.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>
#include <Logging/Logger.hh>

#include <Filesystem/FileSystem.hh>
#include <Filesystem/FileService.hh>
#include <Filesystem/FileWatcherService.hh>

#include <Scene/Entity.hh>

#include <Scripting/InputBinding.hh>
#include <Scripting/MathBindings.hh>
#include <Scripting/SceneBinding.hh>
#include <Scripting/SystemBindings.hh>

#include <Scripting/ScriptingService.hh>

namespace mikoto::scripting {

    using namespace mikoto::core;
    using namespace mikoto::scene;
    using namespace mikoto::filesystem;

    ScriptingService::ScriptingService( const ScriptingServiceDescription& config )
        : mBasePath{ config.mScriptBasePath }
    {}

    auto ScriptingService::Initialize() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_INFO("Initializing ScriptingService...");

        InitState();
        InitBindings();

        mIsInitialized = true;
    }

    auto ScriptingService::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!mIsInitialized) {
            return;
        }

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down ScriptingService..." );
    }

    auto ScriptingService::Update( float timeStep ) -> void {

    }

    auto ScriptingService::LoadScript( const Path &path, Entity* entity ) -> ScriptHandle {
        ScriptHandle handle{};
        if (FileHandle file{ FileService::Get()->LoadFile( path ) }) {
            std::lock_guard lock{ mScriptsMutex };

            try {
                handle = ScriptHandle::New( file,  mLuaState, entity );
                FileWatcherService::Get()->Watch( file->GetPath(),
                [handle, this](const Path& , FileWatchEvent event) mutable -> void {
                    if (event == FileWatchEvent::eModified) {
                        handle->ReloadScript(mLuaState);
                    }
                } );

                mScripts.emplace_back( handle );

            } catch ( const std::exception &e ) {
                MKT_CORE_LOGGER_ERROR( "ScriptingService::LoadScript. File {}. Exception: '{}'", file->GetPath().GetC_Str(), e.what() );
            }
        }

        return handle;
    }

    auto ScriptingService::CreateScript(Entity* entity) -> ScriptHandle {
        // Create a temporary file
        ( void )CreateIfNotExistsDirectory( mBasePath );

        Path scriptPath{ string::Format( "{}/Script.lua", mBasePath.GetPath() ) };

        ScriptHandle handle{};

        if ( FileHandle file{ FileService::Get()->CreateNewFile( scriptPath ) } ) {
            std::lock_guard lock{ mScriptsMutex };

            // This script is an empty template used to create new scripts
            FileHandle scriptBase{ FileService::Get()->LoadFile( "Resources/Script-Examples/base.lua" ) };

            // Set contents by default does not flush
            file->SetContents( scriptBase->GetContentsString().data() );
            file->FlushContents();

            try {
                handle = ScriptHandle::New( file, mLuaState, entity );

                FileWatcherService::Get()->Watch( file->GetPath(),
                [handle, this]( const Path&, FileWatchEvent event ) mutable -> void {
                    if ( event == FileWatchEvent::eModified ) {
                        handle->ReloadScript( mLuaState );
                    }
                } );

                mScripts.emplace_back( handle );

            } catch ( const std::exception& e ) {
                MKT_CORE_LOGGER_ERROR( "ScriptingService::CreateScript. File {}. Exception: '{}'", file->GetPath().GetC_Str(), e.what() );
            }
        }

        return handle;
    }

    auto ScriptingService::InitState() -> void {
        // Open basic libs
        mLuaState.open_libraries(
            sol::lib::base,    // print, etc.
            sol::lib::package, // require
            sol::lib::string,  // string.* functions
            sol::lib::table,   // table.* functions
            sol::lib::math,    // math.* functions
            sol::lib::os       // os.* functions
        );
    }

    auto ScriptingService::InitBindings() -> void {
        mBindings.Register<InputBinding>();
        mBindings.Register<SceneBinding>();
        mBindings.Register<MathBinding>();
        mBindings.Register<SystemBinding>();

        for (auto& binding : mBindings | std::ranges::views::values) {
            binding->Init( mLuaState );
        }
    }
}// namespace Mikoto