/**
 * @file RendererBackend.hh
 * @date 6/9/23
 * @author kate
 * */

#ifndef MIKOTO_RENDERER_API_HH
#define MIKOTO_RENDERER_API_HH

// C++ Standard Library
#include <any>
#include <memory>
#include <utility>

// Third-Party Libraries
#include <glm/glm.hpp>

// Project Headers
#include <Assets/Mesh.hh>
#include <Common/Common.hh>
#include <Material/Core/Material.hh>
#include <Material/Texture/TextureCubeMap.hh>
#include <Models/LightData.hh>
#include <Scene/Camera/SceneCamera.hh>
#include <Scene/Scene/Component.hh>

#define DISPLAY_NORMAL 1
#define DISPLAY_COLOR 2
#define DISPLAY_METAL 3
#define DISPLAY_AO 4
#define DISPLAY_ROUGH 5

#define MKT_SHADER_TRUE 1
#define MKT_SHADER_FALSE 0

namespace Mikoto {
    enum class RenderResolution {
        RENDER_RESOLUTION_HD,
        RENDER_RESOLUTION_FHD,
        RENDER_RESOLUTION_QHD,
        RENDER_RESOLUTION_UHD,
    };

    struct RendererCreateInfo {
        std::string_view Name{};

        UInt32_T ViewportWidth{};
        UInt32_T ViewportHeight{};

        GraphicsAPI Api{ GraphicsAPI::VULKAN_API };
    };

    struct EntityQueueInfo {
        TagComponent& Tag;
        RenderComponent& Render;
        MaterialComponent& Material;
        TransformComponent& Transform;
    };

    class RendererBackend {
    public:
        virtual ~RendererBackend() = default;

        // Initializes and shuts down the renderer
        virtual auto Init() -> bool = 0;
        virtual auto Shutdown() -> void = 0;

        // Handles per-frame rendering
        virtual auto BeginFrame() -> void = 0;
        virtual auto EndFrame() -> void = 0;

        // Queues a game object for rendering
        virtual auto RemoveFromDrawQueue( UInt64_T id ) -> bool = 0;
        virtual auto AddToDrawQueue( const EntityQueueInfo& queueInfo ) -> bool = 0;

        virtual auto RemoveLight( UInt64_T id ) -> bool = 0;
        virtual auto AddLight( UInt64_T id, const LightData& data, LightType activeType) -> bool = 0;

        virtual auto SetupCubeMap(const TextureCubeMap* cubeMap) -> void = 0;

        virtual auto SetRenderResolution(RenderResolution resolution) -> void = 0;
        virtual auto HasUpdatedResolution() const -> bool = 0;
        MKT_NODISCARD auto GetRenderResolution() const -> RenderResolution { return m_RenderResolution; }


        template<typename... Args>
        auto SetOutlineRenderColor( Args&&... args ) -> void {
            m_OutlineRenderColor = glm::vec4{ std::forward<Args>( args )... };
        }

        MKT_NODISCARD auto SetOutlineRenderWidth() const -> float { return m_OutlineRenderWidth; }

        auto SetOutlineRenderWidth(const float size) -> void {
            if (size > 1.0f) {
                m_OutlineRenderWidth = size;
            }
        }

        auto SetOutline(const bool value) -> void { m_OutlineEnable = value; }
        virtual auto SetOutlineRenderTargetEntity(UInt64_T id) -> void = 0;

        // Camera & Viewport
        template<typename... Args>
        auto SetProjection( Args &&...args ) -> void {
            m_Projection = glm::mat4{ std::forward<Args>( args )... };
        }

        auto SetCamera( const SceneCamera &camera ) -> void {
            m_Camera = std::addressof( camera );
        }

        virtual auto SetViewport( float x, float y, float width, float height ) -> void = 0;

        // Post-processing effects
        virtual auto EnableWireframe( bool enable ) -> void = 0;

        template<typename... Args>
        auto SetClearColor( Args&&... args ) -> void {
            m_ClearColor = glm::vec4{ std::forward<Args>( args )... };
        }

        virtual auto SetRenderMode( Size_T mode ) -> void = 0;

        // Factory method to create a renderer instance
        static auto Create( const RendererCreateInfo& createInfo ) -> Scope_T<RendererBackend>;

    protected:
        explicit RendererBackend( const std::string_view name)
            : m_Name{ name }
        {}

    protected:
        std::string m_Name{};

        glm::mat4 m_Projection{};
        glm::vec4 m_ClearColor{};

        bool m_OutlineEnable{};
        float m_OutlineRenderWidth{ 1.5f };
        glm::vec4 m_OutlineRenderColor{};

        bool m_UpdatedResolution{ false };

        RenderResolution m_RenderResolution{ RenderResolution::RENDER_RESOLUTION_FHD };

        const SceneCamera* m_Camera{ nullptr };

    };
}// namespace Mikoto

#endif// MIKOTO_RENDERER_API_HH