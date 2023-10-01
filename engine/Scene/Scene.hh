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
#include "Renderer/Camera/EditorCamera.hh"
#include <Core/Assert.hh>
#include <Core/Logger.hh>
#include <Scene/Component.hh>
#include <Utility/Common.hh>
#include <Utility/Types.hh>

namespace Mikoto {
    /**
     * Forward declaration for return type and avoid
     * header include dependency problems
     * */
    class Entity;

    class Scene {
    public:
        explicit Scene() = default;

        auto OnUpdate(double ts) -> void;
        auto DestroyEntity(Entity& entity) -> void;
        auto OnEditorUpdate(double timeStep, const EditorCamera &camera) -> void;
        auto OnViewPortResize(UInt32_T width, UInt32_T height) -> void;

        auto GetRegistry() -> entt::registry& { return m_Registry; }
        MKT_NODISCARD auto GetName() const -> const std::string& { return m_Name; }

        auto SetName(const std::string& name) { m_Name = name; }


        MKT_NODISCARD static auto CreateEmptyObject(std::string_view tagName, const std::shared_ptr<Scene>& scene) -> Entity;
        MKT_NODISCARD static auto CreatePrefabObject(std::string_view tagName, const std::shared_ptr<Scene>& scene, PrefabSceneObject type) -> Entity;

        ~Scene() = default;
    private:
        friend class Entity;
        friend class ScenePanel;
        friend class HierarchyPanel;
        friend class InspectorPanel;

    private:
        auto UpdateScripts() -> void;

        static constexpr glm::vec3 ENTITY_INITIAL_SIZE{ 1.0f, 1.0f, 1.0f };
        static constexpr glm::vec3 ENTITY_INITIAL_POSITION{ 0.0, 0.0, 0.0 };
        static constexpr glm::vec3 ENTITY_INITIAL_ROTATION{ 0.0f, 0.0f, 0.0f };

    private:
        entt::registry m_Registry{};
        UInt32_T m_ViewportWidth{};
        UInt32_T m_ViewportHeight{};
        std::string m_Name{ "Mikoto" };
    };
}



#endif // MIKOTO_SCENE_HH
