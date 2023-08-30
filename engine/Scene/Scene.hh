/**
 * Scene.hh
 * Created by kate on 6/24/23.
 * */

#ifndef MIKOTO_SCENE_HH
#define MIKOTO_SCENE_HH

// C++ Standard Library
#include <memory>
#include <string_view>

// Third-Party Libraries
#include <entt/entt.hpp>

// Project Headers
#include <Utility/Common.hh>
#include <Utility/Types.hh>
#include <Core/Assert.hh>
#include <Core/Logger.hh>
#include <Scene/Component.hh>
#include <Scene/EditorCamera.hh>

namespace Mikoto {
    class Entity;

    class Scene {
    public:
        explicit Scene();

        auto OnUpdate(double ts) -> void;
        auto DestroyEntity(Entity& entity) -> void;
        auto OnEditorUpdate(double ts, EditorCamera& camera) -> void;
        auto OnViewPortResize(UInt32_T width, UInt32_T height) -> void;

        MKT_NODISCARD static auto CreateEmptyObject(std::string_view tagName, const std::shared_ptr<Scene>& scene) -> Entity;
        MKT_NODISCARD static auto CreatePrefabObject(std::string_view tagName, const std::shared_ptr<Scene>& scene) -> Entity;

        ~Scene() = default;
    private:
        friend class Entity;
        friend class ScenePanel;
        friend class HierarchyPanel;
        friend class InspectorPanel;

    private:
        entt::registry m_Registry{};
        UInt32_T m_ViewportWidth{};
        UInt32_T m_ViewportHeight{};
    };
}



#endif // MIKOTO_SCENE_HH
