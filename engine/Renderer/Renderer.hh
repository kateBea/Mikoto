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
    class Renderer {
    public:
        static auto Init() -> void;
        static auto ShutDown() -> void;

        static auto BeginScene(const ScenePrepareData& prepareData) -> void;
        static auto EndScene() -> void;

        static auto Submit(std::shared_ptr<DrawData> data) -> void;
        static auto Submit(const SceneObjectData& objectData, const glm::mat4& transform, std::shared_ptr<Material> material) -> void;
        static auto SubmitQuad(const glm::mat4& transform, const glm::vec4& color, std::shared_ptr<Material> material) -> void;

        static auto Flush() -> void;
        static auto OnEvent(Event &event) -> void;

        MKT_NODISCARD static auto GetActiveGraphicsAPI() -> GraphicsAPI { return s_ActiveAPI;  }
        MKT_NODISCARD static auto GetActiveGraphicsAPIPtr() -> RendererAPI* { return s_ActiveRendererAPI;  }

        MKT_NODISCARD static auto QueryDrawCallsCount() -> UInt32_T { return s_SavedSceneStats->GetDrawCallsCount(); }
        MKT_NODISCARD static auto QueryQuadCount() -> UInt32_T { return s_SavedSceneStats->GetQuadCount(); }
        MKT_NODISCARD static auto QueryIndexCount() -> UInt32_T { return s_SavedSceneStats->GetIndexCount(); }
        MKT_NODISCARD static auto QueryVertexCount() -> UInt32_T { return s_SavedSceneStats->GetVertexCount(); }

    private:
        /*************************************************************
        * HELPERS
        * ***********************************************************/
        static auto PickGraphicsAPI() -> void;
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
        // States the active Graphics Rendering API for the current window.
        // For the time being, we only have one main window, therefore, this attribute is going
        // to be static. In case we want to try different API at runtime, we may
        // have more than one Renderer API specific active
        inline static GraphicsAPI s_ActiveAPI{ GraphicsAPI::OPENGL_API };

        inline static RendererAPI* s_ActiveRendererAPI{ nullptr };

        inline static std::unique_ptr<RendererDrawData> s_DrawData{};
        inline static std::unordered_map<std::string, PrefabData> s_Prefabs{};

        inline static std::unique_ptr<RenderingStats>   s_RenderingStats{};
        inline static std::unique_ptr<RenderingStats>   s_SavedSceneStats{};
    };
}

#endif // MIKOTO_RENDERER_HH
