/**
 * Renderer.hh
 * Created by kate on 6/5/23.
 * */

#ifndef MIKOTO_RENDERER_HH
#define MIKOTO_RENDERER_HH

// C++ Standard Library
#include <memory>
#include <unordered_map>

// Third-Party Libraries
#include <glm/glm.hpp>

// Project headers
#include <Utility/Common.hh>
#include <Renderer/RendererAPI.hh>
#include <Renderer/Camera/Camera.hh>
#include <Renderer/Material/Material.hh>
#include <Renderer/RenderingUtilities.hh>
#include <Renderer/Buffers/IndexBuffer.hh>
#include <Renderer/Buffers/VertexBuffer.hh>

namespace Mikoto {
    struct RendererSpec{
        GraphicsAPI Backend{};
    };

    class Renderer {
    public:
        static auto Init(RendererSpec&& spec) -> void;
        static auto ShutDown() -> void;

        static auto BeginScene(const ScenePrepareData& prepareData) -> void;
        static auto EndScene() -> void;

        static auto Submit(std::shared_ptr<DrawData> data) -> void;
        static auto Submit(const SceneObjectData& objectData, const glm::mat4& transform, std::shared_ptr<Material> material) -> void;
        MKT_UNUSED_FUNC static auto SubmitQuad(const glm::mat4& transform, const glm::vec4& color, std::shared_ptr<Material> material) -> void;

        static auto Flush() -> void;
        static auto OnEvent(Event& event) -> void;

        MKT_NODISCARD static auto GetActiveGraphicsAPI() -> GraphicsAPI { return s_ActiveAPI;  }
        MKT_NODISCARD static auto GetActiveGraphicsAPIPtr() -> RendererAPI* { return s_ActiveRendererAPI;  }

        MKT_NODISCARD static auto QueryDrawCallsCount() -> UInt64_T { return s_SavedSceneStats->GetDrawCallsCount(); }
        MKT_NODISCARD static auto QueryIndexCount() -> UInt64_T { return s_SavedSceneStats->GetIndexCount(); }
        MKT_NODISCARD static auto QueryVertexCount() -> UInt64_T { return s_SavedSceneStats->GetVertexCount(); }

    private:
        /*************************************************************
        * HELPERS
        * ***********************************************************/
        static auto LoadPrefabs() -> void;

        static auto AddSpritePrefab() -> void;
        static auto AddCubePrefab() -> void;
        static auto AddSpherePrefab() -> void;
        static auto AddCylinderPrefab() -> void;
        static auto AddConePrefab() -> void;
        static auto AddSponzaPrefab() -> void;

        MKT_NODISCARD static auto GetSpritePrefabName() -> const std::string& { static std::string name{ "Sprite" }; return name; }
        MKT_NODISCARD static auto GetCubePrefabName() -> const std::string& { static std::string name{ "Cube" }; return name; }
        MKT_NODISCARD static auto GetSpherePrefabName() -> const std::string& { static std::string name{ "Sphere" }; return name; }
        MKT_NODISCARD static auto GetCylinderPrefabName() -> const std::string& { static std::string name{ "Cylinder" }; return name; }
        MKT_NODISCARD static auto GetConePrefabName() -> const std::string& { static std::string name{ "Cone" }; return name; }
        MKT_NODISCARD static auto GetSponzaPrefabName() -> const std::string& { static std::string name{ "Sponza" }; return name; }

    private:
        // Specifications for the renderer
        inline static RendererSpec s_Spec{};

        // Graphics API currently active
        inline static GraphicsAPI s_ActiveAPI{};

        // Pointer to the currently active graphics backend
        inline static RendererAPI* s_ActiveRendererAPI{};

        // List of prefab models
        inline static std::unordered_map<std::string, PrefabData> s_Prefabs{};

        // Contains the data necessary to prepare a scene for rendering
        inline static std::unique_ptr<RendererDrawData> s_DrawData{};

        // Keep track of rendering stats such as number f vertices, number of indices, etc.
        inline static std::unique_ptr<RenderingStats> s_RenderingStats{};
        inline static std::unique_ptr<RenderingStats> s_SavedSceneStats{};
    };
}

#endif // MIKOTO_RENDERER_HH
