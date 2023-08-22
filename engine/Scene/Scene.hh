/**
 * Scene.hh
 * Created by kate on 6/24/23.
 * */

#ifndef MIKOTO_SCENE_HH
#define MIKOTO_SCENE_HH

// C++ Standard Library
#include <memory>

// Third-Party Libraries
#include <entt/entt.hpp>

// Project Headers
#include <Utility/Common.hh>
#include <Core/Assert.hh>
#include <Core/Logger.hh>
#include <Scene/Component.hh>

namespace Mikoto {
    class Entity;

    class Scene {
    public:
        explicit Scene();

        auto OnUpdate() -> void;

        MKT_NODISCARD static auto CreateEntity(std::string_view tagName, const std::shared_ptr<Scene>& scene) -> Entity;
        auto DestroyEntity(Entity& entity) -> void;

        auto OnViewPortResize(UInt32_T width, UInt32_T height) -> void;

        ~Scene() = default;
    private:
        friend class Entity;
        friend class HierarchyPanel;
        friend class InspectorPanel;
        friend class ScenePanel;

    private:
        entt::registry m_Registry{};

        UInt32_T m_ViewportWidth{};
        UInt32_T m_ViewportHeight{};
    };
}



#endif // MIKOTO_SCENE_HH
