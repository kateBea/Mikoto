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
#include <Common/Common.hh>
#include <Assets/Mesh.hh>
#include <Material/Core/Material.hh>
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
    struct RendererCreateInfo {
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
        glm::mat4 m_Projection{};
        glm::vec4 m_ClearColor{};

        const SceneCamera* m_Camera{ nullptr };

    };
}// namespace Mikoto

#endif// MIKOTO_RENDERER_API_HH