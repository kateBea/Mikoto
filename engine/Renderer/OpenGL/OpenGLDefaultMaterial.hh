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

        auto UploadShaders(const Path_T& vertexShader, const Path_T& fragmentShader) -> void;

        MKT_NODISCARD auto GetShader() -> std::shared_ptr<OpenGLShader> { return m_DefaultVertexPixelShaders; }
        /**
         * This number tells how many textures can be bound to sample from. This is a device
         * dependent property some allow to bind textures to up to 32 different slots.
         * */
        MKT_NODISCARD static auto GetMaxConcurrentSamplingTextures() -> UInt32_T { return 16; }

        auto SetTextureSampler(Int32_T value) -> void;
        auto SetTiltingColor(float red, float green, float blue, float alpha) -> void;
        auto SetTiltingColor(const glm::vec4& color) -> void;

        auto UploadUniformBuffersData() -> void;

        auto SetProjection(const glm::mat4& mat) -> void { m_ProjectionMatrix = mat; }
        auto SetView(const glm::mat4& mat) -> void { m_ViewMatrix = mat; }
        auto SetTransform(const glm::mat4& mat) -> void;

    private:
        std::shared_ptr<OpenGLShader> m_DefaultVertexPixelShaders{};

        // Camera
        glm::mat4 m_ProjectionMatrix{};
        glm::mat4 m_ViewMatrix{};

        // Object
        glm::mat4 m_Transform{};
        glm::vec4 m_Color{};
    };
}

#endif // MIKOTO_OPENGL_DEFAULT_MATERIAL_HH
