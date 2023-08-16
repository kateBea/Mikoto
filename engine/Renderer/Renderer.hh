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
        static auto Flush() -> void;

        static auto OnEvent(Event &event) -> void;

        KT_NODISCARD static auto GetActiveGraphicsAPI() -> GraphicsAPI { return s_ActiveAPI;  }
        KT_NODISCARD static auto GetRendererAPIActive() -> RendererAPI* { return s_ActiveRendererAPI;  }

        /*************************************************************
        * FOR 2D DRAWING
        * ********************************************************+ */

        /**
         * Submits a Quad for drawing, expects the angle to be in degrees
         * */
        static auto SubmitQuad(const glm::vec3 &position, const glm::vec2 &size, const glm::vec4 &color, double angle) -> void;
        static auto SubmitQuad(const glm::vec3 &position, const glm::vec2 &size, const glm::vec4 &color, double angle, const std::shared_ptr<Texture> &texture) -> void;

        static auto SubmitQuad(const glm::mat4 &transform, const glm::vec4 &color) -> void;
        static auto SubmitQuad(const glm::mat4 &transform, const glm::vec4 &color, const std::shared_ptr<Texture> &texture) -> void;


        KT_NODISCARD static auto QueryDrawCallsCount() -> UInt32_T { return s_SavedSceneStats->GetDrawCallsCount(); }
        KT_NODISCARD static auto QueryQuadCount() -> UInt32_T { return s_SavedSceneStats->GetQuadCount(); }
        KT_NODISCARD static auto QueryIndexCount() -> UInt32_T { return s_SavedSceneStats->GetIndexCount(); }
        KT_NODISCARD static auto QueryVertexCount() -> UInt32_T { return s_SavedSceneStats->GetVertexCount(); }

    public:
        // Forbidden operations
        Renderer(const Renderer&) = delete;
        auto operator=(const Renderer&) -> Renderer& = delete;

        Renderer(Renderer&&) = delete;
        auto operator=(Renderer&&) -> Renderer& = delete;
    private:
        // States the active Graphics Rendering API for the current window.
        // For the time being, we only have one main window, therefore, this attribute is going
        // to be static. In case we want to try different API at runtime, we may
        // have more than one Renderer API specific active
        inline static GraphicsAPI s_ActiveAPI{ GraphicsAPI::VULKAN_API };

        static auto PickGraphicsAPI() -> void;
    private:
        // For 2D rendering
        struct RenderingStats {
            explicit RenderingStats() = default;
            RenderingStats(const RenderingStats& other) = default;

            KT_NODISCARD auto GetQuadCount() const -> UInt32_T { return m_QuadCount; }
            KT_NODISCARD auto GetDrawCallsCount() const -> UInt32_T { return m_DrawCallsCount; }

            // This value is fixed since all quads need 4 vertices at most
            KT_NODISCARD auto GetVertexCount() const -> UInt32_T { return m_QuadCount * 4; }
            // This value is fixed since all quads need 6 indices at most
            KT_NODISCARD auto GetIndexCount() const -> UInt32_T { return m_QuadCount * 6; }

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
            std::shared_ptr<Camera> SceneCamera{};
        };

    private:
        inline static RendererAPI* s_ActiveRendererAPI{ nullptr };

        inline static std::unique_ptr<RendererDrawData> s_DrawData{};
        inline static std::unique_ptr<RendererDrawData> s_QuadData{};

        inline static std::unique_ptr<RenderingStats>   s_RenderingStats{};
        inline static std::unique_ptr<RenderingStats>   s_SavedSceneStats{};
    };
}

#endif // MIKOTO_RENDERER_HH
