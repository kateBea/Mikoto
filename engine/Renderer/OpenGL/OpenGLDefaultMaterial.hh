/**
 * OpenGLDefaultMaterial.hh
 * Created by kate on 7/20/2023.
 * */

#ifndef MIKOTO_OPENGL_DEFAULT_MATERIAL_HH
#define MIKOTO_OPENGL_DEFAULT_MATERIAL_HH

// C++ Standard Library
#include <memory>

// Third-Party Libraries
#include <glm/glm.hpp>

// Project Headers
#include <Utility/Common.hh>
#include <Renderer/Material/Material.hh>
#include <Renderer/OpenGL/OpenGLShader.hh>
#include <Renderer/OpenGL/OpenGLTexture2D.hh>

namespace Mikoto {
    class OpenGLDefaultMaterial : public Material {
    public:
        explicit OpenGLDefaultMaterial();

        OpenGLDefaultMaterial(const OpenGLDefaultMaterial& other) = default;
        OpenGLDefaultMaterial(OpenGLDefaultMaterial&& other) = default;

        auto operator=(const OpenGLDefaultMaterial& other) -> OpenGLDefaultMaterial& = default;
        auto operator=(OpenGLDefaultMaterial&& other) -> OpenGLDefaultMaterial& = default;

        auto BindShader() -> void;
        static auto UnbindShader() -> void;

        auto BindTexture() -> void;
        auto UnbindTexture() -> void;

        auto UploadShaders(const Path_T& vertexShader, const Path_T& fragmentShader) -> void;

        KT_NODISCARD auto GetShader() -> std::shared_ptr<OpenGLShader> { return m_DefaultVertexPixelShaders; }
        KT_NODISCARD auto GetTexture() -> std::shared_ptr<OpenGLTexture2D> { return m_Texture; }

        auto SetTextureSampler(Int32_T value) -> void;
        auto SetTiltingColor(float red, float green, float blue, float alpha) -> void;
        auto SetTiltingColor(const glm::vec4& color) -> void;

        auto UploadUniformBuffersData() -> void;

        auto SetProjectionView(const glm::mat4& mat) -> void;
        auto SetTransform(const glm::mat4& mat) -> void;

    private:
        std::shared_ptr<OpenGLTexture2D> m_Texture{};
        std::shared_ptr<OpenGLShader> m_DefaultVertexPixelShaders{};

        glm::mat4 m_ProjectionView{};
        glm::mat4 m_Transform{};
        glm::vec4 m_Color{};
    };
}

#endif // MIKOTO_OPENGL_DEFAULT_MATERIAL_HH
