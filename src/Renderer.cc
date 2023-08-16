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
        s_DrawData->SceneCamera = prepareData.SceneCamera;
    }

    auto Renderer::Submit(const DrawData & data) -> void {
        RenderCommand::Draw(data);
    }

    auto Renderer::EndScene() -> void {
        s_SavedSceneStats = std::make_unique<RenderingStats>(*s_RenderingStats);
        s_RenderingStats->Reset();
    }

    auto Renderer::Flush() -> void {

    }

    auto Renderer::Init() -> void {
        PickGraphicsAPI();
        RenderCommand::Init(s_ActiveRendererAPI);

        s_DrawData = std::make_unique<RendererDrawData>();
        s_QuadData = std::make_unique<RendererDrawData>();
        s_RenderingStats    = std::make_unique<RenderingStats>();
        s_SavedSceneStats   = std::make_unique<RenderingStats>();

        // Init Data for quad rendering
        const std::vector<float> squareData {
            // Positions            // Texture coordinates
            -0.5f,  -0.5f, 0.0f,     0.0f, 0.0f,   // bottom left
             0.5f,  -0.5f, 0.0f,     1.0f, 0.0f,   // bottom right
             0.5f,   0.5f, 0.0f,     1.0f, 1.0f,   // top right
            -0.5f,   0.5f, 0.0f,     0.0f, 1.0f,   // top left
        };

        s_QuadData->VertexBufferData = VertexBuffer::CreateBuffer(squareData);
        s_QuadData->IndexBufferData = IndexBuffer::Create({0, 1, 2, 2, 3, 0});

        s_QuadData->VertexBufferData->SetBufferLayout(BufferLayout{
            { ShaderDataType::FLOAT3_TYPE, "a_Position" },
            { ShaderDataType::FLOAT2_TYPE, "a_TextureCoordinates" }
        });
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
                KATE_CORE_LOGGER_CRITICAL("Unsupported renderer API");
                break;
        }
    }

    auto Renderer::ShutDown() -> void {
        delete s_ActiveRendererAPI;
    }

    auto Renderer::SubmitQuad(const glm::vec3 &position, const glm::vec2 &size, const glm::vec4 &color, double angle) -> void {
        glm::mat4 cameraViewProj{ s_DrawData->SceneCamera->GetProjection() * glm::inverse(s_DrawData->SceneCamera->GetTransform()) };

        // Data Setup
        static constexpr glm::vec3 ZAxis{ 0.0f, 0.0f, 1.0f };
        static constexpr glm::mat4 IdentityMatrix{ glm::mat4(1.0) };

        glm::mat4 scale{ glm::scale(IdentityMatrix, glm::vec3(size, 1.0f)) };
        glm::mat4 rotation{ glm::rotate(IdentityMatrix, (float)glm::radians(angle), ZAxis) };
        glm::mat4 transform{ glm::translate(IdentityMatrix, position) * scale * rotation };

        DrawData data{
                .VertexBufferData = s_QuadData->VertexBufferData,
                .IndexBufferData = s_QuadData->IndexBufferData,

                .TransformData{ .ProjectionView = cameraViewProj, .Transform = transform },
                .Color = color,
        };

        // Render
        RenderCommand::Draw(data);

        // Rendering Stats management
        s_RenderingStats->IncrementQuadCount(1);
        // We increment the number of draw calls because for now
        // The RenderCommand directly flushes the draw call
        s_RenderingStats->IncrementDrawCallCount(1);
    }

    auto Renderer::SubmitQuad(const glm::vec3 &position, const glm::vec2 &size, const glm::vec4 &color, double angle, const std::shared_ptr<Texture> &texture) -> void {
        static constexpr glm::vec3 zAxis{ 0.0f, 0.0f, 1.0f };
        static constexpr glm::mat4 identMat{ glm::mat4(1.0) };

        glm::mat4 scale{ glm::scale(identMat, glm::vec3(size, 1.0f)) };
        glm::mat4 rotation{ glm::rotate(identMat, (float)glm::radians(angle), zAxis) };
        glm::mat4 transform{ glm::translate(identMat, position) * scale * rotation };

        SubmitQuad(transform, color, texture);
    }

    auto Renderer::SubmitQuad(const glm::mat4 &transform, const glm::vec4 &color) -> void {
        glm::mat4 cameraViewProj{ s_DrawData->SceneCamera->GetProjection() * s_DrawData->SceneCamera->GetTransform() };

        DrawData data{
                .VertexBufferData = s_QuadData->VertexBufferData,
                .IndexBufferData = s_QuadData->IndexBufferData,

                .TransformData{ .ProjectionView = cameraViewProj , .Transform = transform},
                .Color = color,
        };

        // Render
        RenderCommand::Draw(data);

        // Rendering Stats management
        s_RenderingStats->IncrementQuadCount(1);
        // We increment the number of draw calls because for now
        // The RenderCommand directly flushes the draw call
        s_RenderingStats->IncrementDrawCallCount(1);
    }

    auto Renderer::SubmitQuad(const glm::mat4 &transform, const glm::vec4 &color, const std::shared_ptr<Texture> &texture) -> void {
        glm::mat4 cameraViewProj{ s_QuadData->SceneCamera->GetProjection() * glm::inverse(s_QuadData->SceneCamera->GetTransform()) };

        DrawData data{
                .VertexBufferData = s_QuadData->VertexBufferData,
                .IndexBufferData = s_QuadData->IndexBufferData,
                .TextureData = texture,
                .TransformData{ .ProjectionView = cameraViewProj, .Transform = transform },
                .Color = color,
        };

        // Render
        RenderCommand::Draw(data);

        // Rendering Stats management
        s_RenderingStats->IncrementQuadCount(1);
        // We increment the number of draw calls because for now
        // The RenderCommand directly flushes the draw call
        s_RenderingStats->IncrementDrawCallCount(1);
    }

    auto Renderer::OnEvent(Event& event) -> void {
        s_ActiveRendererAPI->OnEvent(event);
    }
}
