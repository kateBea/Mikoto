/**
 * Renderer.cc
 * Created by kate on 6/5/23.
 * */

// C++ Standard Library
#include <memory>
#include <utility>

// Third-Party Libraries
#include <glm/glm.hpp>

// Project Headers
#include <Utility/Common.hh>
#include <Renderer/Renderer.hh>
#include <Renderer/RenderCommand.hh>
#include <Renderer/OpenGL/OpenGLRenderer.hh>
#include <Renderer/Vulkan/VulkanRenderer.hh>

namespace Mikoto {
    auto Renderer::BeginScene(const ScenePrepareData& prepareData) -> void {
        s_DrawData->SceneRuntimeCamera = prepareData.RuntimeCamera;
        s_DrawData->SceneEditCamera = prepareData.StaticCamera;
    }

    auto Renderer::Submit(std::shared_ptr<DrawData> data) -> void {
        RenderCommand::AddToRenderQueue(std::move(data));

        // TODO: review
        // Rendering Stats management
        s_RenderingStats->IncrementQuadCount(1);
        // We increment the number of draw calls because for now
        // The RenderCommand directly flushes the draw call
        s_RenderingStats->IncrementDrawCallCount(1);
    }

    auto Renderer::EndScene() -> void {
        s_SavedSceneStats = std::make_unique<RenderingStats>(*s_RenderingStats);
        s_RenderingStats->Reset();
    }

    auto Renderer::Flush() -> void {
        RenderCommand::Flush();
    }

    auto Renderer::Init() -> void {
        PickGraphicsAPI();
        RenderCommand::Init(s_ActiveRendererAPI);

        s_DrawData = std::make_unique<RendererDrawData>();
        s_RenderingStats = std::make_unique<RenderingStats>();
        s_SavedSceneStats = std::make_unique<RenderingStats>();

        LoadPrefabs();
    }

    auto Renderer::PickGraphicsAPI() -> void {
        switch(GetActiveGraphicsAPI()) {
            case GraphicsAPI::OPENGL_API:
                s_ActiveRendererAPI = new OpenGLRenderer();
                s_ActiveRendererAPI->Init();
                break;
            case GraphicsAPI::VULKAN_API:
                s_ActiveRendererAPI = new VulkanRenderer();
                s_ActiveRendererAPI->Init();
                break;
            default:
                MKT_CORE_LOGGER_CRITICAL("Unsupported renderer API");
                break;
        }
    }

    auto Renderer::ShutDown() -> void {
        delete s_ActiveRendererAPI;
    }

    auto Renderer::Submit(const SceneObjectData& objectData, const glm::mat4& transform, std::shared_ptr<Material> material) -> void {
        auto data{ std::make_shared<DrawData>() };
        glm::mat4 cameraViewProj{ s_DrawData->SceneEditCamera->GetViewProjection() };

        if (objectData.IsPrefab) {
            auto& sprite{ s_Prefabs[GetSpritePrefabName()] };
            auto& cube{ s_Prefabs[GetCubePrefabName()] };
            // retrieve prefab type
            switch (objectData.PrefabType) {
                case PrefabSceneObject::SPRITE_PREFAB_OBJECT:
                    data->ModelData = sprite.ModelData;
                    break;
                case PrefabSceneObject::CUBE_PREFAB_OBJECT:
                    data->ModelData = cube.ModelData;
                    break;
            }
        }

        data->Color = objectData.Color;
        data->MaterialData = std::move(material);
        data->TransformData.Transform = transform;
        data->TransformData.ProjectionView = cameraViewProj;

        // Rendering submission
        Renderer::Submit(data);
    }

    auto Renderer::SubmitQuad(const glm::mat4 &transform, const glm::vec4& color, std::shared_ptr<Material> material) -> void {
        glm::mat4 cameraViewProj{ s_DrawData->SceneEditCamera->GetViewProjection() };
        auto& sprite{ s_Prefabs[GetSpritePrefabName()] };

        // Setup rendering data
        auto data{ std::make_shared<DrawData>() };
        data->Color = color;
        data->ModelData = sprite.ModelData;
        data->MaterialData = std::move(material);
        data->TransformData.Transform = transform;
        data->TransformData.ProjectionView = cameraViewProj;

        // Rendering submission
        Renderer::Submit(data);
    }

    auto Renderer::OnEvent(Event& event) -> void {
        s_ActiveRendererAPI->OnEvent(event);
    }

    auto Renderer::LoadPrefabs() -> void {
        AddSpritePrefab();
        AddCubePrefab();
    }

    auto Renderer::AddSpritePrefab() -> void {
        const std::vector<float> squareData {
                // Positions            // Normals           // Colors                // Texture coordinates
                -0.5f,  -0.5f, 0.0f,    0.0f, 0.0f, 0.0f,    1.0f, 0.0f, 0.0f,        0.0f, 0.0f,   // bottom left
                0.5f,  -0.5f, 0.0f,    0.0f, 0.0f, 0.0f,    0.0f, 1.0f, 0.0f,        1.0f, 0.0f,   // bottom right
                0.5f,   0.5f, 0.0f,    0.0f, 0.0f, 0.0f,    0.0f, 0.0f, 1.0f,        1.0f, 1.0f,   // top right
                -0.5f,   0.5f, 0.0f,    0.0f, 0.0f, 0.0f,    0.8f, 0.3f, 0.4f,        0.0f, 1.0f,   // top left
        };

        // Set index and vertex buffers
        auto vertexBuffer{ VertexBuffer::CreateBuffer(squareData) };
        auto indexBuffer{ IndexBuffer::Create({0, 1, 2, 2, 3, 0}) };

        // Set layout
        // get layout from model since it is used later to create the model prefab
        vertexBuffer->SetBufferLayout(BufferLayout{
                { ShaderDataType::FLOAT3_TYPE, "a_Position" },
                { ShaderDataType::FLOAT3_TYPE, "a_Normal" },
                { ShaderDataType::FLOAT3_TYPE, "a_Color" },
                { ShaderDataType::FLOAT2_TYPE, "a_TextureCoordinates" },
        });

        // Construct mesh and add it to the model
        MeshData meshData{};
        meshData.SetVertices(vertexBuffer);
        meshData.SetIndices(indexBuffer);

        PrefabData prefab{};
        prefab.TransformData = {};
        prefab.ModelData = std::make_shared<ModelPrefab>();

        prefab.ModelData->AddMesh(meshData);

        // Add model to the list of prefabs
        s_Prefabs.emplace(GetSpritePrefabName(), prefab);
    }

    auto Renderer::AddCubePrefab() -> void {
        // Construct mesh and add it to the model
        PrefabData prefab{};
        prefab.TransformData = {};
        prefab.ModelData = std::make_shared<ModelPrefab>("../assets/models/Prefabs/cube/source/cube.obj");

        // Add model to the list of prefabs
        s_Prefabs.emplace(GetCubePrefabName(), prefab);
    }
}
