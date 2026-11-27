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

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Logging/Logger.hh>

#include <Filesystem/FileSystem.hh>

#if MIKOTO_MODULE_PHYSICS
#include <Physics/PhysicSystem.hh>
#include <Scene/Scene.hh>
#endif

#include <Scripting/ScriptingService.hh>
#include <Scripting/Dotnet/DotnetScriptingService.hh>

namespace mikoto::scripting {
    using namespace core;
    using namespace filesystem;

    ScriptingService::ScriptingService( const ScriptingServiceDescription& config )
        : mBasePath{ config.mScriptBasePath } {
    }

    ScriptingService::~ScriptingService() {
        Shutdown();
    }

    auto ScriptingService::Initialize() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( mIsInitialized ) {
            return;
        }

        MKT_CORE_LOGGER_INFO( "Initializing ScriptingService..." );

        // Keep ownership local until all services initialize successfully.
        mLuaService = eastl::make_unique<LuaScriptingService>();
        mDotnetService = eastl::make_unique<DotnetScriptingService>();
        mNativeService = eastl::make_unique<NativeScriptingService>();

        mLuaService->Initialize();
        mDotnetService->Initialize();
        mNativeService->Initialize();

        mIsInitialized = true;
    }

    auto ScriptingService::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !mIsInitialized ) {
            return;
        }

        MKT_CORE_LOGGER_INFO( "Shutting down ScriptingService..." );

        if ( mNativeService ) {
            mNativeService->Shutdown();
            mNativeService.reset();
        }

        if ( mDotnetService ) {
            mDotnetService->Shutdown();
            mDotnetService.reset();
        }

        if ( mLuaService ) {
            mLuaService->Shutdown();
            mLuaService.reset();
        }

        mIsInitialized = false;
    }

    auto ScriptingService::SetInputFocus( bool focused, bool pointerOver ) -> void {
        mInputFocused = focused;
        mPointerOverInput = focused && pointerOver;
    }

    auto ScriptingService::HasInputFocus() const -> bool {
        return mInputFocused;
    }

    auto ScriptingService::IsPointerOverInput() const -> bool {
        return mPointerOverInput;
    }

    auto ScriptingService::Update( float timeStep ) -> void {
        if ( mLuaService ) {
            mLuaService->Update( timeStep );
        }

        if ( mNativeService ) {
            mNativeService->Update( timeStep );
        }

#if MIKOTO_MODULE_PHYSICS
        if ( auto *physics{ physics::PhysicSystem::GetPtr() } ) {
            if ( auto *scene{ physics->GetSimulationTarget() } ) {
                scene->DispatchPhysicsEvents();
            }
        }
#endif
    }

    auto ScriptingService::CreateScript( ScriptLanguage language, const filesystem::Path& path ) -> bool {
        if ( !mIsInitialized ) {
            return false;
        }

        switch ( language ) {
            case ScriptLanguage::eNative:
                return mNativeService->CreateScript( path );
            case ScriptLanguage::eLua:
                return mLuaService->CreateScript( path );
            case ScriptLanguage::eCSharp:
                return mDotnetService->CreateScript( path );
        }

        return false;
    }

    auto ScriptingService::SetScriptBasePath( const filesystem::Path& path ) -> void {
        mBasePath = path.ToAbsolute();
    }

    auto ScriptingService::GetScriptBasePath() const -> const filesystem::Path& {
        return mBasePath;
    }

    auto ScriptingService::GetScriptDirectory( ScriptLanguage language ) const -> filesystem::Path {
        const char* folder{};

        switch ( language ) {
            case ScriptLanguage::eNative:
                folder = "Native";
                break;
            case ScriptLanguage::eLua:
                folder = "Lua";
                break;
            case ScriptLanguage::eCSharp:
                folder = "CSharp";
                break;
            default:
                return {};
        }

        return Path{ string::Format( "{}/{}", mBasePath.GetPath(), folder ) };
    }

    auto ScriptingService::CreateScript( ScriptLanguage language ) -> filesystem::Path {
        if ( !mIsInitialized ) {
            return {};
        }

        const char* extension{};

        switch ( language ) {
            case ScriptLanguage::eNative:
                extension = "cpp";
                break;
            case ScriptLanguage::eLua:
                extension = "lua";
                break;
            case ScriptLanguage::eCSharp:
                extension = "cs";
                break;
            default:
                return {};
        }

        const auto directory{ GetScriptDirectory( language ) };
        ( void )filesystem::CreateIfNotExistsDirectory( directory );
        Path path{ string::Format( "{}/Script.{}", directory.GetPath(), extension ) };

        // Keep existing sources intact when generating another default script.
        for ( core::u32 suffix{ 1 }; path.Exists(); ++suffix ) {
            path = Path{ string::Format( "{}/Script{}.{}", directory.GetPath(), suffix, extension ) };
        }

        return CreateScript( language, path ) ? path : filesystem::Path{};
    }

    auto ScriptingService::CreateScript( ScriptLanguage language, scene::Entity* entity, bool watch ) -> ScriptHandle {
        if ( !entity ) {
            return {};
        }

        const auto path{ CreateScript( language ) };
        return path.IsEmpty() ? ScriptHandle{} : LoadScript( language, path, entity, watch );
    }

    auto ScriptingService::LoadScript( ScriptLanguage language, const filesystem::Path& path, scene::Entity* entity, bool watch ) -> ScriptHandle {
        if ( !mIsInitialized ) {
            return {};
        }

        switch ( language ) {
            case ScriptLanguage::eNative:
                return mNativeService->LoadScript( path, entity );
            case ScriptLanguage::eLua:
                return mLuaService->LoadScript( path, entity, watch );
            case ScriptLanguage::eCSharp:
                return mDotnetService->LoadScript( path, entity );
        }

        return {};
    }

    auto ScriptingService::GetNativeScriptingService() const -> NativeScriptingService* {
        return mNativeService.get();
    }

    auto ScriptingService::GetLuaScriptingService() const -> LuaScriptingService* {
        return mLuaService.get();
    }

    auto ScriptingService::GetDotnetScriptingService() const -> DotnetScriptingService* {
        return mDotnetService.get();
    }

}// namespace mikoto::scripting
