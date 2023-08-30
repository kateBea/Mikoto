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

        s_RenderingStats    = std::make_unique<RenderingStats>();
        s_SavedSceneStats   = std::make_unique<RenderingStats>();

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

    auto Renderer::SubmitQuad(const glm::mat4 &transform, const glm::vec4& color, std::shared_ptr<Material> material) -> void {
        glm::mat4 cameraViewProj{ s_DrawData->SceneEditCamera->GetViewProjection() };

        auto data{ std::make_shared<DrawData>() };
        data->Color = color;
        data->VertexBufferData = s_QuadData->VertexBufferData;
        data->IndexBufferData = s_QuadData->IndexBufferData;
        data->MaterialData = std::move(material);
        data->TransformData.ProjectionView = cameraViewProj;
        data->TransformData.Transform = transform;

        // Render
        Renderer::Submit(data);
    }

    auto Renderer::OnEvent(Event& event) -> void {
        s_ActiveRendererAPI->OnEvent(event);
    }

    auto Renderer::LoadPrefabs() -> void {
        Construct2DPlane();
    }

    auto Renderer::Construct2DPlane() -> void {
        s_DrawData = std::make_unique<RendererDrawData>();
        s_QuadData = std::make_unique<RendererDrawData>();

        const std::vector<float> squareData {
                // Positions            // Normals           // Colors                // Texture coordinates
                -0.5f,  -0.5f, 0.0f,    0.0f, 0.0f, 0.0f,    1.0f, 0.0f, 0.0f,        0.0f, 0.0f,   // bottom left
                0.5f,  -0.5f, 0.0f,    0.0f, 0.0f, 0.0f,    0.0f, 1.0f, 0.0f,        1.0f, 0.0f,   // bottom right
                0.5f,   0.5f, 0.0f,    0.0f, 0.0f, 0.0f,    0.0f, 0.0f, 1.0f,        1.0f, 1.0f,   // top right
                -0.5f,   0.5f, 0.0f,    0.0f, 0.0f, 0.0f,    0.8f, 0.3f, 0.4f,        0.0f, 1.0f,   // top left
        };

        s_QuadData->VertexBufferData = VertexBuffer::CreateBuffer(squareData);
        s_QuadData->IndexBufferData = IndexBuffer::Create({0, 1, 2, 2, 3, 0});

        s_QuadData->VertexBufferData->SetBufferLayout(BufferLayout{
                { ShaderDataType::FLOAT3_TYPE, "a_Position" },
                { ShaderDataType::FLOAT3_TYPE, "a_Normal" },
                { ShaderDataType::FLOAT3_TYPE, "a_Color" },
                { ShaderDataType::FLOAT2_TYPE, "a_TextureCoordinates" },
        });
    }
}
