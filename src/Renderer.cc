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
    auto Renderer::BeginScene(std::shared_ptr<OrthographicCamera> camera) -> void {
        s_DrawData->OrthographicCameraForScene = std::move(camera);
    }

    auto Renderer::BeginScene(std::shared_ptr<Camera> camera) -> void {
        s_DrawData->CameraForScene = std::move(camera);
    }

    auto Renderer::Submit(const RenderingData& data) -> void {
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
        s_QuadData = std::make_unique<Renderer2DDrawData>();
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
        s_QuadData->IndexBufferData = IndexBuffer::CreateBuffer({ 0, 1, 2, 2, 3, 0 });

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

    auto Renderer::SubmitQuad(const glm::vec3 &position, const glm::vec2 &size, const glm::vec4 &color, double angle, bool useOrthographicCamera) -> void {
        glm::mat4 cameraViewProj{};

        if (!useOrthographicCamera)
            // inverse transform matrix to get the camera View matrix
            cameraViewProj = s_DrawData->CameraForScene->GetProjection() * glm::inverse(s_DrawData->CameraForScene->GetTransform());
        else
            cameraViewProj = s_DrawData->OrthographicCameraForScene->GetProjectionView();

        // Data Setup
        static constexpr glm::vec3 ZAxis{ 0.0f, 0.0f, 1.0f };
        static constexpr glm::mat4 IdentityMatrix{ glm::mat4(1.0) };

        glm::mat4 scale{ glm::scale(IdentityMatrix, glm::vec3(size, 1.0f)) };
        glm::mat4 rotation{ glm::rotate(IdentityMatrix, (float)glm::radians(angle), ZAxis) };
        glm::mat4 transform{ glm::translate(IdentityMatrix, position) * scale * rotation };

        RenderingData data{
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

    auto Renderer::SubmitQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color, double angle, const std::shared_ptr<Texture>& texture, bool useOrthographicCamera) -> void {
        static constexpr glm::vec3 zAxis{ 0.0f, 0.0f, 1.0f };
        static constexpr glm::mat4 identMat{ glm::mat4(1.0) };

        glm::mat4 scale{ glm::scale(identMat, glm::vec3(size, 1.0f)) };
        glm::mat4 rotation{ glm::rotate(identMat, (float)glm::radians(angle), zAxis) };
        glm::mat4 transform{ glm::translate(identMat, position) * scale * rotation };

        SubmitQuad(transform, color, texture, useOrthographicCamera);
    }

    auto Renderer::SubmitQuad(const glm::mat4& transform, const glm::vec4& color, bool useOrthographicCamera) -> void {
        glm::mat4 cameraViewProj{};

        if (!useOrthographicCamera)
            // inverse transform matrix to get the camera View matrix
            cameraViewProj = s_DrawData->CameraForScene->GetProjection() * s_DrawData->CameraForScene->GetTransform();
        else
            cameraViewProj = s_DrawData->OrthographicCameraForScene->GetProjectionView();

        RenderingData data{
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

    auto Renderer::SubmitQuad(const glm::mat4& transform, [[maybe_unused]] const glm::vec4& color, const std::shared_ptr<Texture>& texture, bool useOrthographicCamera) -> void {
        glm::mat4 cameraViewProj{};

        if (!useOrthographicCamera)
            // inverse transform matrix to get the camera View matrix
            cameraViewProj = s_QuadData->CameraForScene->GetProjection() * glm::inverse(s_QuadData->CameraForScene->GetTransform());
        else
            cameraViewProj = s_DrawData->OrthographicCameraForScene->GetProjectionView();

        RenderingData data{
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
