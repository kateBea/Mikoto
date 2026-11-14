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


#include <glm/gtx/compatibility.hpp>

#include <Core/Profiler.hh>

#include <Logging/Logger.hh>

#include <Filesystem/FileService.hh>

#include <Platform/InputSystem.hh>

#include <Assets/AssetRegistryService.hh>

#include <Scene/Scene.hh>

#include <Scripting/ScriptingService.hh>
#include <Scripting/Dotnet/DotnetBindings.hh>
#include <Scripting/Dotnet/DotnetScriptingService.hh>


namespace mikoto::scripting {
    using namespace core;

    // Managed workers and finalizers must never inherit access to the ECS.
    static thread_local scene::Scene* sCallbackScene{};
    static thread_local u64 sCallbackSession{};

    static auto GetCallbackEntity( u64 session, u64 id ) -> scene::Entity* {
        return sCallbackScene && session == sCallbackSession ? sCallbackScene->FindByID( id ) : nullptr;
    }

    static auto GetActiveLight( scene::LightComponent& component ) -> renderer::LightObject* {
        switch ( component.GetActiveType() ) {
            case renderer::LightType::eDirectional:
                return &component.Get<renderer::DirectionalLight>();
            case renderer::LightType::ePoint:
                return &component.Get<renderer::PointLight>();
            case renderer::LightType::eSpot:
                return &component.Get<renderer::SpotLight>();
            case renderer::LightType::eAreaLight:
                return &component.Get<renderer::AreaLight>();
            default:
                return nullptr;
        }
    }


    DotnetSceneBinding::CallbackScope::CallbackScope( scene::Scene& scene, u64 session )
        : mPreviousScene{ sCallbackScene }, mPreviousSession{ sCallbackSession } {
        sCallbackScene = &scene;
        sCallbackSession = session;
    }

    DotnetSceneBinding::CallbackScope::~CallbackScope() {
        sCallbackScene = mPreviousScene;
        sCallbackSession = mPreviousSession;
    }

    auto DotnetSceneBinding::HasActiveCallback() -> bool {
        return sCallbackScene != nullptr;
    }

    auto DotnetSceneBinding::Init( ManagedNativeApi& api ) -> void {
        MKT_BEGIN_PROFILER();
        api.mGetTransform = &GetTransform;
        api.mSetTransform = &SetTransform;
        api.mPlayAudio = &PlayAudio;
        api.mHasComponent = &HasComponent;

        api.mGetLightIntensity = &GetLightIntensity;
        api.mSetLightIntensity = &SetLightIntensity;

        api.mFindCanvasElement = &FindCanvasElement;
        api.mGetCanvasValue = &GetCanvasValue;
        api.mSetCanvasValue = &SetCanvasValue;

        api.mFindEntity = &FindEntity;
        api.mSetVehicleInputs = &SetVehicleInputs;
        api.mGetVehicleSpeed = &GetVehicleSpeed;

        api.mSetCanvasText = &SetCanvasText;
        api.mSetCharacterInputs = &SetCharacterInputs;
        api.mGetCharacterGrounded = &GetCharacterGrounded;
    }

    auto DotnetInputBinding::Init( ManagedNativeApi& api ) -> void {
        MKT_BEGIN_PROFILER();
        api.mIsKeyPressed = &IsKeyPressed;
        api.mIsMouseButtonPressed = &IsMouseButtonPressed;
        api.mGetMousePosition = &GetMousePosition;
        api.mIsInputFocused = &IsInputFocused;
        api.mIsPointerOverInput = &IsPointerOverInput;
        api.mGetMouseScrollDelta = &GetMouseScrollDelta;
    }

    auto DotnetSystemBinding::Init( ManagedNativeApi& api ) -> void {
        MKT_BEGIN_PROFILER();
        api.mLog = &Log;
        api.mReportCompilation = &ReportCompilation;
    }


    auto DotnetSceneBinding::HasComponent( u64 session, u64 id, ManagedComponentType type ) -> i32 {
        try {
            auto* entity{ GetCallbackEntity( session, id ) };

            if ( !entity ) {
                return 0;
            }

            switch ( type ) {
                case ManagedComponentType::eTransform:
                    return entity->HasComponent<scene::TransformComponent>() ? 1 : 0;
                case ManagedComponentType::eLight:
                    return entity->HasComponent<scene::LightComponent>() ? 1 : 0;
                case ManagedComponentType::eCanvas:
                    return entity->HasComponent<scene::CanvasComponent>() ? 1 : 0;
                case ManagedComponentType::eVehicle:
                    return entity->HasComponent<scene::VehicleComponent>() ? 1 : 0;
                case ManagedComponentType::eCharacter:
                    return entity->HasComponent<scene::CharacterControllerComponent>() ? 1 : 0;
                default:
                    return 0;
            }
        } catch ( ... ) {
            return 0;
        }
    }

    auto DotnetSceneBinding::FindEntity( u64 session, u64 owner, cstr name, u64* output ) -> i32 {
        try {
            if ( !GetCallbackEntity( session, owner ) || !name || !*name || !output ) {
                return 0;
            }

            *output = 0;

            for ( auto [handle, tag]: sCallbackScene->GetRegistry().view<scene::TagComponent>().each() ) {
                if ( tag.GetTag() == name ) {
                    // Ambiguous names are rejected rather than selecting an arbitrary entity.
                    if ( *output ) {
                        return 0;
                    }

                    *output = tag.GetGuid();
                }
            }

            return 1;
        } catch ( ... ) {
            return 0;
        }
    }

    auto DotnetSceneBinding::SetVehicleInputs( u64 session, u64 id, f32 throttle, f32 steer, f32 brake, f32 handBrake ) -> i32 {
        try {
            auto* entity{ GetCallbackEntity( session, id ) };

            if ( !entity || !entity->HasComponent<scene::VehicleComponent>() ) {
                return 0;
            }

            auto& vehicle{ entity->GetComponent<scene::VehicleComponent>() };
            vehicle.mThrottle = glm::clamp( throttle, -1.0f, 1.0f );
            vehicle.mSteer = glm::clamp( steer, -1.0f, 1.0f );
            vehicle.mBrake = glm::clamp( brake, 0.0f, 1.0f );
            vehicle.mHandBrake = glm::clamp( handBrake, 0.0f, 1.0f );
            return 1;
        } catch ( ... ) {
            return 0;
        }
    }

    auto DotnetSceneBinding::SetCharacterInputs( u64 session, u64 id, const ManagedVector3* velocity, f32 jumpSpeed ) -> i32 {
        try {
            auto* entity{ GetCallbackEntity( session, id ) };

            if ( !entity || !velocity || !entity->HasComponent<scene::CharacterControllerComponent>() ) {
                return 0;
            }

            auto& character{ entity->GetComponent<scene::CharacterControllerComponent>() };
            character.mDesiredVelocity = { velocity->mX, velocity->mY, velocity->mZ };
            character.mJumpSpeed = glm::max( 0.0f, jumpSpeed );
            return 1;
        } catch ( ... ) {
            return 0;
        }
    }

    auto DotnetSceneBinding::GetCharacterGrounded( u64 session, u64 id ) -> i32 {
        try {
            auto* entity{ GetCallbackEntity( session, id ) };

            if ( !entity || !entity->HasComponent<scene::CharacterControllerComponent>() ) {
                return -1;
            }

            return entity->GetComponent<scene::CharacterControllerComponent>().mGrounded ? 1 : 0;
        } catch ( ... ) {
            return -1;
        }
    }

    auto DotnetSceneBinding::GetVehicleSpeed( u64 session, u64 id, f32* output ) -> i32 {
        try {
            auto* entity{ GetCallbackEntity( session, id ) };

            if ( !entity || !output || !entity->HasComponent<scene::VehicleComponent>() ||
                 !entity->HasComponent<scene::RigidBodyComponent>() ) {
                return 0;
            }

            const auto& transform{ entity->GetComponent<scene::TransformComponent>() };
            const float3 forward{ transform.GetWorldTransform()[2] };
            const float length{ glm::length( forward ) };

            if ( length < 1e-6f ) {
                return 0;
            }

            *output = glm::dot( entity->GetComponent<scene::RigidBodyComponent>().GetLinearVelocity(), forward / length );
            return 1;
        } catch ( ... ) {
            return 0;
        }
    }

    auto DotnetSceneBinding::SetCanvasText( u64 session, u64 id, u64 element, cstr text ) -> i32 {
        try {
            auto* entity{ GetCallbackEntity( session, id ) };

            if ( !entity || !text || !entity->HasComponent<scene::CanvasComponent>() ) {
                return 0;
            }

            auto* command{ entity->GetComponent<scene::CanvasComponent>().GetCanvas().FindElement( element ) };

            if ( !command ) {
                return 0;
            }

            command->mText = text;
            return 1;
        } catch ( ... ) {
            return 0;
        }
    }

    auto DotnetSceneBinding::GetLightIntensity( u64 session, u64 id, f32* output ) -> i32 {
        try {
            auto* entity{ GetCallbackEntity( session, id ) };

            if ( !entity || !output || !entity->HasComponent<scene::LightComponent>() ) {
                return 0;
            }

            const auto* light{ GetActiveLight( entity->GetComponent<scene::LightComponent>() ) };

            if ( !light ) {
                return 0;
            }

            *output = light->GetIntensity();
            return 1;
        } catch ( ... ) {
            return 0;
        }
    }

    auto DotnetSceneBinding::SetLightIntensity( u64 session, u64 id, f32 value ) -> i32 {
        try {
            auto* entity{ GetCallbackEntity( session, id ) };

            if ( !entity || value < 0.0f || !entity->HasComponent<scene::LightComponent>() ) {
                return 0;
            }

            auto* light{ GetActiveLight( entity->GetComponent<scene::LightComponent>() ) };

            if ( !light ) {
                return 0;
            }

            light->SetIntensity( value );
            return 1;
        } catch ( ... ) {
            return 0;
        }
    }

    auto DotnetSceneBinding::FindCanvasElement( u64 session, u64 id, cstr name, u64* output ) -> i32 {
        try {
            auto* entity{ GetCallbackEntity( session, id ) };

            if ( !entity || !name || !*name || !output || !entity->HasComponent<scene::CanvasComponent>() ) {
                return 0;
            }

            *output = entity->GetComponent<scene::CanvasComponent>().GetCanvas().FindElement( eastl::string_view{ name } );
            return 1;
        } catch ( ... ) {
            return 0;
        }
    }

    auto DotnetSceneBinding::GetCanvasValue( u64 session, u64 id, u64 element, f32* output ) -> i32 {
        try {
            auto* entity{ GetCallbackEntity( session, id ) };

            if ( !entity || !output || !entity->HasComponent<scene::CanvasComponent>() ) {
                return 0;
            }

            const auto* command{ entity->GetComponent<scene::CanvasComponent>().GetCanvas().FindElement( element ) };

            if ( !command ) {
                return 0;
            }

            *output = command->mValue;
            return 1;
        } catch ( ... ) {
            return 0;
        }
    }

    auto DotnetSceneBinding::SetCanvasValue( u64 session, u64 id, u64 element, f32 value ) -> i32 {
        try {
            auto* entity{ GetCallbackEntity( session, id ) };

            if ( !entity || !entity->HasComponent<scene::CanvasComponent>() ) {
                return 0;
            }

            auto& canvas{ entity->GetComponent<scene::CanvasComponent>().GetCanvas() };

            if ( !canvas.FindElement( element ) ) {
                return 0;
            }

            canvas.SetValue( element, value );
            return 1;
        } catch ( ... ) {
            return 0;
        }
    }

    auto DotnetSceneBinding::PlayAudio( u64 session, u64 id, cstr path ) -> i32 {
        try {
            auto* entity{ sCallbackScene && session == sCallbackSession ? sCallbackScene->FindByID( id ) : nullptr };

            if ( !entity || !path || !*path || !entity->HasComponent<scene::AudioSourceComponent>() ) {
                return 0;
            }

            auto& component{ entity->GetComponent<scene::AudioSourceComponent>() };
            auto file{ filesystem::FileService::Get()->LoadFile( filesystem::Path{ path } ) };

            if ( !file ) {
                return 0;
            }

            auto clip{ asset::AssetRegistryService::Get()->LoadAsset<audio::Audio>( asset::AudioLoadDescription{ .mFile = file } ) };

            if ( !clip ) {
                return 0;
            }

            component.SetClip( clip );
            auto source{ component.GetSource() };

            if ( !source ) {
                return 0;
            }

            source->Play();
            return 1;
        } catch ( ... ) {
            return 0;
        }
    }

    auto DotnetSceneBinding::GetTransform( u64 session, u64 id, i32 kind, ManagedVector3* output ) -> i32 {
        try {
            auto* entity{ sCallbackScene && session == sCallbackSession ? sCallbackScene->FindByID( id ) : nullptr };

            if ( !entity || !entity->HasComponent<scene::TransformComponent>() || !output || kind < 0 || kind > 2 ) {
                return 0;
            }

            const auto& transform{ entity->GetComponent<scene::TransformComponent>() };
            const float3 value{ kind == 0 ? transform.GetTranslation() : kind == 1 ? transform.GetRotation()
                                                                                   : transform.GetScale() };
            *output = { value.x, value.y, value.z };
            return 1;
        } catch ( ... ) {
            return 0;
        }
    }

    auto DotnetSceneBinding::SetTransform( u64 session, u64 id, i32 kind, const ManagedVector3* input ) -> i32 {
        try {
            auto* entity{ sCallbackScene && session == sCallbackSession ? sCallbackScene->FindByID( id ) : nullptr };

            if ( !entity || !entity->HasComponent<scene::TransformComponent>() || !input || kind < 0 || kind > 2 ) {
                return 0;
            }

            auto& transform{ entity->GetComponent<scene::TransformComponent>() };
            const float3 value{ input->mX, input->mY, input->mZ };

            if ( kind == 0 ) {
                transform.SetTranslation( value );
            }
            else if ( kind == 1 ) {
                transform.SetRotation( value );
            }
            else {
                transform.SetScale( value );
            }

            return 1;
        } catch ( ... ) {
            return 0;
        }
    }

    auto DotnetInputBinding::IsKeyPressed( i32 key ) -> i32 {
        try {
            auto* input{ platform::InputSystem::GetPtr() };
            return IsInputFocused() && input && key >= 32 && key <= 348 && input->IsKeyPressed( static_cast<platform::KeyCode>( key ) ) ? 1 : 0;
        } catch ( ... ) {
            return 0;
        }
    }

    auto DotnetInputBinding::IsInputFocused() -> i32 {
        const auto* scripting{ ScriptingService::GetPtr() };
        const auto* input{ platform::InputSystem::GetPtr() };
        return DotnetSceneBinding::HasActiveCallback() && scripting && scripting->HasInputFocus() && input && input->HasFocus() ? 1 : 0;
    }

    auto DotnetInputBinding::IsPointerOverInput() -> i32 {
        const auto* scripting{ ScriptingService::GetPtr() };
        return IsInputFocused() && scripting->IsPointerOverInput() ? 1 : 0;
    }

    auto DotnetInputBinding::IsMouseButtonPressed( i32 button ) -> i32 {
        try {
            const auto* input{ platform::InputSystem::GetPtr() };
            return IsInputFocused() && input && button >= 0 && button <= 7 &&
                           input->IsMouseKeyPressed( static_cast<platform::MouseButton>( button ) )
                       ? 1
                       : 0;
        } catch ( ... ) {
            return 0;
        }
    }

    auto DotnetInputBinding::GetMousePosition( f32* x, f32* y ) -> i32 {
        try {
            const auto* input{ platform::InputSystem::GetPtr() };

            if ( !x || !y || !IsInputFocused() || !input ) {
                return 0;
            }

            const auto position{ input->GetMousePos() };
            *x = as<f32>( position.first );
            *y = as<f32>( position.second );
            return 1;
        } catch ( ... ) {
            return 0;
        }
    }

    auto DotnetInputBinding::GetMouseScrollDelta( f32* x, f32* y ) -> i32 {
        const auto* input{ platform::InputSystem::GetPtr() };

        if ( !x || !y || !IsPointerOverInput() || !input ) {
            return 0;
        }

        const auto delta{ input->GetMouseScrollDelta() };
        *x = delta.first;
        *y = delta.second;
        return 1;
    }

    auto DotnetSystemBinding::Log( i32 severity, cstr message ) -> void {
        // No C++ exception may unwind into CoreCLR. Managed strings are
        // temporary UTF-8 buffers, copied synchronously by the logger.
        try {
            const eastl::string text{ message ? message : "" };

            if ( severity == 2 ) {
                MKT_CORE_LOGGER_ERROR( "C#: {}", text );
            }
            else if ( severity == 1 ) {
                MKT_CORE_LOGGER_WARN( "C#: {}", text );
            }
            else if ( severity == 3 ) {
                MKT_CORE_LOGGER_DEBUG( "C#: {}", text );
            }
            else {
                MKT_CORE_LOGGER_INFO( "C#: {}", text );
            }
        } catch ( ... ) {
        }
    }

    auto DotnetSystemBinding::ReportCompilation( const ManagedCompilationReport* report ) -> void {
        try {
            if ( report ) {
                DotnetCompilerStatus::Report( *report );
            }
        } catch ( ... ) {
            // Diagnostics must never unwind into managed code.
        }
    }

}// namespace mikoto::scripting
