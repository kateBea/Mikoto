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
#include <Renderer/Buffers/IndexBuffer.hh>
#include <Renderer/Buffers/VertexBuffer.hh>
#include <Renderer/Camera/Camera.hh>
#include <Renderer/Material/Material.hh>
#include <Renderer/Model.hh>
#include <Scene/EditorCamera.hh>
#include <Scene/SceneCamera.hh>

namespace Mikoto {
    struct ScenePrepareData {
        SceneCamera* RuntimeCamera{};
        EditorCamera* StaticCamera{};
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
        std::shared_ptr<Camera> CameraData{};
        std::shared_ptr<Material> MaterialData{};
        UniformTransformData TransformData{};
        glm::vec4 Color{};
    };

    enum class PrefabSceneObject {
        NO_PREFAB_OBJECT,
        SPRITE_PREFAB_OBJECT,
        CUBE_PREFAB_OBJECT,
        CONE_PREFAB_OBJECT,
        CYLINDER_PREFAB_OBJECT,
        SPHERE_PREFAB_OBJECT,
        COUNT_PREFAB_OBJECT,
    };

    struct SceneObjectData {
        std::shared_ptr<Model> ModelData{};
        glm::vec4 Color{};
        PrefabSceneObject PrefabType{ PrefabSceneObject::NO_PREFAB_OBJECT };
        bool IsPrefab{}; // If it is prefab, PrefabType contains the type otherwise PrefabType is NO_PREFAB_OBJECT
    };

    struct PrefabData {
        std::shared_ptr<ModelPrefab> ModelData{};
        UniformTransformData TransformData{};
    };

    enum class GraphicsAPI {
        OPENGL_API,
        VULKAN_API,
    };

    struct RenderingStats {
        explicit RenderingStats() = default;
        RenderingStats(const RenderingStats& other) = default;

        MKT_NODISCARD auto GetQuadCount() const -> UInt32_T { return m_QuadCount; }
        MKT_NODISCARD auto GetDrawCallsCount() const -> UInt32_T { return m_DrawCallsCount; }

        // This value is fixed since all quads need four vertices at most
        MKT_NODISCARD auto GetVertexCount() const -> UInt32_T { return m_QuadCount * 4; }
        // This value is fixed since all quads need six indices at most
        MKT_NODISCARD auto GetIndexCount() const -> UInt32_T { return m_QuadCount * 6; }

        auto IncrementDrawCallCount(UInt32_T value) { m_DrawCallsCount += value; }
        auto IncrementQuadCount(UInt32_T value) { m_QuadCount += value; }

        auto Reset() -> void {
            m_DrawCallsCount = 0;
            m_QuadCount = 0;
        }

        UInt32_T m_DrawCallsCount{};
        UInt32_T m_QuadCount{};
    };

    struct RendererDrawData {
        std::shared_ptr<VertexBuffer> VertexBufferData{};
        std::shared_ptr<IndexBuffer> IndexBufferData{};
        SceneCamera* SceneRuntimeCamera{};
        EditorCamera* SceneEditCamera{};
    };
}

#endif // MIKOTO_RENDERING_UTILITIES_HH
