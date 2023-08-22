/**
 * Entity.hh
 * Created by kate on 6/24/23.
 * */

#ifndef MIKOTO_ENTITY_HH
#define MIKOTO_ENTITY_HH

// C++ Standard Library
#include <utility>

// Third-Party Libraries
#include <entt/entt.hpp>

// Project Headers
#include <Utility/Common.hh>
#include <Core/Logger.hh>
#include <Scene/Scene.hh>
#include <Scene/Component.hh>

namespace Mikoto {
    class Entity {
    public:
        explicit Entity() = default;
        Entity(Entity&& other) noexcept = default;
        Entity(const Entity& other) = default;

        auto operator=(const Entity& other) -> Entity& = default;

        auto operator=(Entity&& other) noexcept -> Entity& {
            m_EntityHandle = std::move(other.m_EntityHandle);

            if (auto ptr{ other.m_Scene.lock() })
                m_Scene = ptr;
            else
                KATE_CORE_LOGGER_ERROR("Other entity's scene has expired and no longer exists! Failed on moving operation");
            return *this;
        }

        /**
         * Returns true if this entity contains all of the components
         * listed in the parameter pack
         * @returns true if this entity contains all of components of the parameter pack list
         * @tparam ComponentTypeList parameter pack containing all of the components to be checked
         * */
        template<typename... ComponentTypeList>
        MKT_NODISCARD auto HasAllComponents() -> bool {
            if (auto ptr{ m_Scene.lock() }) {
                return ptr->m_Registry.all_of<ComponentTypeList...>(m_EntityHandle);
            }

            KATE_CORE_LOGGER_ERROR("This entity's scene has expired and no longer exists!");
            return false;
        }

        /**
         * Returns true if this entity contains at least one of the components
         * listed in the parameter pack
         * @returns true if this entity contains at least one of components of the parameter pack list
         * @tparam ComponentTypeList parameter pack containing all of the components to be checked
         * */
        template<typename... ComponentTypeList>
        MKT_NODISCARD auto HasAnyOfComponents() -> bool {
            if (auto ptr{ m_Scene.lock() }) {
                return ptr->m_Registry.any_of<ComponentTypeList...>(m_EntityHandle);
            }

            KATE_CORE_LOGGER_ERROR("This entity's scene has expired and no longer exists!");
            return false;
        }

        /**
         * Returns true if this entity contains the given component
         * @returns true if this entity contains the given component
         * @tparam ComponentType component to be checked
         * */
        template<typename ComponentType>
        MKT_NODISCARD auto HasComponent() -> bool {
            if (auto ptr{ m_Scene.lock() }) {
                return ptr->m_Registry.all_of<ComponentType>(m_EntityHandle);
            }

            KATE_CORE_LOGGER_ERROR("This entity's scene has expired and no longer exists!");
            return false;
        }

        /**
         * Returns true if this entity is valid, false otherwise
         * @returns true if the implicit parameter is a valid entity
         * */
        MKT_NODISCARD auto IsValid() const -> bool { return m_EntityHandle != entt::null; }

        /**
         * Returns the component with specified type
         * @returns specified component
         * @tparam ComponentType type of the component to be looked for
         * */
        template<typename ComponentType>
        auto GetComponent() -> decltype(auto) {
            if (!m_Scene.lock())
                KATE_CORE_LOGGER_ERROR("This entity's scene has expired and no longer exists!");

            return m_Scene.lock()->m_Registry.get<ComponentType>(m_EntityHandle);
        }

        template<typename... ComponentTypeList>
        auto GetComponentList() -> decltype(auto) {
            if (!m_Scene.lock())
                KATE_CORE_LOGGER_ERROR("This entity's scene has expired and no longer exists!");

            return std::forward_as_tuple(m_Scene.lock()->m_Registry.get<ComponentTypeList>(m_EntityHandle)...);
        }

        template<typename ComponentType, typename... Args>
        auto AddComponent(Args&&... args) -> decltype(auto) {
            std::shared_ptr<Scene> ptr{};
            if (!(ptr = m_Scene.lock()))
                KATE_CORE_LOGGER_ERROR("This entity's scene has expired and no longer exists!");

            ComponentType& newComponent{ ptr->m_Registry.emplace_or_replace<ComponentType>(m_EntityHandle, std::forward<Args>(args)...) };
            OnComponentAttach(newComponent);

            return newComponent;
        }

        // drops the component if and only if it exists,
        // otherwise it returns safely to the caller:
        template<typename ComponentType>
        auto RemoveComponent() -> void {
            if (auto ptr{ m_Scene.lock() }) {
                ptr->m_Registry.remove<ComponentType>(m_EntityHandle);
                // TODO: add OnComponentRemove similar to AddComponent
            }
            else
                KATE_CORE_LOGGER_ERROR("This entity's scene has expired and no longer exists!");
        }

        // Returns true if the entity is valid for a given Scene
        // If the scene is nullptr it will return true if this Entity is valid Entity for a scene
        auto IsValidSceneEntity(const std::shared_ptr<Scene>& scene) {
            return scene->m_Registry.valid(m_EntityHandle);
        }

        auto operator==(const Entity& other) const -> bool { return m_EntityHandle == other.m_EntityHandle; /*&& m_Scene == other.m_Scene;*/ }

        auto Invalidate() -> void { m_EntityHandle = entt::null; }

        auto SetContext(const std::weak_ptr<Scene>& context) { m_Scene = context; }
    private:
        template<typename ComponentType>
        auto OnComponentAttach(ComponentType& newComponent) -> void;

    private:
        explicit Entity(const std::shared_ptr<Scene>& scene) {
            m_Scene = scene;
            m_EntityHandle = scene->m_Registry.create();
        }

        explicit Entity(entt::entity handle, const std::shared_ptr<Scene>& scene) {
            m_Scene = scene;
            m_EntityHandle = handle;
        }
    private:
        friend class HierarchyPanel;
        friend class InspectorPanel;
        friend class ScenePanel;

        friend class Scene;
        entt::entity m_EntityHandle{ entt::null };

        // The scene this entity belongs to
        // We do not use shared or unique pointers because the Scene
        // is not part of the entity and shouldn't extend its lifetime
        std::weak_ptr<Scene> m_Scene{};
    };

    template<typename ComponentType>
    inline auto Entity::OnComponentAttach(ComponentType& newComponent) -> void {
        static_assert("Invalid Component Type for OnComponentAttach");
    }

    template<>
    inline auto Entity::OnComponentAttach<TagComponent>(TagComponent& newComponent) -> void {
        KATE_CORE_LOGGER_INFO("Added new Tag Component");
    }

    template<>
    inline auto Entity::OnComponentAttach<TransformComponent>(TransformComponent& newComponent) -> void {
        KATE_CORE_LOGGER_INFO("Added new Transform Component");
    }

    template<>
    inline auto Entity::OnComponentAttach<SpriteRendererComponent>(SpriteRendererComponent& newComponent) -> void {
        KATE_CORE_LOGGER_INFO("Added new Sprite Renderer Component");
    }

    template<>
    inline auto Entity::OnComponentAttach<CameraComponent>(CameraComponent& newComponent) -> void {
        KATE_CORE_LOGGER_INFO("Added new Camera Component");
        std::shared_ptr<Scene> ptr{};
        if (!(ptr = m_Scene.lock()))
            KATE_CORE_LOGGER_ERROR("This entity's scene has expired and no longer exists!");
        newComponent.GetCameraPtr()->SetViewportSize(ptr->m_ViewportWidth, ptr->m_ViewportHeight);
    }

    template<>
    inline auto Entity::OnComponentAttach<NativeScriptComponent>(NativeScriptComponent& newComponent) -> void {
        KATE_CORE_LOGGER_INFO("Added new Native Script Component");
    }

    class ScriptableEntity : public Entity {
    public:
        explicit ScriptableEntity() = default;
        ~ScriptableEntity() = default;

        ScriptableEntity(const ScriptableEntity& other) = default;
        ScriptableEntity(ScriptableEntity&& other) = default;

        auto operator=(const ScriptableEntity& other) -> ScriptableEntity& = default;
        auto operator=(ScriptableEntity&& other) -> ScriptableEntity& = default;
    };

}

#endif // MIKOTO_ENTITY_HH
