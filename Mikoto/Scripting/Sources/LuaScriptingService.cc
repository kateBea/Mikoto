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


#include <EASTL/atomic.h>

#include <Core/Profiler.hh>

#include <Logging/Logger.hh>

#include <Filesystem/FileService.hh>
#include <Filesystem/FileWatcherService.hh>

#include <Scripting/Lua/LuaScript.hh>
#include <Scripting/Lua/LuaBindings.hh>
#include <Scripting/Lua/LuaScriptingService.hh>


namespace mikoto::scripting {
    using namespace core;
    using namespace filesystem;
    using namespace scene;

    struct LuaScriptingService::WatchedScript {
        ScriptHandle mScript{};
        eastl::shared_ptr<eastl::atomic<bool>> mDirty{ eastl::make_shared<eastl::atomic<bool>>( false ) };
        bool mActive{ true };
    };

    LuaScriptingService::LuaScriptingService() = default;

    LuaScriptingService::~LuaScriptingService() {
        Shutdown();
    }

    auto LuaScriptingService::Initialize() -> void {
        if ( mIsInitialized ) {
            return;
        }

        MKT_CORE_LOGGER_INFO( "Initializing LuaScriptingService..." );

        try {
            mLuaState = eastl::make_shared<sol::state>();
            mLuaState->open_libraries( sol::lib::base, sol::lib::package, sol::lib::string,
                sol::lib::table, sol::lib::math, sol::lib::os );
            InitBindings();
            mIsInitialized = true;
        } catch ( ... ) {
            // Initialization has not completed, so Shutdown's guard would skip this rollback.
            mBindings.Clear();
            mLuaState.reset();
            throw;
        }
    }

    auto LuaScriptingService::Shutdown() -> void {
        if ( !mIsInitialized ) {
            return;
        }

        MKT_CORE_LOGGER_INFO( "Shutting down LuaScriptingService..." );

        for ( const auto& slot: mScripts ) {
            slot->mActive = false;
        }

        mScripts.clear();
        mBindings.Clear();
        mLuaState.reset();
        mIsInitialized = false;
    }

    auto LuaScriptingService::Update( float ) -> void {
        // Collect only dirty slots before executing user code: reload may revoke
        // watches, so mScripts cannot be iterated while callbacks are running.
        eastl::vector<eastl::shared_ptr<WatchedScript>> pending{};

        for ( const auto& slot: mScripts ) {
            if ( slot->mActive && slot->mDirty->exchange( false ) ) {
                pending.push_back( slot );
            }
        }

        for ( const auto& slot: pending ) {
            if ( slot->mActive ) {
                slot->mScript->ReloadScript();
            }
        }
    }

    auto LuaScriptingService::LoadScript( const filesystem::Path& path, scene::Entity* entity, bool watch ) -> ScriptHandle {
        MKT_BEGIN_PROFILER();

        if ( !mIsInitialized || !entity || path.IsEmpty() || path.GetNormalizedExtension() != "lua" ) {
            return {};
        }

        try {
            if ( auto file{ filesystem::FileService::Get()->LoadFile( path ) } ) {
                ScriptHandle script{ core::Ref<LuaScript>::New( file, mLuaState, entity ) };

                if ( watch ) {
                    ReleaseEntityScripts( entity );
                    WatchScript( script );
                }

                return script;
            }
        } catch ( const std::exception& error ) {
            MKT_CORE_LOGGER_ERROR( "Could not load Lua script '{}': {}", path.GetC_Str(), error.what() );
        }

        return {};
    }

    auto LuaScriptingService::CreateScript( const filesystem::Path& path ) -> bool {
        if ( !mIsInitialized ) {
            return false;
        }

        try {
            if ( path.IsEmpty() || !path.IsExtension( "lua" ) || path.Exists() ) {
                MKT_CORE_LOGGER_ERROR( "Choose a new .lua file; existing scripts are not overwritten." );
                return false;
            }

            const auto source{ filesystem::FileService::Get()->LoadFile( "Resources/Script-Examples/base.lua" ) };

            if ( !source ) {
                MKT_CORE_LOGGER_ERROR( "Could not load the Lua script template." );
                return false;
            }

            eastl::string name{ path.GetStem() };

            for ( auto& character: name ) {
                if ( !( ( character >= 'a' && character <= 'z' ) || ( character >= 'A' && character <= 'Z' ) ||
                         ( character >= '0' && character <= '9' ) || character == '_' ) ) {
                    character = '_';
                }
            }
            // A prefix makes digit-leading filenames and Lua keywords valid identifiers.
            name.insert( 0, "Script_" );
            auto text{ source->GetContentsString() };
            core::usize offset{};

            while ( ( offset = text.find( "base", offset ) ) != eastl::string::npos ) {
                text.replace( offset, 4, name );
                offset += name.size();
            }

            filesystem::FileHandle file{ filesystem::FileService::Get()->CreateNewFile( path ) };

            if ( file.IsEmpty() ) {
                return false;
            }

            file->Write( text );
            return true;
        } catch ( const std::exception& error ) {
            MKT_CORE_LOGGER_ERROR( "Could not create Lua script: {}", error.what() );
            return false;
        }
    }

    auto LuaScriptingService::WatchScript( ScriptHandle handle ) -> void {
        auto slot{ eastl::make_shared<WatchedScript>() };
        slot->mScript = eastl::move( handle );
        filesystem::FileWatcherService::Get()->Watch( slot->mScript->GetFile()->GetPath(),
            [weak = eastl::weak_ptr<eastl::atomic<bool>>{ slot->mDirty }]( const filesystem::Path&, filesystem::FileWatchEvent event ) {
            if ( event == filesystem::FileWatchEvent::eModified ) {
                if ( auto active{ weak.lock() } ) {
                    active->store( true );
                }
            }
        } );

        mScripts.push_back( eastl::move( slot ) );
    }

    auto LuaScriptingService::ReleaseEntityScripts( scene::Entity* entity ) -> void {
        for ( auto it{ mScripts.begin() }; it != mScripts.end(); ) {
            if ( ( *it )->mScript->GetEntity() == entity ) {
                ( *it )->mActive = false;
                it = mScripts.erase( it );
            }
            else {
                ++it;
            }
        }
    }

    auto LuaScriptingService::InitBindings() -> void {
        MKT_BEGIN_PROFILER();

        mBindings.Register<InputBinding>();
        mBindings.Register<SceneBinding>();
        mBindings.Register<MathBinding>();
        mBindings.Register<SystemBinding>();

        for ( auto& [type, binding]: mBindings ) {
            binding->Init( *mLuaState );
        }
    }
}// namespace mikoto::scripting
