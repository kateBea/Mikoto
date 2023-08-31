/**
* OpenGLRenderer.cc
* Created by kate on 6/6/23.
* */

// C++ Standard Library
#include <memory>

// Third-Party Library
#include <GL/glew.h>

// Project Headers
#include <Core/Application.hh>
#include <Platform/Window/Window.hh>
#include <Renderer/Buffers/FrameBuffer.hh>
#include <Renderer/OpenGL/OpenGLRenderer.hh>
#include <Renderer/OpenGL/OpenGLIndexBuffer.hh>

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
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
        m_DefaultFrameBuffer.Unbind();
    }

    auto OpenGLRenderer::SetViewport(UInt32_T x, UInt32_T y, UInt32_T width, UInt32_T height) -> void {
        glViewport((GLsizei)x, (GLsizei)y, (GLsizei)width, (GLsizei)height);
    }

    auto OpenGLRenderer::DrawIndexed(const std::shared_ptr<VertexBuffer> &vertexBuffer, const std::shared_ptr<IndexBuffer> &indexBuffer) -> void {
        m_DefaultFrameBuffer.Bind();
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_BLEND);
        m_CurrentDefaultMaterial->BindShader();
        m_VertexArray.UseVertexBuffer(vertexBuffer);
        std::dynamic_pointer_cast<OpenGLIndexBuffer>(indexBuffer)->Bind();

        glDrawElements(GL_TRIANGLES, (GLsizei)indexBuffer->GetCount(), GL_UNSIGNED_INT, nullptr);
        m_DefaultFrameBuffer.Unbind();
    }

    auto OpenGLRenderer::Draw() -> void {
        for (const auto& drawData : m_DrawQueue) {
            m_CurrentDefaultMaterial = std::dynamic_pointer_cast<OpenGLDefaultMaterial>(drawData->MaterialData);

            m_CurrentDefaultMaterial->SetTiltingColor(drawData->Color);
            m_CurrentDefaultMaterial->SetProjectionView(drawData->TransformData.ProjectionView);
            m_CurrentDefaultMaterial->SetTransform(drawData->TransformData.Transform);

            if (m_CurrentDefaultMaterial->GetTexture() != nullptr) {
                m_CurrentDefaultMaterial->SetTextureSampler(0);
                std::dynamic_pointer_cast<OpenGLTexture2D>(m_CurrentDefaultMaterial->GetTexture())->Bind(0);
            }

            for (const auto& mesh : drawData->ModelData->GetMeshes()) {
                m_CurrentDefaultMaterial->UploadUniformBuffersData();
                DrawIndexed(mesh.GetVertexBuffer(), mesh.GetIndexBuffer());
            }
        }
    }

    auto OpenGLRenderer::Init() -> void {
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

    }

    auto OpenGLRenderer::CreateFrameBuffers() -> void {
        Window& window{ Application::Get().GetMainWindow() };
        FrameBufferCreateInfo createInfo{};

        createInfo.width = window.GetWidth();
        createInfo.height = window.GetHeight();
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