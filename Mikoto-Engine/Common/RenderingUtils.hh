/**
 * RenderingUtilities.hh
 * Created by kate on 7/21/2023.
 * */

#ifndef MIKOTO_RENDERING_UTILITIES_HH
#define MIKOTO_RENDERING_UTILITIES_HH

// C++ Standard Library
#include <memory>

// Third-Party Libraries
#include "glm/glm.hpp"

// Projects Headers
#include "Common/Common.hh"
#include "Common/Types.hh"
#include "EditorCamera.hh"
#include "Renderer/Buffers/IndexBuffer.hh"
#include "Renderer/Buffers/VertexBuffer.hh"
#include "Renderer/Material/Material.hh"
#include "Renderer/Model.hh"
#include "Scene/Camera.hh"
#include "Scene/SceneCamera.hh"

namespace Mikoto {
    struct ScenePrepareData {
        const SceneCamera* RuntimeCamera{};
        const EditorCamera* StaticCamera{};
    };

    enum BufferBit {
        COLOR_BUFFER_BIT = BIT_SET(1),
        DEPTH_BUFFER_BIT = BIT_SET(2),
    };

    struct UniformTransformData {
        glm::mat4 View{};
        glm::mat4 Projection{};
        glm::mat4 Transform{};
    };

    enum class LightType {
        DIRECTIONAL_LIGHT_TYPE  = 0,
        POINT_LIGHT_TYPE        = 1,
        SPOT_LIGHT_TYPE         = 2,
    };

    enum class PrefabSceneObject {
        NO_PREFAB_OBJECT,
        SPRITE_PREFAB_OBJECT,
        CUBE_PREFAB_OBJECT,
        CONE_PREFAB_OBJECT,
        CYLINDER_PREFAB_OBJECT,
        SPHERE_PREFAB_OBJECT,
        SPONZA_PREFAB_OBJECT,
        COUNT_PREFAB_OBJECT,
    };

    struct MeshMetaData {
        const Mesh* ModelMesh{};

        // Temporary. Assumes a mesh can only have one material
        std::shared_ptr<Material> MeshMaterial{};
    };

    struct DirectionalLight {
        // if Direction.w == 1, we use the lights position
        // to compute the rays directions
        glm::vec4 Direction{ 1.0f, 1.0f, 1.0f, 0.0f };
        glm::vec4 Position{ 1.0f, 1.0f, 1.0f, 1.0f };

        glm::vec4 Ambient{ 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec4 Diffuse{ 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec4 Specular{ 1.0f, 1.0f, 1.0f, 1.0f };
    };

    // Point lights for now
    struct PointLight {
        glm::vec4 Position{ 0.0f, 0.0f, 0.0f, 1.0f };

        glm::vec4 Ambient{ 1.0f, 1.0f, 1.0f, 0.1f };
        glm::vec4 Diffuse{ 1.0f, 1.0f, 1.0f, 0.1f };
        glm::vec4 Specular{ 1.0f, 1.0f, 1.0f, 0.1f };

        // Using vec4s for now to avoid dealing with alignment issues for now
        // x=constant, y=linear, y=quadratic, w=range
        glm::vec4 Components{ 1.0f, 0.09f, 0.032f, 0.1f };

        // x=ambient, rest unused for now
        glm::vec4 Intensity{};
    };

    struct SpotLight {
        glm::vec4 Position{};
        glm::vec4 Direction{ 0.0f, -1.0f, 0.0f, 0.0f }; // facing down by default

        glm::vec4 Ambient{ 1.0f, 1.0f, 1.0f, 0.1f };
        glm::vec4 Diffuse{ 1.0f, 1.0f, 1.0f, 0.1f };
        glm::vec4 Specular{ 1.0f, 1.0f, 1.0f, 0.1f };

        // cutoff
        // x=cutOff, y=outerCutOff (both angles in radians), rest unused
        glm::vec4 CutOffValues{};

        // Components
        // x=constant, y=linear, z=quadratic, w=unused
        // Used for now for simplicity for proper alignment
        glm::vec4 Components{ 1.0f, 0.7f, 1.8f, 0.0f };
    };

    struct DrawData {
        // Mesh details
        const std::vector<MeshMetaData>* MeshMeta{};

        // The transform of this object applies to all of its meshes for now
        UniformTransformData TransformData{};

        // We have a single color for the whole model
        glm::vec4 Color{};

        // [Light info]
        PointLight LightInfo{};
        glm::vec4 ViewPosition{};
    };

    struct SceneObjectData {
        // Mesh details
        std::vector<MeshMetaData> MeshMeta{};

        // Objects Color
        glm::vec4 Color{};

        // Type of prefab
        PrefabSceneObject PrefabType{ PrefabSceneObject::NO_PREFAB_OBJECT };

        // If it is prefab, PrefabType contains
        // the type otherwise PrefabType is NO_PREFAB_OBJECT
        bool IsPrefab{};

        // If the mesh is not a prefab, we will have its path and name
        Path_T ModelPath{};
        std::string ModelName{};

        // For usage in the inspector panel. It tells which mesh is currently selected
        // No mesh is selected until we actually do so, hence why it's initialized to -1
        Int32_T MeshSelectedIndex{ NO_MESH_SELECTED_INDEX };

        // Index indicating no mesh is yet active for editing
        static constexpr auto NO_MESH_SELECTED_INDEX{ -1 };


        // [Lighting]
        // just one for now
        PointLight Light{};
    };

    /**
     * @brief Holds light related information.
     * Contains the relevant data specific for the three types of light:
     * directional light, spot light, point light. Used to store light informatio
     * in the light component.
     * */
    struct LightData {
        // Directional light
        DirectionalLight DireLightData{};

        // Point light
        PointLight PointLightDat{};

        // Spotlight
        SpotLight SpotLightData{};
    };

    /**
     * Returns the string representing the name of a given prefab type
     * @returns string representation of a prefab type
     * */
    inline constexpr auto PrefabTypeStr(PrefabSceneObject type) -> std::string_view {
        switch (type) {
            case PrefabSceneObject::NO_PREFAB_OBJECT:           return "NO_PREFAB_OBJECT";
            case PrefabSceneObject::SPRITE_PREFAB_OBJECT:       return "SPRITE_PREFAB_OBJECT";
            case PrefabSceneObject::CUBE_PREFAB_OBJECT:         return "CUBE_PREFAB_OBJECT";
            case PrefabSceneObject::CONE_PREFAB_OBJECT:         return "CONE_PREFAB_OBJECT";
            case PrefabSceneObject::CYLINDER_PREFAB_OBJECT:     return "CYLINDER_PREFAB_OBJECT";
            case PrefabSceneObject::SPHERE_PREFAB_OBJECT:       return "SPHERE_PREFAB_OBJECT";
            case PrefabSceneObject::SPONZA_PREFAB_OBJECT:       return "SPONZA_PREFAB_OBJECT";
            case PrefabSceneObject::COUNT_PREFAB_OBJECT:        return "COUNT_PREFAB_OBJECT";
        }
    }

    /**
     * Returns the PrefabSceneObject representing the given prefab name. This function is mainly
     * useful for scene objects serialization in conjunction with PrefabTypeStr(PrefabSceneObject)
     * @returns PrefabSceneObject representation of a prefab name
     * */
    inline auto PrefabTypeFromName(std::string_view name) -> PrefabSceneObject {
        if (name == PrefabTypeStr(PrefabSceneObject::NO_PREFAB_OBJECT))         return PrefabSceneObject::NO_PREFAB_OBJECT;
        if (name == PrefabTypeStr(PrefabSceneObject::SPRITE_PREFAB_OBJECT))     return PrefabSceneObject::SPRITE_PREFAB_OBJECT;
        if (name == PrefabTypeStr(PrefabSceneObject::CUBE_PREFAB_OBJECT))       return PrefabSceneObject::CUBE_PREFAB_OBJECT;
        if (name == PrefabTypeStr(PrefabSceneObject::CONE_PREFAB_OBJECT))       return PrefabSceneObject::CONE_PREFAB_OBJECT;
        if (name == PrefabTypeStr(PrefabSceneObject::CYLINDER_PREFAB_OBJECT))   return PrefabSceneObject::CYLINDER_PREFAB_OBJECT;
        if (name == PrefabTypeStr(PrefabSceneObject::SPHERE_PREFAB_OBJECT))     return PrefabSceneObject::SPHERE_PREFAB_OBJECT;
        if (name == PrefabTypeStr(PrefabSceneObject::SPONZA_PREFAB_OBJECT))     return PrefabSceneObject::SPONZA_PREFAB_OBJECT;
        if (name == PrefabTypeStr(PrefabSceneObject::COUNT_PREFAB_OBJECT))      return PrefabSceneObject::COUNT_PREFAB_OBJECT;
    }

    enum class GraphicsAPI {
        OPENGL_API,
        VULKAN_API,
    };

    struct RenderingStats {
        explicit RenderingStats() = default;
        RenderingStats(const RenderingStats& other) = default;
        auto operator=(const RenderingStats& other) -> RenderingStats& = default;

        MKT_NODISCARD auto GetDrawCallsCount() const -> UInt64_T { return m_DrawCallsCount; }


        MKT_NODISCARD auto GetVertexCount() const -> UInt64_T { return m_VertexCount; }
        MKT_NODISCARD auto GetIndexCount() const -> UInt64_T { return m_IndexCount; }
        MKT_NODISCARD auto GetDrawCallCount() const -> UInt64_T { return m_DrawCallsCount; }

        MKT_NODISCARD auto GetModelsCount() const -> UInt64_T { return m_Models; }
        MKT_NODISCARD auto GetMeshesCount() const -> UInt64_T { return m_Meshes; }
        MKT_NODISCARD auto GetObjectsCount() const -> UInt64_T { return m_Objects; }
        MKT_NODISCARD auto GetSceneCamerasCount() const -> UInt64_T { return m_Cameras; }

        /**
         * Increments the total number of draw calls by the given amount
         * @param value increment value
         * */
        auto IncrementDrawCallCount(UInt64_T value) { m_DrawCallsCount += value; }

        /**
         * Increments the total number of indices by the given amount
         * @param value increment value
         * */
        auto IncrementIndexCount(UInt64_T value) { m_IndexCount += value; }

        /**
         * Increments the total number of vertices by the given amount
         * @param value increment value
         * */
        auto IncrementVertexCount(UInt64_T value) { m_VertexCount += value; }

        auto IncrementModelsCount(UInt64_T value) -> void { m_Models += value; }
        auto IncrementMeshesCount(UInt64_T value) -> void { m_Meshes += value; }
        auto IncrementObjectsCount(UInt64_T value) -> void { m_Objects += value; }
        auto IncrementSceneCamerasCount(UInt64_T value) -> void { m_Cameras += value; }

        /**
         * Sets the 0 the counters for vertices, indices and draw calls
         * */
        auto Reset() -> void {
            m_DrawCallsCount = 0;
            m_IndexCount = 0;
            m_VertexCount = 0;

            m_Models = 0;
            m_Meshes = 0;
            m_Objects = 0;
            m_Cameras = 0;
        }

    private:
        UInt64_T m_DrawCallsCount{};
        UInt64_T m_IndexCount{};
        UInt64_T m_VertexCount{};

        UInt64_T m_Models{};
        UInt64_T m_Meshes{};
        UInt64_T m_Objects{};
        UInt64_T m_Cameras{};
    };

    struct RendererDrawData {
        const SceneCamera* SceneRuntimeCamera{};
        const EditorCamera* SceneEditCamera{};
    };
}

#endif // MIKOTO_RENDERING_UTILITIES_HH
