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

    static auto TestCode() -> void {
        BufferHandle vertexBuffer{};
        BufferHandle stagingBuffer{};
        TextureHandle texture{};

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
        vertexBuffer = gpuDev->CreateBuffer( desc );

        // Allocate staging buffer to copy over the texture data
        BufferDescription stagingDesc{};
        stagingDesc.WithData( nullptr )
                .WithUsage( BufferUsage::BUFFER_USAGE_STAGING )
                .WithSizeBytes( MKT_MEGABYTES( 10 ) )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_STREAM );
        stagingBuffer = gpuDev->CreateBuffer( stagingDesc );

        TextureLoadDescription loadDesc{};
        loadDesc
                .WithFile( FileService::Get()->LoadFile( "./texture.png" ) )
                .WithType( TextureType::TEXTURE_2D );

        texture = AssetsService::Get()->LoadAsset<Texture>( loadDesc );
    }

    GraphicsLayer::GraphicsLayer( std::string_view name, const Window* window  )
        : ILayer{ name }, m_Window { window } {}

    auto GraphicsLayer::OnCreate() -> void {
        MKT_FILE_LOGGER_DEBUG( "Initializing Graphics Layer" );
        TestCode();

        LoadModels();

        SetupCamera();

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

    auto GraphicsLayer::LoadModels() -> void {
        ModelLoadDescription descFirst{
            .ModelFile{ FileService::Get()->LoadFile( "./Resources/Models/3 - Dachniy house/source/Dachniy_Domik/D_House.FBX" ) },
            .WantTextures{ true }
        };

        m_ModelMultipleMeshes = AssetsService::Get()->LoadAsset<Model>( descFirst );

        ModelLoadDescription descSecond{
            .ModelFile{ FileService::Get()->LoadFile( "./Resources/Models/1 - Box texture/BoxTexture.obj" ) },
            .WantTextures{ true }
        };

        m_ModelSingleMesh = AssetsService::Get()->LoadAsset<Model>( descSecond );
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

        // Load a model with multiples mesh nodes for testing
        Entity* multipleNodes{ m_MainScene->CreateEntity( EntityCreateInfo{
            .Root{ entity },
            .Name{ "Npc" },
            .Model{ m_ModelMultipleMeshes },
        } ) };

        if ( entity ) {
            multipleNodes->AddComponent<ScriptComponent>( "idle.lua" );
            multipleNodes->AddComponent<AudioSourceComponent>( "quack.mp3" );
        }

        // Load a model with multiples mesh nodes for testing
        Entity* multipleNodesNoRoot{ m_MainScene->CreateEntity( EntityCreateInfo{
            .Root{ nullptr },
            .Name{ "Npc 1" },
            .Model{ m_ModelMultipleMeshes },
        } ) };

        if ( multipleNodesNoRoot ) {
            multipleNodesNoRoot->AddComponent<ScriptComponent>( "idle.lua" );
            multipleNodesNoRoot->AddComponent<AudioSourceComponent>( "quack.mp3" );
        }

        // Load a model 1 node mesh nodes for testing
        Entity* rootNoMultiple{ m_MainScene->CreateEntity( EntityCreateInfo{
            .Root{ multipleNodesNoRoot },
            .Name{ "Npc 2" },
            .Model{ m_ModelSingleMesh },
        } ) };

        if ( rootNoMultiple ) {
            rootNoMultiple->AddComponent<ScriptComponent>( "idle.lua" );
            rootNoMultiple->AddComponent<AudioSourceComponent>( "quack.mp3" );
        }

        // This can hear sound and has a camera
        // it would make sense as we generally want stuff close to the camera to be heard
        // the further they are from the camera, the less we can hear sources
        Entity* listener{ m_MainScene->CreateEntity( "PlushCat" ) };
        if ( listener ) {
            listener->AddComponent<CameraComponent>();
            listener->AddComponent<ScriptComponent>( "hello_world.lua" );
            listener->AddComponent<AudioListenerComponent>();
        }

        // Some checks just to test the Scene interface
        if ( m_MainScene->ExistsByName( "PlushCat" ) ) {
            MKT_CORE_LOGGER_WARN( "Entity with name {} exists.", "PlushCat" );
        } else {
            MKT_CORE_LOGGER_WARN( "Entity with name {} not exists.", "PlushCat" );
        }

        if (m_MainScene->ExistsByID( 4 )) {
            MKT_CORE_LOGGER_WARN( "Entity with ID {} exists.", 4 );
        } else {
            MKT_CORE_LOGGER_WARN( "Entity with ID {} does not exist.", 4 );
        }
    }

    auto GraphicsLayer::SetupCamera() -> void {
        constexpr float nearPlane{ 0.1f };
        constexpr float farPlane{ 1000.0f };
        constexpr float fov{ 45.0f };
        const float aspectRatio{ static_cast<float>( m_Window->GetWidth() ) / static_cast<float>( m_Window->GetHeight() ) };

        m_SceneCamera = CreateScope<SceneCamera>( fov, aspectRatio, nearPlane, farPlane );
        m_SceneCamera->SetTargetWindow( m_Window );
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
