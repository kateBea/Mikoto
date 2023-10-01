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

        /**
         * Constructs an entity as from the parameter using move operations
         * The parameter entity is put into an invalid state after this operation.
         * @param other moved from entity
         * */
        Entity(Entity&& other) noexcept {
            m_EntityHandle = std::move(other.m_EntityHandle);

            if (auto ptr{ other.m_Scene.lock() })
                m_Scene = ptr;
            else
                MKT_CORE_LOGGER_WARN("Other entity's scene has expired and no longer exists!");

            other.Invalidate();
        }

        /**
         * Constructs an entity as a copy of the parameter (defaulted)
         * @param other copied from entity
         * */
        Entity(const Entity& other) {
            m_EntityHandle = other.m_EntityHandle;

            if (auto ptr{ other.m_Scene.lock() })
                m_Scene = ptr;
            else
                MKT_CORE_LOGGER_WARN("Other entity's scene has expired and no longer exists!");
        }

        /**
         * Assigns the implicit parameter to the parameter entity via copy operations
         * @param other copied from entity
         * @returns *this
         * */
        auto operator=(const Entity& other) -> Entity& {
            m_EntityHandle = other.m_EntityHandle;

            if (auto ptr{ other.m_Scene.lock() })
                m_Scene = ptr;
            else
                MKT_CORE_LOGGER_WARN("Other entity's scene has expired and no longer exists!");

            return *this;
        };

        /**
         * Assigns the implicit parameter to the parameter entity via move operations.
         * The parameter entity is put into an invalid state after this operation.
         * @param other moved from entity
         * @returns *this
         * */
        auto operator=(Entity&& other) noexcept -> Entity& {
            m_EntityHandle = std::move(other.m_EntityHandle);

            if (auto ptr{ other.m_Scene.lock() })
                m_Scene = ptr;
            else
                MKT_CORE_LOGGER_WARN("Other entity's scene has expired and no longer exists!");

            other.Invalidate();
            return *this;
        }

        /**
         * Returns true if this entity contains all of the components listed in the parameter pack
         * @returns true if this entity contains all of components of the parameter pack list
         * @tparam ComponentTypeList parameter pack containing all of the components to be checked
         * */
        template<typename... ComponentTypeList>
        MKT_NODISCARD auto HasAllComponents() -> bool {
            if (auto ptr{ m_Scene.lock() }) {
                return ptr->m_Registry.all_of<ComponentTypeList...>(m_EntityHandle);
            }

            MKT_CORE_LOGGER_WARN("This entity's scene has expired and no longer exists!");
            return false;
        }

        /**
         * Returns true if this entity contains at least one of the components listed in the parameter pack
         * @returns true if this entity contains at least one of components of the parameter pack list
         * @tparam ComponentTypeList parameter pack containing all of the components to be checked
         * */
        template<typename... ComponentTypeList>
        MKT_NODISCARD auto HasAnyOfComponents() -> bool {
            if (auto ptr{ m_Scene.lock() }) {
                return ptr->m_Registry.any_of<ComponentTypeList...>(m_EntityHandle);
            }

            MKT_CORE_LOGGER_WARN("This entity's scene has expired and no longer exists!");
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

            MKT_CORE_LOGGER_WARN("This entity's scene has expired and no longer exists!");
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
                MKT_CORE_LOGGER_WARN("This entity's scene has expired and no longer exists!");

            return m_Scene.lock()->m_Registry.get<ComponentType>(m_EntityHandle);
        }

        /**
         * Returns the list of components specified as a tuple
         * @returns list of specified components
         * */
        template<typename... ComponentTypeList>
        auto GetComponentList() -> decltype(auto) {
            if (!m_Scene.lock())
                MKT_CORE_LOGGER_WARN("This entity's scene has expired and no longer exists!");

            return std::forward_as_tuple(m_Scene.lock()->m_Registry.get<ComponentTypeList>(m_EntityHandle)...);
        }

        /**
         * Adds the specified components to this entity
         * @retuns newly added component
         * @param args pack containing the arguments to initialize the new component
         * @tparam ComponentType type of the new component
         * @tparam Args pack of types for the arguments required to initialize the new component
         * */
        template<typename ComponentType, typename... Args>
        auto AddComponent(Args&&... args) -> decltype(auto) {
            std::shared_ptr<Scene> ptr{};
            if (!(ptr = m_Scene.lock()))
                MKT_CORE_LOGGER_WARN("This entity's scene has expired and no longer exists!");

            ComponentType& newComponent{ ptr->m_Registry.emplace_or_replace<ComponentType>(m_EntityHandle, std::forward<Args>(args)...) };
            OnComponentAttach(newComponent);

            return newComponent;
        }

        /**
         * Removes the component if and only if it is part of this entity
         * @tparam ComponentType type of the component to be added
         * */
        template<typename ComponentType>
        auto RemoveComponent() -> void {
            if (auto ptr{ m_Scene.lock() }) {
                OnComponentDetach(GetComponent<ComponentType>());
                ptr->m_Registry.remove<ComponentType>(m_EntityHandle);
            }
            else
                MKT_CORE_LOGGER_WARN("This entity's scene has expired and no longer exists!");
        }

        /**
         * Returns true if the entity is valid for a given Scene
         * @returns true if this entity is valid within the given scene, false otherwise
         * */
        auto IsValidSceneEntity(const std::shared_ptr<Scene>& scene) -> decltype(auto) {
            return scene->m_Registry.valid(m_EntityHandle);
        }

        /**
         * Returns true if this entity is the same as other, meaning they have the
         * same handle and are part of the same scene
         * @param other entity to compare the implicit parameter to
         * */
        auto operator==(const Entity& other) const -> bool {
            return m_EntityHandle == other.m_EntityHandle; /*&& m_Scene == other.m_Scene;*/
        }

        /**
         * Puts this entity into an invalid state. Other methods are not recommended to be called
         * on the implicit parameter after a call to this function, otherwise it may result
         * in undefined behaviour. This entity can be validated again via move or copy assigment
         * */
        auto Invalidate() -> void {
            m_EntityHandle = entt::null;
        }

        /**
         * Establishes the scene this entity scene this entity belongs to
         * @param context scene this entity belongs to
         * */
        auto SetContext(const std::weak_ptr<Scene>& context) {
            m_Scene = context;
        }

        ~Entity() = default;

    private:
        template<typename ComponentType>
        auto OnComponentAttach(ComponentType& newComponent) -> void;

        template<typename ComponentType>
        auto OnComponentDetach(ComponentType& newComponent) -> void;

        /**
         * Construct this entity as part of the given scene. Hidden because this constructor is only useful
         * for the friend classes, especially for the Scene class which is the only one that should be creating entities
         * @param scene scene this entity is part of
         * */
        explicit Entity(const std::shared_ptr<Scene>& scene) {
            m_Scene = scene;
            m_EntityHandle = scene->m_Registry.create();
        }

        /**
         * Construct this entity as part of the given scene and entity handle. Hidden because this constructor is only useful
         * for the friend classes, especially for the Scene class which is the only one that should be creating entities
         * @param handle handle for this entity
         * @param scene scene this entity is part of
         * */
        explicit Entity(entt::entity handle, const std::shared_ptr<Scene>& scene) {
            m_Scene = scene;
            m_EntityHandle = handle;
        }
    private:
        friend class Scene;
        friend class ScenePanel;
        friend class HierarchyPanel;
        friend class InspectorPanel;

        entt::entity m_EntityHandle{ entt::null };

        /**
         * The scene this entity belongs to.
         *
         * We do not use shared or unique pointers because the Scene is not
         * part of the entity and the entity shouldn't extend the scene's lifetime
         * */
        std::weak_ptr<Scene> m_Scene{};
    };

    template<typename ComponentType>
    inline auto Entity::OnComponentAttach(ComponentType& newComponent) -> void {
        static_assert("Invalid Component Type for OnComponentAttach");
    }

    template<>
    inline auto Entity::OnComponentAttach<TagComponent>(TagComponent& newComponent) -> void {
        MKT_CORE_LOGGER_INFO("Added new Tag Component");
    }

    template<>
    inline auto Entity::OnComponentAttach<TransformComponent>(TransformComponent& newComponent) -> void {
        MKT_CORE_LOGGER_INFO("Added new Transform Component");
    }

    template<>
    inline auto Entity::OnComponentAttach<SpriteRendererComponent>(SpriteRendererComponent& newComponent) -> void {
        MKT_CORE_LOGGER_INFO("Added new Sprite Renderer Component");
    }

    template<>
    inline auto Entity::OnComponentAttach<CameraComponent>(CameraComponent& newComponent) -> void {
        MKT_CORE_LOGGER_INFO("Added new Camera Component");
        std::shared_ptr<Scene> ptr{};
        if (!(ptr = m_Scene.lock()))
            MKT_CORE_LOGGER_ERROR("This entity's scene has expired and no longer exists!");
        newComponent.GetCameraPtr()->SetViewportSize(ptr->m_ViewportWidth, ptr->m_ViewportHeight);
    }

    template<>
    inline auto Entity::OnComponentAttach<NativeScriptComponent>(NativeScriptComponent& newComponent) -> void {
        MKT_CORE_LOGGER_INFO("Removed new Native Script Component");
    }

    template<typename ComponentType>
    inline auto Entity::OnComponentDetach(ComponentType& newComponent) -> void {
        static_assert("Invalid Component Type for OnComponentDetach");
    }

    template<>
    inline auto Entity::OnComponentDetach<TagComponent>(TagComponent& newComponent) -> void {
        MKT_CORE_LOGGER_INFO("Removed Tag Component");
    }

    template<>
    inline auto Entity::OnComponentDetach<TransformComponent>(TransformComponent& newComponent) -> void {
        MKT_CORE_LOGGER_INFO("Removed Transform Component");
    }

    template<>
    inline auto Entity::OnComponentDetach<SpriteRendererComponent>(SpriteRendererComponent& newComponent) -> void {
        MKT_CORE_LOGGER_INFO("Removed Sprite Renderer Component");
    }

    template<>
    inline auto Entity::OnComponentDetach<CameraComponent>(CameraComponent& newComponent) -> void {
        MKT_CORE_LOGGER_INFO("Removed Camera Component");
        std::shared_ptr<Scene> ptr{};

    }

    template<>
    inline auto Entity::OnComponentDetach<NativeScriptComponent>(NativeScriptComponent& newComponent) -> void {
        MKT_CORE_LOGGER_INFO("Removed Native Script Component");
    }
}

#endif // MIKOTO_ENTITY_HH
