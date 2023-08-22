/**
* Renderer.hh
* Created by kate on 6/5/23.
* */

#ifndef MIKOTO_RENDERER_HH
#define MIKOTO_RENDERER_HH

// C++ Standard Library
#include <memory>

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

        static auto Submit(const DrawData & data) -> void;
        static auto SubmitQuad(const glm::mat4 &transform, const glm::vec4 &color, std::shared_ptr<Material> material) -> void;

        static auto Flush() -> void;
        static auto OnEvent(Event &event) -> void;

        MKT_NODISCARD static auto GetActiveGraphicsAPI() -> GraphicsAPI { return s_ActiveAPI;  }
        MKT_NODISCARD static auto GetActiveGraphicsAPIPtr() -> RendererAPI* { return s_ActiveRendererAPI;  }

        MKT_NODISCARD static auto QueryDrawCallsCount() -> UInt32_T { return s_SavedSceneStats->GetDrawCallsCount(); }
        MKT_NODISCARD static auto QueryQuadCount() -> UInt32_T { return s_SavedSceneStats->GetQuadCount(); }
        MKT_NODISCARD static auto QueryIndexCount() -> UInt32_T { return s_SavedSceneStats->GetIndexCount(); }
        MKT_NODISCARD static auto QueryVertexCount() -> UInt32_T { return s_SavedSceneStats->GetVertexCount(); }

    public:
        /*************************************************************
        * DELETED OPERATIONS
        * ***********************************************************/
        Renderer(const Renderer&) = delete;
        auto operator=(const Renderer&) -> Renderer& = delete;

        Renderer(Renderer&&) = delete;
        auto operator=(Renderer&&) -> Renderer& = delete;
    private:
        /*************************************************************
        * HELPERS
        * ***********************************************************/
        static auto PickGraphicsAPI() -> void;

    private:
        // States the active Graphics Rendering API for the current window.
        // For the time being, we only have one main window, therefore, this attribute is going
        // to be static. In case we want to try different API at runtime, we may
        // have more than one Renderer API specific active
        inline static GraphicsAPI s_ActiveAPI{ GraphicsAPI::VULKAN_API };

        inline static RendererAPI* s_ActiveRendererAPI{ nullptr };

        inline static std::unique_ptr<RendererDrawData> s_DrawData{};
        inline static std::unique_ptr<RendererDrawData> s_QuadData{};

        inline static std::unique_ptr<RenderingStats>   s_RenderingStats{};
        inline static std::unique_ptr<RenderingStats>   s_SavedSceneStats{};
    };
}

#endif // MIKOTO_RENDERER_HH
