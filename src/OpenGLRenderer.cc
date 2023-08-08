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
#include <Renderer/OpenGL/OpenGLRenderer.hh>
#include <Renderer/Buffers/FrameBuffer.hh>

namespace kaTe {
    auto OpenGLRenderer::SetClearColor(float red, float green, float blue, float alpha) -> void {
        glClearColor(red, green, blue, alpha);
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    }

    auto OpenGLRenderer::SetClearColor(const glm::vec4 &color) -> void {
        glClearColor(color.r, color.g, color.b, color.a);
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    }

    auto OpenGLRenderer::SetViewport(UInt32_T x, UInt32_T y, UInt32_T width, UInt32_T height) -> void {
        glViewport((GLsizei)x, (GLsizei)y, (GLsizei)width, (GLsizei)height);
    }

    auto OpenGLRenderer::DrawIndexed(const std::shared_ptr<VertexBuffer> &vertexBuffer, const std::shared_ptr<IndexBuffer> &indexBuffer) -> void {
        m_DefaultFrameBuffer.Bind();
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_BLEND);
        m_DefaultMaterial.BindShader();
        m_VertexArray.UseVertexBuffer(vertexBuffer);
        indexBuffer->Bind();

        glDrawElements(GL_TRIANGLES, (GLsizei)indexBuffer->GetCount(), GL_UNSIGNED_INT, nullptr);
        m_DefaultFrameBuffer.Unbind();
    }

    auto OpenGLRenderer::Draw(const RenderingData& data) -> void {

        m_DefaultMaterial.SetTiltingColor(data.Color);
        m_DefaultMaterial.SetProjectionView(data.TransformData.ProjectionView);
        m_DefaultMaterial.SetTransform(data.TransformData.Transform);

        if (data.TextureData != nullptr) {
            m_DefaultMaterial.SetTextureSampler(0);
            std::dynamic_pointer_cast<OpenGLTexture2D>(data.TextureData)->Bind(0);
        }

        if (data.IndexBufferData != nullptr) {
            m_DefaultMaterial.UploadUniformBuffersData();
            DrawIndexed(data.VertexBufferData, data.IndexBufferData);
        }
    }

    auto OpenGLRenderer::Init() -> void {
        m_DefaultMaterial.UploadShaders("../assets/shaders/debugShaderVert.glsl", "../assets/shaders/debugShaderFrag.glsl");
        CreateFrameBuffers();
    }

    auto OpenGLRenderer::Shutdown() -> void {

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

}