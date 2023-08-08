//
// Created by kate on 6/24/23.
//

#ifndef KATE_ENGINE_SCENE_HH
#define KATE_ENGINE_SCENE_HH

#include <entt/entt.hpp>

#include <Utility/Common.hh>

#include <Core/Assert.hh>
#include <Core/Logger.hh>

#include <Scene/Component.hh>

namespace kaTe {
    // Forward declare Entity to not engine it. Temporary
    class Entity;

    class Scene {
    public:
        explicit Scene();
        ~Scene() = default;

        auto OnUpdate() -> void;

        // Should only construct entities from this function
        KT_NODISCARD static auto CreateEntity(std::string_view entityNameTag, std::shared_ptr<Scene> scene) -> Entity;
        auto DestroyEntity(Entity& entity) -> void;

        auto OnViewPortResize(UInt32_T width, UInt32_T height) -> void;

    private:
        friend class Entity;

        friend class HierarchyPanel;
        friend class InspectorPanel;
        friend class ScenePanel;

        entt::registry m_Registry{};

        UInt32_T m_ViewportWidth{};
        UInt32_T m_ViewportHeight{};
    };
}



#endif//KATE_ENGINE_SCENE_HH
