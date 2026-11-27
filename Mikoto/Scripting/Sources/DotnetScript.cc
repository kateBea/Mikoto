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

#include <EASTL/utility.h>
#include <EASTL/string.h>

#include <Core/Profiler.hh>

#include <Logging/Logger.hh>

#include <Physics/ContactEvents.hh>

#include <Scene/Scene.hh>

#include <Scripting/ScriptingService.hh>
#include <Scripting/Dotnet/DotnetScript.hh>
#include <Scripting/Dotnet/DotnetScriptingService.hh>

namespace mikoto::scripting {

    DotnetScript::DotnetScript( filesystem::FileHandle file, scene::Entity *entity )
        : mFile{ eastl::move( file ) }, mEntity{ entity } {
    }

    DotnetScript::~DotnetScript() {
        Stop();
    }

    auto DotnetScript::Start( scene::Scene &scene ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mScene == &scene || !mEntity || !scene.IsPlaying() ) {
            return;
        }

        Stop();
        auto *scripting{ ScriptingService::GetPtr() };
        auto *service{ scripting ? scripting->GetDotnetScriptingService() : nullptr };

        if ( !service ) {
            return;
        }

        // Register without executing user code. Inactive entities start on their first Update.
        mEntityID = mEntity->GetComponent<scene::TagComponent>().GetGuid();
        mClassName = mEntity->GetComponent<scene::ScriptComponent>().GetClassName();
        mSessionID = service->RegisterScript( scene, *this );

        if ( mSessionID ) {
            mScene = &scene;
            mService = service;
        }
    }

    auto DotnetScript::CreateInstance() -> void {
        MKT_BEGIN_PROFILER();
        const auto path{ mFile->GetPath().GetAbsolute() };
        core::u32 classID{};
        mAppliedProperties = mEntity->GetComponent<scene::ScriptComponent>().GetProperties();
        mCreated = mService->Execute( *mScene,
            { .mOperation = ManagedOperation::eCreate,
                .mScene = mSessionID,
                .mEntity = mEntityID,
                .mAssembly = path.c_str(),
                .mClass = mClassName.c_str(),
                .mClassID = &classID,
                .mValues = mAppliedProperties.c_str() } );

        if ( mCreated ) {
            ( void )mService->mRuntime.GetMetadata( classID, mScriptMetadata );
        }

        mFailed = !mCreated;

        if ( mFailed ) {
            MKT_CORE_LOGGER_ERROR(
                "C#: could not create instance {} from '{}', class '{}'. Reload or restart Play to retry.",
                mEntityID, path.c_str(), mClassName.c_str() );
        }
    }

    auto DotnetScript::Update( float timeStep ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !mService ) {
            return;
        }

        // Coalesce reload requests at a callback boundary, never in the middle of user code.
        mService->ProcessReload( *mScene );

        if ( mFailed ) {
            return;
        }

        if ( !mCreated ) {
            CreateInstance();
        }

        if ( !mCreated ) {
            return;
        }

        const auto &values{ mEntity->GetComponent<scene::ScriptComponent>().GetProperties() };

        if ( values != mAppliedProperties ) {
            if ( !mService->Execute( *mScene, { .mOperation = ManagedOperation::eSetFields,
                .mScene = mSessionID,
                .mEntity = mEntityID,
                .mValues = values.c_str() } ) ) {
                mFailed = true;
                MKT_CORE_LOGGER_ERROR( "C#: fix the script properties and reload to retry instance {}.", mEntityID );
                return;
            }

            mAppliedProperties = values;
        }

        mFailed = !mService->Execute( *mScene,
            { .mOperation = ManagedOperation::eUpdate,
                .mScene = mSessionID,
                .mEntity = mEntityID,
                .mTimeStep = timeStep } );

        if ( mFailed ) {
            MKT_CORE_LOGGER_ERROR( "C#: update disabled for instance {} after an exception. Reload or restart Play to retry.", mEntityID );
        }
    }

    auto DotnetScript::OnPhysicsEvent( const physics::ContactEvent &event, scene::Entity &other ) -> void {
        if ( !mService ) {
            return;
        }
        mService->ProcessReload( *mScene );
        if ( mFailed ) {
            return;
        }
        if ( !mCreated ) {
            CreateInstance();
        }
        if ( !mCreated ) {
            return;
        }

        const auto operation{ static_cast<ManagedOperation>(
            static_cast<core::u32>( ManagedOperation::eCollisionEnter ) + event.GetCallbackIndex() ) };
        mFailed = !mService->Execute( *mScene, { .mOperation = operation,
                                                   .mScene = mSessionID,
                                                   .mEntity = mEntityID,
                                                   .mOtherEntity = other.GetComponent<scene::TagComponent>().GetGuid() } );
        if ( mFailed ) {
            MKT_CORE_LOGGER_ERROR( "C#: {} failed for instance {}. Reload or restart Play to retry.", event.GetCallbackName(), mEntityID );
        }
    }

    auto DotnetScript::Stop() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mService ) {
            return;
        }

        // Also forget definitions staged by reload for currently inactive instances.
        ( void )mService->Execute( *mScene, { .mOperation = ManagedOperation::eDestroy,
            .mScene = mSessionID,
            .mEntity = mEntityID } );
        mService->UnregisterScript( *mScene, *this );

        mService = nullptr;
        mScene = nullptr;
        mSessionID = 0;
        mEntityID = 0;
        mClassName.clear();
        mCreated = false;
        mFailed = false;
    }

    auto DotnetScript::ReloadScript() -> void {
        MKT_BEGIN_PROFILER();

        if ( mService ) {
            mService->RequestReload( *mScene );
        }
        else if ( auto *service{ ScriptingService::Get()->GetDotnetScriptingService() } ) {
            const auto &component{ mEntity->GetComponent<scene::ScriptComponent>() };
            ( void )service->Inspect( mFile->GetPath(), component.GetClassName(), mScriptMetadata );
        }
    }

    auto DotnetScript::RestartInstance() -> void {
        // The managed reload already destroyed the old object. Recreate lazily on Update.
        mCreated = false;
        mFailed = false;
    }

    auto DotnetScript::GetFile() const -> filesystem::FileHandle {
        return mFile;
    }

    auto DotnetScript::GetEntity() const -> scene::Entity * {
        return mEntity;
    }

    auto DotnetScript::GetLanguage() const -> ScriptLanguage {
        return ScriptLanguage::eCSharp;
    }

    auto DotnetScript::GetMetaData() const -> const ScriptClassMetadata & {
        return mScriptMetadata;
    }
}// namespace mikoto::scripting
