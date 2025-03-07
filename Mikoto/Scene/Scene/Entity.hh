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
#include <Scene/Scene/Component.hh>

namespace Mikoto {

    class Entity {
    public:

        explicit Entity(entt::registry& registry)
            :   m_Handle{ registry.create() }, m_Registry{ std::addressof(registry) }
        {
            /**
             * See: Observe changes section from https://github.com/skypjack/entt/wiki/Entity-Component-System
             * for details on listeners.
             * */
        }

        MKT_NODISCARD auto Get() const -> decltype( auto ) { return (m_Handle); }

        /**
         * Returns true if this entity contains all the components listed in the parameter pack
         * @returns true if this entity contains all components of the parameter pack list
         * @tparam ComponentTypeList parameter pack containing all the components to be checked
         * */
        template<typename... ComponentTypeList>
        MKT_NODISCARD auto HasAllComponents() const -> bool {
            return m_Registry->all_of<ComponentTypeList...>(m_Handle);
        }

        /**
         * Returns true if this entity contains at least one of the components listed in the parameter pack
         * @returns true if this entity contains at least one of the components of the parameter pack list
         * @tparam ComponentTypeList parameter pack containing all the components to be checked
         * */
        template<typename... ComponentTypeList>
        MKT_NODISCARD auto HasAnyOfComponents() const -> bool {
            return m_Registry->any_of<ComponentTypeList...>(m_Handle);
        }

        /**
         * Returns true if this entity contains the given component
         * @returns true if this entity contains the given component
         * @tparam ComponentType component to be checked
         * */
        template<typename ComponentType>
        MKT_NODISCARD auto HasComponent() const -> bool {
            return HasAllComponents<ComponentType>();
        }

        /**
         * Returns true if this entity is valid, false otherwise
         * @returns true if the implicit parameter is a valid entity
         * */
        MKT_NODISCARD auto IsValid() const -> bool {
            return m_Registry != nullptr && m_Registry->valid(m_Handle);
        }

        /**
         * Returns the component with specified type
         * @returns specified component
         * @tparam ComponentType type of the component to be looked for
         * */
        template<typename ComponentType>
        auto GetComponent() -> ComponentType& {
            return m_Registry->get<ComponentType>(m_Handle);
        }

        /**
         * Returns the component with specified type
         * @returns specified component
         * @tparam ComponentType type of the component to be looked for
         * */
        template<typename ComponentType>
        auto GetComponent() const -> const ComponentType& {
            return m_Registry->get<ComponentType>(m_Handle);
        }

        /**
         * Returns the list of components specified as a tuple
         * @returns list of specified components
         * */
        template<typename... ComponentTypeList>
        auto GetComponentList() -> decltype(auto) {
            return std::forward_as_tuple(m_Registry->get<ComponentTypeList...>(m_Handle));
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
            ComponentType& newComponent{ m_Registry->emplace_or_replace<ComponentType>(m_Handle, std::forward<Args>(args)...) };

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

            m_Registry->remove<ComponentType>(m_Handle);
        }

        /**
         * Returns true if this entity is the same as other, meaning they have the
         * same handle and are part of the same scene
         * @param other entity to compare the implicit parameter to
         * */
        auto operator==(const Entity& other) const -> bool {
            return m_Handle == other.m_Handle && m_Registry == other.m_Registry;
        }

        /**
         * Puts this entity into an invalid state. Other methods are not recommended to be called
         * on the implicit parameter after a call to this function, otherwise it may result
         * in undefined behaviour. This entity can be validated again via move or copy assigment
         * */
        auto Invalidate() -> void {
            m_Registry = nullptr;
            m_Handle = entt::null;
        }

        ~Entity() {
            Invalidate();
        }

    private:

        friend class Scene;

    private:
        entt::entity m_Handle{ entt::null };
        entt::registry* m_Registry{ nullptr };
    };
}

#endif // MIKOTO_ENTITY_HH
