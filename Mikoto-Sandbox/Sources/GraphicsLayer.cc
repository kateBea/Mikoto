//
// Created by kate on 10/13/25.
//

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>// For glm::value_ptr

#include <Renderer/Core/RenderService.hh>
#include <Assets/AssetsService.hh>
#include <Core/InputService.hh>
#include <Filesystem/FileService.hh>
#include <GraphicsLayer.hh>
#include <ImGui/ImGuiService.hh>
#include <ImGui/ImGuiUtility.hh>
#include <Memory/Allocator.hh>
#include <Physics/PhysicService.hh>
#include <Renderer/Core/RenderUtility.hh>
#include <Scene/Component.hh>

#include "Scene/SceneManager.hh"

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

        ShaderModuleHandle pbrVertex{ ShaderLibrary::Get()->LoadShader( "./Resources/Shaders/vulkan-spirv/PBRVertexShader.sprv", ShaderStage::VERTEX_STAGE ) };
        ShaderModuleHandle pbrFragment{ ShaderLibrary::Get()->LoadShader( "./Resources/Shaders/vulkan-spirv/StandardFragmentShader.sprv", ShaderStage::FRAGMENT_STAGE ) };
    }

    GraphicsLayer::GraphicsLayer( std::string_view name, const Window *window )
        : ILayer{ name }, m_Window{ window } {}

    auto GraphicsLayer::OnCreate() -> void {
        MKT_FILE_LOGGER_DEBUG( "Initializing Graphics Layer" );
        TestCode();

        LoadModels();

        SetupCamera();

        SetupRenderer();

        SetupScene();
    }

    auto GraphicsLayer::OnDestroy() -> void {
        m_Renderer->Shutdown();

        m_Renderer = nullptr;
    }

    auto GraphicsLayer::UpdateListener() -> void {
        TransformComponent &transform{ m_Listener->GetComponent<TransformComponent>() };
        AudioListenerComponent &listenerComp{ m_Listener->GetComponent<AudioListenerComponent>() };
        AudioListener &listener{ listenerComp.GetListener() };

        if (ImGui::Begin( "Audio Listener" )) {
            // === Position ===
            Vec3F position{ transform.GetTranslation() };
            if (ImGui::DragFloat3( "Position", glm::value_ptr( position ), 0.1f )) {
                listener.SetPosition( position.x, position.y, position.z );
                transform.SetTranslation( position );
            }

            // === Orientation ===
            static Vec3F forward{ 0.0f, 0.0f, -1.0f };
            static Vec3F up{ 0.0f, 1.0f, 0.0f };
            ImGui::SeparatorText( "Orientation" );
            ImGui::DragFloat3( "Forward", glm::value_ptr( forward ), 0.05f, -1.0f, 1.0f );
            ImGui::DragFloat3( "Up", glm::value_ptr( up ), 0.05f, -1.0f, 1.0f );
            if (ImGui::Button( "Apply Orientation" )) {
                listener.SetOrientation( forward.x, forward.y, forward.z, up.x, up.y, up.z );
            }

            // === Velocity ===
            static Vec3F velocity{ 0.0f, 0.0f, 0.0f };
            ImGui::SeparatorText( "Velocity" );
            if (ImGui::DragFloat3( "Velocity", glm::value_ptr( velocity ), 0.1f )) {
                listener.SetVelocity( velocity.x, velocity.y, velocity.z );
            }

            ImGui::Separator();
            static bool autoApply{ true };
            if (ImGui::Checkbox( "Auto apply", std::addressof( autoApply ) )) {

            }
            if (!autoApply) {
                ImGui::SameLine(  );
                if (ImGui::Button( "Apply Listener State" )) {
                    listener.Apply();
                }
            } else {
                listener.Apply();
            }

            ImGui::End();
        }
    }

    auto GraphicsLayer::OnUpdate( const float deltaTime ) -> void {
        ImGuiUtils::ImGuiScopedBorderColor borderColor{ { 66, 200, 255, 255 } };

        UpdateListener();

        UpdateCamera( deltaTime );
        DisplayCameraDebugInfo();

        m_MainScene->SetState( SceneState::IDLE );

        m_Renderer->SetScene( m_MainScene);
        m_Renderer->SetCamera( m_SceneCamera.get() );

        m_Renderer->Render( deltaTime );

        DrawViewport();
    }

    auto GraphicsLayer::OnEvent( Event &event ) -> void {

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
        m_MainScene = SceneManager::Get()->CreateScene( "Hello World" );
        m_MainScene->SetName( "Change name just for fun" );

        // You need to specify the Scene the physics are simulated on
        PhysicService::Get()->SetSimulationTarget( m_MainScene );

        // This emitting sounds
        Entity *entity{ m_MainScene->CreateEntity( "Ball" ) };
        if (entity) {
            entity->AddComponent<ScriptComponent>( "hello_world.lua" );
            entity->AddComponent<AudioSourceComponent>( "my_song.mp3" );
        }

        // Load a model with multiple mesh nodes for testing
        Entity *multipleNodes{ m_MainScene->CreateEntity( EntityCreateInfo{
            .Root{ entity },
            .Name{ "Npc" },
            .Model{ m_ModelMultipleMeshes },
        } ) };

        if (entity) {
            multipleNodes->AddComponent<ScriptComponent>( "idle.lua" );
            multipleNodes->AddComponent<AudioSourceComponent>( "quack.mp3" );
        }

        // Load a model with multiple mesh nodes for testing
        Entity *multipleNodesNoRoot{ m_MainScene->CreateEntity( EntityCreateInfo{
            .Root{ nullptr },
            .Name{ "Npc 1" },
            .Model{ m_ModelMultipleMeshes },
        } ) };

        if (multipleNodesNoRoot) {
            multipleNodesNoRoot->AddComponent<ScriptComponent>( "idle.lua" );
            multipleNodesNoRoot->AddComponent<AudioSourceComponent>( "quack.mp3" );

            multipleNodesNoRoot->AddComponent<MeshComponent>( );
        }

        // Load a model 1 node mesh nodes for testing
        Entity *rootNoMultiple{ m_MainScene->CreateEntity( EntityCreateInfo{
            .Root{ multipleNodesNoRoot },
            .Name{ "Npc 2" },
        } ) };

        if (rootNoMultiple) {
            rootNoMultiple->AddComponent<ScriptComponent>( "idle.lua" );
            rootNoMultiple->AddComponent<AudioSourceComponent>( "quack.mp3" );
        }

        // This can hear sound and has a camera
        // it would make sense as we generally want stuff close to the camera to be heard
        // the further they are from the camera, the less we can hear sources
        m_Listener = m_MainScene->CreateEntity( "PlushCat" );
        if (m_Listener) {
            m_Listener->AddComponent<CameraComponent>();
            m_Listener->AddComponent<ScriptComponent>( "hello_world.lua" );
            AudioListenerComponent &listenerComp{ m_Listener->AddComponent<AudioListenerComponent>() };
            AudioListener &audioListener{ listenerComp.GetListener() };
            audioListener.Apply();

            RigidBodyComponent& rigidBody{ m_Listener->AddComponent<RigidBodyComponent>() };
            rigidBody.SetBodyType( RigidBodyComponent::BodyType::DYNAMIC );
            rigidBody.SetFriction( 0 );

            // This requires the simulation scene to have been specified before
            m_MainScene->AttachRigidBody( m_Listener );
        }

        // Some checks just to test the Scene interface
        if (m_MainScene->ExistsByName( "PlushCat" )) {
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
        spec.WithName( "Scene renderer" )
            .WithDevice( RenderService::Get()->GetGpuDevice() );

        m_Renderer = SceneRenderer::Create( spec );

        if (m_Renderer) {
            m_Renderer->Init();
        }
    }

    auto GraphicsLayer::UpdateCamera( const float timeStep ) -> void {

        m_SceneCamera->SetMovementSpeed( 50.f );
        m_SceneCamera->SetRotationSpeed( 50.f );

        m_SceneCamera->SetFarPlane( 20000.0f );
        m_SceneCamera->SetNearPlane( 1.0f );

        m_SceneCamera->WantRotation( true, true );

        m_SceneCamera->SetFieldOfView( 45 );

        // Get viewport dimensions from the window
        // we render to the window here not to an imgui viewport
        m_SceneCamera->SetViewportSize( m_Window->GetWidth(), m_Window->GetHeight() );

        if (InputService::Get()->IsMouseKeyPressed( Mouse_Button_Right )) {
            m_SceneCamera->EnableCamera( true );
        } else {
            m_SceneCamera->EnableCamera( false );
        }

        m_SceneCamera->UpdateState( timeStep );
    }

    auto GraphicsLayer::DrawViewport() const -> void {

        if (ImGui::Begin( "Viewport" )) {
            ImGuiBackend *backend{ ImGuiService::Get()->GetBackend() };
            const TextureHandle finalComposition{ m_Renderer->GetFinalComposition() };

            const ImVec2 dim{ ImGui::GetContentRegionAvail() };

            m_SceneCamera->SetViewportSize( dim.x, dim.y );

            const ImTextureID textureID{ backend->ConstructImGuiTextureID( finalComposition ) };
            ImGui::Image( textureID, ImVec2{ dim.x, dim.y }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 } );

            ImGui::End();
        }
    }

    auto GraphicsLayer::DisplayCameraDebugInfo() const -> void {
        if (ImGui::Begin( "Camera Properties" )) {
            ImGui::Text( "Camera Debug Info" );
            ImGui::Separator();

            // --- Position ---
            glm::vec3 position{ m_SceneCamera->GetPosition() };
            if (ImGui::DragFloat3( "Position", glm::value_ptr( position ), 0.1f )) m_SceneCamera->SetPosition( position );

            // --- Rotation ---
            glm::vec3 rotation{ m_SceneCamera->GetRotation() };
            if (ImGui::DragFloat3( "Rotation (deg)", glm::value_ptr( rotation ), 0.5f )) m_SceneCamera->SetRotation( rotation );

            ImGui::Separator();

            // --- Projection ---
            float fov{ m_SceneCamera->GetFOV() };
            if (ImGui::SliderFloat( "Field of View", &fov, 10.0f, 120.0f )) m_SceneCamera->SetFieldOfView( fov );

            float nearPlane{ m_SceneCamera->GetNearPlane() };
            float farPlane{ m_SceneCamera->GetFarPlane() };

            if (ImGui::DragFloat( "Near Plane", &nearPlane, 0.01f, 0.01f, farPlane - 0.1f )) m_SceneCamera->SetNearPlane( nearPlane );

            if (ImGui::DragFloat( "Far Plane", &farPlane, 1.0f, nearPlane + 0.1f, 10000.0f )) m_SceneCamera->SetFarPlane( farPlane );

            ImGui::Separator();

            // --- Projection type (combo box) ---
            static const char *projectionTypes[]{ "Perspective", "Orthographic" };
            int currentProjection = m_SceneCamera->IsOrthographic() ? 1 : 0;

            if (ImGui::Combo( "Projection Type", &currentProjection, projectionTypes, IM_ARRAYSIZE( projectionTypes ) )) {
                m_SceneCamera->SetProjectionType(
                        currentProjection == 0 ? ProjectionType::PERSPECTIVE : ProjectionType::ORTHOGRAPHIC );
            }

            ImGui::Separator();

            // --- Matrices ---
            if (ImGui::TreeNode( "Matrices" )) {
                const glm::mat4 &view{ m_SceneCamera->GetViewMatrix() };
                const glm::mat4 &proj{ m_SceneCamera->GetProjection() };

                if (ImGui::BeginTable( "MatrixTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg )) {
                    ImGui::TableSetupColumn( "Row" );
                    ImGui::TableSetupColumn( "View C0" );
                    ImGui::TableSetupColumn( "View C1" );
                    ImGui::TableSetupColumn( "View C2" );
                    ImGui::TableSetupColumn( "View C3" );
                    ImGui::TableHeadersRow();


                    for (Int32 i{}; i < 4; ++i) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex( 0 );
                        ImGui::Text( "Row %d", i );
                        ImGui::TableSetColumnIndex( 1 );
                        ImGui::Text( "%.3f", view[i][0] );
                        ImGui::TableSetColumnIndex( 2 );
                        ImGui::Text( "%.3f", view[i][1] );
                        ImGui::TableSetColumnIndex( 3 );
                        ImGui::Text( "%.3f", view[i][2] );
                        ImGui::TableSetColumnIndex( 4 );
                        ImGui::Text( "%.3f", view[i][3] );
                    }

                    ImGui::Separator();
                    ImGui::EndTable();
                }

                ImGui::Spacing();

                if (ImGui::BeginTable( "ProjectionMatrixTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg )) {
                    ImGui::TableSetupColumn( "Row" );
                    ImGui::TableSetupColumn( "Proj C0" );
                    ImGui::TableSetupColumn( "Proj C1" );
                    ImGui::TableSetupColumn( "Proj C2" );
                    ImGui::TableSetupColumn( "Proj C3" );
                    ImGui::TableHeadersRow();

                    for (Int32 i{}; i < 4; ++i) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex( 0 );
                        ImGui::Text( "Row %d", i );
                        ImGui::TableSetColumnIndex( 1 );
                        ImGui::Text( "%.3f", proj[i][0] );
                        ImGui::TableSetColumnIndex( 2 );
                        ImGui::Text( "%.3f", proj[i][1] );
                        ImGui::TableSetColumnIndex( 3 );
                        ImGui::Text( "%.3f", proj[i][2] );
                        ImGui::TableSetColumnIndex( 4 );
                        ImGui::Text( "%.3f", proj[i][3] );
                    }

                    ImGui::EndTable();
                }

                ImGui::TreePop();
            }
        }
        ImGui::End();
    }

}// namespace Mikoto