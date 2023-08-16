/**
 * RenderingUtilities.hh
 * Created by kate on 7/21/2023.
 * */

#ifndef MIKOTO_RENDERING_UTILITIES_HH
#define MIKOTO_RENDERING_UTILITIES_HH

// C++ Standard Library
#include <memory>

// Third-Party Libraries
#include <glm/glm.hpp>

// Projects Headers
#include <Renderer/Model.hh>
#include <Renderer/Material/Material.hh>
#include <Renderer/Material/Texture.hh>
#include <Renderer/Buffers/IndexBuffer.hh>
#include <Renderer/Buffers/VertexBuffer.hh>
#include <Renderer/Camera/Camera.hh>

namespace Mikoto {
    struct ScenePrepareData {
        std::shared_ptr<Camera> SceneCamera{};
    };

    enum BufferBit {
        COLOR_BUFFER_BIT = BIT_SET(1),
        DEPTH_BUFFER_BIT = BIT_SET(2),
    };

    struct UniformTransformData {
        glm::mat4 ProjectionView{};
        glm::mat4 Transform{};
    };

    struct DrawData {
        std::shared_ptr<Model> ModelData{};
        std::shared_ptr<VertexBuffer> VertexBufferData{};
        std::shared_ptr<IndexBuffer> IndexBufferData{};
        std::shared_ptr<Material> MaterialData{};
        std::shared_ptr<Texture> TextureData{};
        std::shared_ptr<Camera> Camera{};
        UniformTransformData TransformData{};
        glm::vec4 Color{};
    };

    enum class GraphicsAPI {
        OPENGL_API,
        VULKAN_API,
    };
}

#endif // MIKOTO_RENDERING_UTILITIES_HH
