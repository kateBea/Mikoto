/**
 * OpenGLDefaultMaterial.hh
 * Created by kate on 7/20/2023.
 * */

// Third-Party Libraries
#include <glm/glm.hpp>

// Project Headers
#include <Renderer/OpenGL/OpenGLDefaultMaterial.hh>

namespace Mikoto {
    auto OpenGLDefaultMaterial::UploadShaders(const Path_T &vertexShader, const Path_T &fragmentShader) -> void {
        m_DefaultVertexPixelShaders = std::dynamic_pointer_cast<OpenGLShader>(Shader::Create(vertexShader, fragmentShader));
    }

    auto OpenGLDefaultMaterial::SetTextureSampler(Int32_T value) -> void {
        m_DefaultVertexPixelShaders->SetUniformInt("u_TextSampler", value);
    }

    auto OpenGLDefaultMaterial::SetTiltingColor(float red, float green, float blue, float alpha) -> void {
        m_Color.r = red;
        m_Color.g = green;
        m_Color.b = blue;
        m_Color.a = alpha;
    }

    auto OpenGLDefaultMaterial::SetTiltingColor(const glm::vec4& color) -> void {
        m_Color = color;
    }

    auto OpenGLDefaultMaterial::SetTransform(const glm::mat4& mat) -> void {
        m_Transform = mat;
    }

    auto OpenGLDefaultMaterial::UploadUniformBuffersData() -> void {
        m_DefaultVertexPixelShaders->SetUniformMat4("u_Transform", m_Transform);
        m_DefaultVertexPixelShaders->SetUniformMat4("u_Projection", m_ProjectionMatrix);
        m_DefaultVertexPixelShaders->SetUniformMat4("u_View", m_ViewMatrix);
        m_DefaultVertexPixelShaders->SetUniformVec4("u_Color", m_Color);

    }

    OpenGLDefaultMaterial::OpenGLDefaultMaterial()
        :   Material{ "OpenGL Default Material" }
    {
        UploadShaders("../assets/shaders/opengl-glsl/debugShaderVert.glsl", "../assets/shaders/opengl-glsl/debugShaderFrag.glsl");
    }
}
