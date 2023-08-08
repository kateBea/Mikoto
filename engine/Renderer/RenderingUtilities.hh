/**
 * RenderingUtilities.hh
 * Created by kate on 7/21/2023.
 * */

#ifndef KATE_ENGINE_RENDERING_UTILITIES_HH
#define KATE_ENGINE_RENDERING_UTILITIES_HH

// C++ Standard Library
#include <memory>

// Third-Party Libraries
#include <glm/glm.hpp>

// Projects Headers
#include <Renderer/Material/Material.hh>
#include <Renderer/Material/Texture.hh>
#include <Renderer/Buffers/IndexBuffer.hh>
#include <Renderer/Buffers/VertexBuffer.hh>
#include <Renderer/Model.hh>

namespace kaTe {
    enum BufferBit {
        COLOR_BUFFER_BIT = BIT_SET(1),
        DEPTH_BUFFER_BIT = BIT_SET(2),
    };

    struct UniformTransformData {
        glm::mat4 ProjectionView{};
        glm::mat4 Transform{};
        glm::mat4 Projection{};
        glm::mat4 View{};
    };

    struct RenderingData {
        std::shared_ptr<VertexBuffer> VertexBufferData{};
        std::shared_ptr<IndexBuffer> IndexBufferData{};
        std::shared_ptr<Material> MaterialData{};
        std::shared_ptr<Texture> TextureData{};
        UniformTransformData TransformData{};
        glm::vec4 Color{};
    };

    // TODO: will eventually merge into RenderingData
    struct DrawData {
        std::shared_ptr<Model> ModelData{};
        std::shared_ptr<Material> Material{};

        glm::mat4 Model{};
        glm::mat4 View{};
        glm::mat4 Projection{};
    };
}

#endif//KATE_ENGINE_RENDERING_UTILITIES_HH
