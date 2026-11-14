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

#include <EASTL/algorithm.h>

#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Platform/InputSystem.hh>

#include <Physics/PhysicSystem.hh>

#include <Renderer/Core/RenderSystem.hh>
#include <Renderer/Interface/CanvasInput.hh>

#include <Scene/SceneManager.hh>

#include <Scripting/ScriptingService.hh>

#include <Layers/RuntimeLayer.hh>

namespace mikoto::runtime {
    using namespace core;
    using namespace scene;
    using namespace renderer;
    using namespace renderer::rhi;

    RuntimeLayer::RuntimeLayer( platform::IWindow* window, project::Project& project )
        : mName{ "Game" }, mWindow{ window }, mProject{ &project } {
    }

    auto RuntimeLayer::OnCreate() -> void {
        MKT_BEGIN_PROFILER();

        if ( mProject->GetScenePath().IsEmpty() ) {
            MKT_THROW_RUNTIME_ERROR( "The project has no startup scene. Save a scene in the editor first." );
        }

        mScene = scene::SceneManager::Get()->Load( mProject->GetScenePath() );

        if ( !mScene ) {
            MKT_THROW_RUNTIME_ERROR( "Could not load the project's startup scene." );
        }

        const auto& settings{ mProject->GetSceneRendererSettings() };
        mRenderer = ISceneRenderer::Create()
            .SetName( "GameRenderer" )
            .SetDevice( renderer::RenderSystem::Get()->GetGpuDevice() )
            .SetRenderResolution( settings.mResolution )
            .SetMultisampling( settings.mMultisampling )
            .Build();
        mRenderer->ApplySettings( settings );
        mRenderer->Initialize();
        mRenderer->SetEditorOverlaysEnabled( false );
        mRenderer->SetEnableInfiniteGrid( false );

        // Resolve the initial world transforms and camera before simulation starts.
        mScene->Update( 0.0f );
        UpdateCamera();
        mScene->SetState( scene::SceneState::ePlaying );
        mWindow->SetTitle( mProject->GetName() );
    }

    auto RuntimeLayer::UpdateCamera() -> void {
        MKT_BEGIN_PROFILER();

        scene::CameraComponent* selected{};
        const scene::TransformComponent* transform{};
        core::u64 selectedID{};

        // Multiple Main Cameras are resolved deterministically by entity ID.
        for ( auto [entity, camera, pose, tag]: mScene->GetRegistry().view<scene::CameraComponent, scene::TransformComponent, scene::TagComponent>().each() ) {
            if ( tag.IsActive() && camera.HasCamera() && camera.IsMainCamera() && ( !selected || tag.GetGuid() < selectedID ) ) {
                selected = &camera;
                transform = &pose;
                selectedID = tag.GetGuid();
            }
        }

        if ( !selected ) {
            MKT_THROW_RUNTIME_ERROR( "No active Main Camera in the game scene." );
        }

        static_cast<scene::Camera&>( mCamera ) = selected->GetCamera();

        if ( !selected->IsAspectRatioFixed() ) {
            mCamera.SetViewportSize( mWindow->GetWidth(), mWindow->GetHeight() );
        }

        // A camera inherits its world position and rotation, but not mesh scale.
        const auto& world{ transform->GetWorldTransform() };
        core::float4x4 pose{ 1.0f };

        for ( core::i32 axis{}; axis < 3; ++axis ) {
            const core::float3 direction{ world[axis] };
            const core::f32 length{ glm::length( direction ) };
            pose[axis] = core::float4{ length > 1e-6f ? direction / length : core::float3{ pose[axis] }, 0.0f };
        }

        pose[3] = world[3];
        mCamera.SetRenderPose( pose );

        mRenderer->SetMainCamera( &mCamera );
        mRenderer->SetGamma( selected->GetGamma() );
        mRenderer->SetExposure( selected->GetExposure() );
        mRenderer->SetSkyboxBlur( selected->GetSkyboxBlur() );
        mRenderer->SetClearColor( selected->GetClearColor() );

        renderer::SceneBackgroundType background{ renderer::SceneBackgroundType::eClearColor };

        switch ( selected->GetClearFlags() ) {
            case scene::CameraClearFlags::eSkybox:
                background = renderer::SceneBackgroundType::eSkybox;
                break;
            case scene::CameraClearFlags::eBlurredSkybox:
                background = renderer::SceneBackgroundType::ePrefilterMap;
                break;
            default:
                break;
        }

        mRenderer->SetRenderBackground( background );
    }

    auto RuntimeLayer::OnUpdate( float timeStep ) -> void {
        MKT_BEGIN_PROFILER();

        auto* input{ platform::InputSystem::GetPtr() };
        const auto mouse{ input->GetMousePos() };
        const bool down{ input->IsMouseKeyPressed( platform::MouseButton::Mouse_Button_Left ) };
        const core::float2 uv{
            core::as<core::f32>( mouse.first ) / eastl::max( mWindow->GetWidth(), 1 ),
            core::as<core::f32>( mouse.second ) / eastl::max( mWindow->GetHeight(), 1 )
        };
        const bool inside{ uv.x >= 0.0f && uv.y >= 0.0f && uv.x <= 1.0f && uv.y <= 1.0f };
        scripting::ScriptingService::Get()->SetInputFocus( true, inside );

        renderer::UpdateCanvasInput( *mScene, mCamera, uv, down, down && !mPointerDown, !down && mPointerDown, inside );
        mPointerDown = down;

        // Edge-trigger keyboard activation so holding Space never toggles a
        // control every frame. Pointer and keyboard share the canvas focus.
        const platform::KeyCode keys[]{ platform::KeyCode::Key_Tab, platform::KeyCode::Key_Enter, platform::KeyCode::Key_Space,
            platform::KeyCode::Key_Left, platform::KeyCode::Key_Right, platform::KeyCode::Key_Home, platform::KeyCode::Key_End };
        const renderer::CanvasNavigation actions[]{
            input->IsKeyPressed( platform::KeyCode::Key_Left_Shift ) || input->IsKeyPressed( platform::KeyCode::Key_Right_Shift ) ? renderer::CanvasNavigation::ePrevious : renderer::CanvasNavigation::eNext,
            renderer::CanvasNavigation::eActivate, renderer::CanvasNavigation::eActivate, renderer::CanvasNavigation::eDecrease,
            renderer::CanvasNavigation::eIncrease, renderer::CanvasNavigation::eHome, renderer::CanvasNavigation::eEnd
        };

        for ( core::usize i{}; i < 7; ++i ) {
            const bool keyDown{ input->IsKeyPressed( keys[i] ) };

            if ( keyDown && !mCanvasKeysDown[i] ) {
                renderer::NavigateCanvasInput( *mScene, actions[i] );
            }

            mCanvasKeysDown[i] = keyDown;
        }

        mScene->Update( timeStep );
        UpdateCamera();

        // Render from the pose produced by this frame's scripts and physics.
        mScene->Update( mRenderer.get() );
        mRenderer->Render( mScene );
        renderer::RenderSystem::Get()->SetPresentTarget( mRenderer->GetFinalImage() );
    }

    auto RuntimeLayer::OnRender( float ) -> void {
    }

    auto RuntimeLayer::GetName() const -> eastl::string_view {
        return mName;
    }

    auto RuntimeLayer::OnEvent( platform::Event& ) -> void {
    }

    auto RuntimeLayer::CancelInput() -> void {
        scripting::ScriptingService::Get()->SetInputFocus( false, false );

        if ( mScene ) {
            renderer::CancelCanvasInput( *mScene );
        }

        mPointerDown = false;

        for ( auto& key: mCanvasKeysDown ) {
            key = false;
        }
    }

    auto RuntimeLayer::OnDestroy() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mScene && !mRenderer ) {
            return;
        }

        CancelInput();

        if ( mScene ) {
            mScene->SetState( scene::SceneState::eIdle );
        }

        if ( auto* physics{ physics::PhysicSystem::GetPtr() } ) {
            physics->SetSimulationTarget( nullptr );
        }

        if ( auto* render{ renderer::RenderSystem::GetPtr() } ) {
            if ( auto device{ render->GetGpuDevice() } ) {
                device->WaitIdle();
            }

            render->SetPresentTarget( {} );
        }

        if ( mRenderer ) {
            mRenderer->Shutdown();
            mRenderer.reset();
        }

        if ( mScene ) {
            scene::SceneManager::Get()->Unload( mScene );
            mScene = nullptr;
            scene::SceneManager::Get()->CollectRetiredScenes();
        }
    }
}// namespace mikoto::runtime
