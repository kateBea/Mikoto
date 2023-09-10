/**
* OpenGLRenderer.cc
* Created by kate on 6/6/23.
* */

// C++ Standard Library
#include <memory>

// Third-Party Library
#include <GL/glew.h>

// Project Headers
#include <Platform/Window/Window.hh>
#include <Renderer/Buffers/FrameBuffer.hh>
#include <Renderer/OpenGL/OpenGLRenderer.hh>
#include <Renderer/OpenGL/OpenGLIndexBuffer.hh>
#include <Renderer/OpenGL/OpenGLVertexBuffer.hh>

namespace Mikoto {
    auto OpenGLRenderer::SetClearColor(float red, float green, float blue, float alpha) -> void {
        m_DefaultFrameBuffer.Bind();
        glClearColor(red, green, blue, alpha);
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
        m_DefaultFrameBuffer.Unbind();
    }

    auto OpenGLRenderer::SetClearColor(const glm::vec4 &color) -> void {
        m_DefaultFrameBuffer.Bind();
        glClearColor(color.r, color.g, color.b, color.a);
        m_DefaultFrameBuffer.Unbind();
    }

    auto OpenGLRenderer::SetViewport(float x, float y, float width, float height) -> void {
        glViewport((GLsizei)x, (GLsizei)y, (GLsizei)width, (GLsizei)height);
    }

    auto OpenGLRenderer::DrawIndexed(const std::shared_ptr<OpenGLVertexBuffer>& vertexBuffer, const std::shared_ptr<OpenGLIndexBuffer>& indexBuffer) -> void {
        m_CurrentDefaultMaterial->GetShader()->Bind();
        m_VertexArray.UseVertexBuffer(vertexBuffer);
        std::dynamic_pointer_cast<OpenGLIndexBuffer>(indexBuffer)->Bind();

        glDrawElements(GL_TRIANGLES, (GLsizei)indexBuffer->GetCount(), indexBuffer->GetBufferDataType(), nullptr);
    }

    auto OpenGLRenderer::Draw() -> void {
        m_DefaultFrameBuffer.Bind();
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
        const auto maxTextureSlots{ OpenGLDefaultMaterial::GetMaxConcurrentSamplingTextures() };

        for (const auto& drawData : m_DrawQueue) {
            m_CurrentDefaultMaterial = std::dynamic_pointer_cast<OpenGLDefaultMaterial>(drawData->MaterialData);

            m_CurrentDefaultMaterial->SetTiltingColor(drawData->Color);
            m_CurrentDefaultMaterial->SetProjection(drawData->TransformData.Projection);
            m_CurrentDefaultMaterial->SetView(drawData->TransformData.View);
            m_CurrentDefaultMaterial->SetTransform(drawData->TransformData.Transform);

            for (const auto& mesh : drawData->ModelData->GetMeshes()) {
                // Setup textures
                // The mesh may have multiple textures we want to sample from.
                // Therefore, the texture should not be part of the material
#if false
                // TODO: pending of testing
                UInt32_T samplerIndex{};

                for (const auto& texture : mesh.GetTextures()) {
                    if (samplerIndex < maxTextureSlots) {
                        // TODO: dodgy
                        OpenGLTexture2D* oglTexture{ (OpenGLTexture2D*)texture.get() };

                        m_CurrentDefaultMaterial->SetTextureSampler(samplerIndex);
                        oglTexture->Bind(samplerIndex);
                        ++samplerIndex;
                    }
                    else {
                        // No need to continue if we have used all texture slots
                        break;
                    }
                }
#endif

                // Upload uniform data
                m_CurrentDefaultMaterial->UploadUniformBuffersData();

                // Draw command
                const auto& vertexBuffer{ std::dynamic_pointer_cast<OpenGLVertexBuffer>(mesh.GetVertexBuffer()) };
                const auto& indexBuffer{ std::dynamic_pointer_cast<OpenGLIndexBuffer>(mesh.GetIndexBuffer()) };
                DrawIndexed(vertexBuffer, indexBuffer);
            }
        }

        m_DefaultFrameBuffer.Unbind();
    }

    auto OpenGLRenderer::Init() -> void {
        // Blending
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Enable Depth testing
        glEnable(GL_DEPTH_TEST);

        // Face culling
        // Temporarily disabled
#if false
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
#endif

        // Stencil
        // Temporarily disabled
#if false
        glEnable(GL_STENCIL_TEST);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
#endif

        CreateFrameBuffers();
    }

    auto OpenGLRenderer::Shutdown() -> void {
        m_DefaultFrameBuffer.OnRelease();
    }

    auto OpenGLRenderer::EnableWireframeMode() -> void {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    auto OpenGLRenderer::DisableWireframeMode() -> void {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    auto OpenGLRenderer::OnEvent(Event& event) -> void {
        (void)event;
    }

    auto OpenGLRenderer::CreateFrameBuffers() -> void {
        FrameBufferCreateInfo createInfo{};
        createInfo.width = 1920;
        createInfo.height = 1080;
        createInfo.samples = 1;

        m_DefaultFrameBuffer.OnCreate(createInfo);
    }

    auto OpenGLRenderer::Flush() -> void {
        Draw();
        m_DrawQueue.clear();
    }

    auto OpenGLRenderer::QueueForDrawing(std::shared_ptr<DrawData> data) -> void {
        m_DrawQueue.emplace_back(data);
    }

}