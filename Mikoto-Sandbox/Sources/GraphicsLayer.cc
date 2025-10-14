//
// Created by kate on 10/13/25.
//

#include <Assets/AssetsService.hh>
#include <Core/InputService.hh>
#include <Filesystem/FileService.hh>
#include <GraphicsLayer.hh>
#include <Memory/Allocator.hh>
#include <Renderer/RenderService.hh>
#include <Renderer/RenderUtility.hh>
#include <Scene/Component.hh>

namespace Mikoto {

    GraphicsLayer::GraphicsLayer( std::string_view name, const Window* window  )
        : ILayer{ name }, m_Window { window } {}

    auto GraphicsLayer::OnCreate() -> void {
        MKT_FILE_LOGGER_DEBUG( "Initializing Graphics Layer" );
        // Some example data: a few floats for a vertex buffer
        std::array vertexData{
            0.0f, 0.5f, 0.0f,  // Vertex 1 (x, y, z)
            -0.5f, -0.5f, 0.0f,// Vertex 2
            0.5f, -0.5f, 0.0f  // Vertex 3
        };

        // Describe the buffer
        BufferDescription desc{};
        desc.WithSizeBytes( vertexData.size() * sizeof( vertexData[0] ) )
                .WithData( AsBytes( vertexData.data() ) )
                .WithUsage( BufferUsage::BUFFER_USAGE_VERTEX )
                .WithBufferDataType( BufferDataType::BUFFER_DATA_FLOAT32 )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_STATIC );

        // Create it through the GPU device (Vulkan or otherwise)
        const auto gpuDev{ RenderService::Get()->GetGpuDevice() };
        m_VertexBuffer = gpuDev->CreateBuffer( desc );

        // Allocate staging buffer to copy over the texture data
        BufferDescription stagingDesc{};
        stagingDesc.WithData( nullptr )
                .WithUsage( BufferUsage::BUFFER_USAGE_STAGING )
                .WithSizeBytes( MKT_MEGABYTES( 10 ) )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_STREAM );
        m_StagingBuffer = gpuDev->CreateBuffer( stagingDesc );

        TextureLoadDescription loadDesc{};
        loadDesc
                .WithFile( FileService::Get()->LoadFile( "./texture.png" ) )
                .WithType( TextureType::TEXTURE_2D );

        m_Texture = AssetsService::Get()->LoadAsset<Texture>( loadDesc );

        ModelLoadDescription modelLoadDesc{
            .ModelFile{ FileService::Get()->LoadFile( "./Resources/Models/2 - Cat with scarf/source/Pbr/base.obj" ) },
            .WantTextures{ true }
        };

        m_Model = AssetsService::Get()->LoadAsset<Model>( modelLoadDesc );

        constexpr float NEAR_PLANE{ 0.1f };
        constexpr float FAR_PLANE{ 1000.0f };
        constexpr float FIELD_OF_VIEW{ 45.0f };
        const float ASPECT_RATIO{
            static_cast<float>( m_Window->GetWidth() ) / static_cast<float>( m_Window->GetHeight() )
        };

        m_SceneCamera = CreateScope<SceneCamera>( FIELD_OF_VIEW, ASPECT_RATIO, NEAR_PLANE, FAR_PLANE );
        m_SceneCamera->SetTargetWindow( m_Window );

        SetupRenderer();

        SetupScene();
    }

    auto GraphicsLayer::OnDestroy() -> void {
    }

    auto GraphicsLayer::OnUpdate( float timeStep ) -> void {
        UpdateCamera( timeStep );

        m_Renderer->SetState( SceneState::IDLE );
        m_Renderer->SetScene( m_MainScene.get() );
        m_Renderer->SetCamera( m_SceneCamera.get() );
        m_Renderer->Render( timeStep /*Render target??*/ );
    }

    auto GraphicsLayer::SetupScene() -> void {
        m_MainScene = Scene::Create( "Hello World" );
        m_MainScene->SetName( "Change name just for fun" );

        // This emits sounds
        Entity* entity{ m_MainScene->CreateEntity( "Ball" ) };
        if ( entity ) {
            entity->AddComponent<ScriptComponent>( "hello_world.lua" );
            entity->AddComponent<AudioSourceComponent>( "my_song.mp3" );
        }

        // This can hear sound and has a camera sounds
        // it would make sense as we generally want stuff close to the camera to be heard
        // the further they are from the camera, the less we can hear sources
        Entity* listener{ m_MainScene->CreateEntity( "PlushCat" ) };
        if ( listener ) {
            listener->AddComponent<CameraComponent>();
            listener->AddComponent<ScriptComponent>( "hello_world.lua" );
            listener->AddComponent<AudioListenerComponent>();
        }
    }

    auto GraphicsLayer::SetupRenderer() -> void {
        SceneRendererCreateInfo spec{};
        m_Renderer = SceneRenderer::Create( spec );

        if ( m_Renderer ) {
            m_Renderer->Init();
        }
    }

    auto GraphicsLayer::UpdateCamera( float timeStep ) -> void {

        m_SceneCamera->SetMovementSpeed( 13.f );
        m_SceneCamera->SetRotationSpeed( 13.f );

        m_SceneCamera->SetFarPlane( 20000.0f );
        m_SceneCamera->SetNearPlane( 1.0f );

        m_SceneCamera->WantRotation( true, true );

        m_SceneCamera->SetFieldOfView( 45 );

        // Get viewport dimensions from window
        // we render to the window here not to an imgui viewport
        m_SceneCamera->SetViewportSize( m_Window->GetWidth(), m_Window->GetHeight() );

        if ( InputService::Get()->IsMouseKeyPressed( Mouse_Button_Right ) ) {
            m_SceneCamera->EnableCamera( true );
        } else {
            m_SceneCamera->EnableCamera( false );
        }

        m_SceneCamera->UpdateState( timeStep );
    }

}// namespace Mikoto
