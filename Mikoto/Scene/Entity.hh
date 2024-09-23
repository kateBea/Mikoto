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
#include <Common/Common.hh>

#include <Core/Logger.hh>

#include <Scene/Component.hh>

namespace Mikoto {
    class Entity {
    public:
        explicit Entity() = default;

        Entity(const entt::entity &handle, entt::registry& registry)
            :   m_EntityHandle{ handle }, m_Registry{ std::addressof(registry) }
        {

        }

        Entity(const Entity& other) = default;
        Entity(Entity&& other) = default;

        auto operator=(const Entity& entity) -> Entity& = default;
        auto operator=(Entity&& entity) noexcept -> Entity& = default;

        MKT_NODISCARD auto Get() const -> const entt::entity& { return m_EntityHandle; }
        /**
         * Returns true if this entity contains all of the components listed in the parameter pack
         * @returns true if this entity contains all of components of the parameter pack list
         * @tparam ComponentTypeList parameter pack containing all of the components to be checked
         * */
        template<typename... ComponentTypeList>
        MKT_NODISCARD auto HasAllComponents() -> bool {
            return m_Registry->all_of<ComponentTypeList...>(m_EntityHandle);
        }

        /**
         * Returns true if this entity contains at least one of the components listed in the parameter pack
         * @returns true if this entity contains at least one of components of the parameter pack list
         * @tparam ComponentTypeList parameter pack containing all of the components to be checked
         * */
        template<typename... ComponentTypeList>
        MKT_NODISCARD auto HasAnyOfComponents() -> bool {
            return m_Registry->any_of<ComponentTypeList...>(m_EntityHandle);
        }

        /**
         * Returns true if this entity contains the given component
         * @returns true if this entity contains the given component
         * @tparam ComponentType component to be checked
         * */
        template<typename ComponentType>
        MKT_NODISCARD auto HasComponent() -> bool {
            return HasAllComponents<ComponentType>();
        }

        /**
         * Returns true if this entity is valid, false otherwise
         * @returns true if the implicit parameter is a valid entity
         * */
        MKT_NODISCARD auto IsValid() const -> bool {
            return m_Registry != nullptr && m_Registry->valid(m_EntityHandle);
        }

        /**
         * Returns the component with specified type
         * @returns specified component
         * @tparam ComponentType type of the component to be looked for
         * */
        template<typename ComponentType>
        auto GetComponent() -> ComponentType& {
            return m_Registry->get<ComponentType>(m_EntityHandle);
        }

        /**
         * Returns the list of components specified as a tuple
         * @returns list of specified components
         * */
        template<typename... ComponentTypeList>
        auto GetComponentList() -> decltype(auto) {
            return std::forward_as_tuple(m_Registry->get<ComponentTypeList...>(m_EntityHandle));
        }

        /**
         * Adds the specified components to this entity
         * @retuns newly added component
         * @param args pack containing the arguments to initialize the new component
         * @tparam ComponentType type of the new component
         * @tparam Args pack of types for the arguments required to initialize the new component
         * */
        template<typename ComponentType, typename... Args>
        auto AddComponent(Args&&... args) -> ComponentType& {
            ComponentType& newComponent{ m_Registry->emplace_or_replace<ComponentType>(m_EntityHandle, std::forward<Args>(args)...) };

            newComponent.OnComponentAttach();

            return newComponent;
        }

        /**
         * Removes the component if and only if it is part of this entity
         * @tparam ComponentType type of the component to be added
         * */
        template<typename ComponentType>
        auto RemoveComponent() -> void {
            if ( !HasComponent<ComponentType>()) {
                return;
            }

            GetComponent<ComponentType>().OnComponentRemoved();
            m_Registry->remove<ComponentType>(m_EntityHandle);
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
            m_Registry = nullptr;
            m_EntityHandle = entt::null;
        }


        ~Entity() = default;

    private:
        friend class Scene;
        friend class ScenePanel;
        friend class HierarchyPanel;
        friend class InspectorPanel;

    private:
        entt::entity m_EntityHandle{ entt::null };
        entt::registry* m_Registry{ nullptr };  // TODO: change to a shared pointer
    };
}

#endif // MIKOTO_ENTITY_HH
