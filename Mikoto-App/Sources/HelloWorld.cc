//
// Created by kate on 11/22/25.
//

#include <Application/HelloWorld.hh>
#include <Core/Root.hh>

#include "Assets/AssetsService.hh"
#include "Core/EventService.hh"
#include "Core/InputService.hh"
#include "Core/TimeService.hh"
#include "Filesystem/FileService.hh"
#include "Physics/PhysicService.hh"
#include "Renderer/RenderService.hh"

auto Mikoto::CreateApplication(int argc, char** argv) -> Application* {
    return new MikotoApp::HelloWorld();
}

namespace MikotoApp {


    auto HelloWorld::Run( Int32 argc, char **argv ) -> Int32 {

        while (IsRunning()) {
            Update();
        }

        return 0;
    }

    auto HelloWorld::Init() -> void {

        // Configure engine orchestrator
        const RootConfig config{
            .LockFrameRate{ false },
            .TargetWindow{ m_Window }
        };

        Root::Init( config );

        // (Optional). Mikoto by default offers layered application
        // If we add layers we must update their state if we do not use layers we can skip this call,
        // but then we would not use this architecture. The layer stack initializes every
        // layer when it is pushed we just need to remember to call OnUpdate and Shutdown on the layer stack

        // =================================================================
        // Renderer
        SceneRendererCreateInfo spec{};
        spec.WithName( "Scene renderer" )
                .WithDevice( RenderService::Get()->GetGpuDevice() );

        m_SceneRenderer = SceneRenderer::Create( spec );

        if ( m_SceneRenderer ) {
            m_SceneRenderer->Init();
        }

        // =================================================================
        // Camera
        constexpr float NEAR_PLANE{ 0.1f };
        constexpr float FAR_PLANE{ 1000.0f };
        constexpr float FIELD_OF_VIEW{ 45.0f };
        const float ASPECT_RATIO{
            static_cast<float>( m_Window->GetWidth() ) / static_cast<float>( m_Window->GetHeight() )
        };

        m_Camera = CreateScope<SceneCamera>( FIELD_OF_VIEW, ASPECT_RATIO, NEAR_PLANE, FAR_PLANE );
        m_Camera->SetTargetWindow( m_Window );

        // =================================================================
        // Scene
        m_ActiveScene = CreateScope<Scene>( "Hello World" );

        ModelLoadDescription descFirst{
            .ModelFile{ FileService::Get()->LoadFile( "./Resources/Models/1 - Box texture/BoxTexture.obj" ) },
            .WantTextures{ true }
        };

        ModelHandle box{ AssetsService::Get()->LoadAsset<Model>( descFirst ) };

        m_ActiveScene = Scene::Create( "Hello World" );
        m_ActiveScene->SetName( "Change name just for fun" );

        // You need to specify the Scene the physics are simulated on
        PhysicService::Get()->SetSimulationScene( m_ActiveScene.get() );

        // This emitting sounds
        EntityCreateInfo groundDesc{
            .Root{ nullptr },
            .Name { "Ground" },
            .Model{ box }
        };
        Entity *ground{ m_ActiveScene->CreateEntity( groundDesc ) };
        if (ground) {
            ground->AddComponent<ScriptComponent>( "./hello_world.lua" );

            TransformComponent& transformComponent{ ground->GetComponent<TransformComponent>() };
            transformComponent.SetScale( { 5.0f, 0.5f, 5.00f } );
            transformComponent.SetTranslation( { 0.0f, 0.0f, 0.0f } );
        }

        // Second ground
        Entity* ground2{ m_ActiveScene->CreateEntity( groundDesc ) };
        if (ground2) {
            ground2->AddComponent<ScriptComponent>( "./Resources/Script-Examples/console_rpg.lua.lua" );

            TransformComponent& transformComponent{ ground2->GetComponent<TransformComponent>() };
            transformComponent.SetScale( { 5.0f, 0.5f, 5.00f } );
            transformComponent.SetTranslation( { -10.0f, 0.0f, 0.0f } );
        }

        Entity* light{ m_ActiveScene->CreateEntity( "Light" ) };
        if (light) {
            light->AddComponent<ScriptComponent>( "./Resources/Script-Examples/console_rpg.lua.lua" );
            LightComponent& lightComp{ light->AddComponent<LightComponent>() };
            lightComp.SetActiveType( LightType::POINT_LIGHT_TYPE );

            auto& pointLightData{ lightComp.Get<PointLight>() };
            pointLightData.SetIntensity( 31.81f );
            pointLightData.SetRadius( 7.44f );

            TransformComponent& transformComponent{ light->GetComponent<TransformComponent>() };
            transformComponent.SetTranslation( { 0.0f, 4.0f, 0.0f } );
        }

        // =================================================================
        // Scene
        AddHandler( EventType::WINDOW_CLOSE_EVENT,
                    [this]( Event &event ) -> bool {
                        m_State = ApplicationStatus::STOPPED;
                        event.SetHandled( true );
                        MKT_CORE_LOGGER_TRACE( "HelloWorld - Handled Window Event close" );
                        return true;
                    } );

        EventService::Get()->Subscribe( this );

    }

    auto HelloWorld::Shutdown() -> void {

        Root::Shutdown();

    }

    auto HelloWorld::Update() -> void {
        // Update time step
        TimeService::Get()->Update();

        const double timeStep{ TimeService::Get()->GetTimeStep( TimeUnit::SECONDS ) };

        // We start the frame
        Root::StartFrame();

        m_ActiveScene->SetState( SceneState::IDLE );

        SetupCamera( timeStep );
        SetupRenderer( timeStep );

        m_SceneRenderer->SetScene( m_ActiveScene.get() );
        m_SceneRenderer->SetCamera( m_Camera.get() );
        m_SceneRenderer->SetViewport( 1920, 1080 );

        m_ActiveScene->Update( timeStep );
        m_SceneRenderer->Render( timeStep );

        // Update engine state
        Root::UpdateState( static_cast<float>( timeStep ) );

        // End the current frame
        Root::EndFrame();
    }

    auto HelloWorld::SetupRenderer( double ) const -> void {
        // Setup renderer
        const Vec4F& color{ 0.2f, 0.3f, 0.5f, 1.0f };
        m_SceneRenderer->SetClearColor( color.r, color.g, color.b, color.a );
    }

    auto HelloWorld::SetupCamera( const double timeStep ) const -> void {

        m_Camera->SetMovementSpeed( 13.f );
        m_Camera->SetRotationSpeed( 13.f );

        m_Camera->SetFarPlane( 20000.0f );
        m_Camera->SetNearPlane( 1.0f );

        m_Camera->WantRotation( true, true );

        m_Camera->SetFieldOfView( 45 );

        // Set viewport to the currently active window we can either expand
        // the final composition to occupy the whole screen or just an ImGui viewport
        m_Camera->SetViewportSize( m_Window->GetWidth(), m_Window->GetHeight() );

        if ( InputService::Get()->IsMouseKeyPressed( Mouse_Button_Right ) ) {
            m_Camera->EnableCamera( true );
        } else {
            m_Camera->EnableCamera( false );
        }

        m_Camera->UpdateState( timeStep );
    }

    auto HelloWorld::SetWindow( Window *window ) -> void {
        m_Window = window;
    }
}// namespace MikotoApp
