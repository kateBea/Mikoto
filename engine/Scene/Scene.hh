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
#include <glm/glm.hpp>

// Project Headers
#include <Utility/Types.hh>
#include <Utility/Common.hh>
#include <Utility/Random.hh>
#include <Renderer/RenderingUtilities.hh>
#include <Scene/Camera/EditorCamera.hh>

namespace Mikoto {
    class Entity;

    class Scene {
    public:
        explicit Scene(std::string_view name = "Mikoto")
            :   m_Name{ name }
        {

        }

        Scene(Scene&& other) = default;
        auto operator=(Scene&& other) -> Scene& = default;

        auto operator==(const Scene& other) noexcept -> bool { return m_Name == other.m_Name; }

        auto OnRuntimeUpdate(double ts) -> void;
        auto DestroyEntity(Entity& entity) -> void;
        auto OnEditorUpdate(double timeStep, const EditorCamera &camera) -> void;
        auto OnViewPortResize(UInt32_T width, UInt32_T height) -> void;

        auto GetRegistry() -> entt::registry& { return m_Registry; }
        auto GetRegistry() const -> const entt::registry& { return m_Registry; }

        MKT_NODISCARD auto GetName() const -> const std::string& { return m_Name; }

        auto AddEmptyObject(std::string_view tagName, UInt64_T guid = Random::GUID::GenerateGUID()) -> Entity;
        auto AddPrefabObject(std::string_view tagName, PrefabSceneObject type, UInt64_T guid = Random::GUID::GenerateGUID()) -> Entity;

        MKT_NODISCARD static auto GetActiveScene() -> Scene*;
        static auto SetActiveScene(Scene *scene) -> void;

        auto Clear() -> void;

        ~Scene();
    private:
        friend class Entity;
        friend class ScenePanel;
        friend class HierarchyPanel;
        friend class InspectorPanel;

        static inline Scene* s_ActiveScene{ nullptr };

    private:
        auto UpdateScripts() -> void;

        static constexpr glm::vec3 ENTITY_INITIAL_SIZE{ 1.0f, 1.0f, 1.0f };
        static constexpr glm::vec3 ENTITY_INITIAL_POSITION{ 0.0, 0.0, 0.0 };
        static constexpr glm::vec3 ENTITY_INITIAL_ROTATION{ 0.0f, 0.0f, 0.0f };

    private:
        std::string m_Name{};
        UInt32_T m_ViewportWidth{};
        UInt32_T m_ViewportHeight{};
        entt::registry m_Registry{};
    };
}

#endif // MIKOTO_SCENE_HH
