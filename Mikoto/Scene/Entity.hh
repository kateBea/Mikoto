//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MIKOTO_ENTITY_HH
#define MIKOTO_ENTITY_HH

#include <utility>

#include <entt/entt.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Memory/Allocator.hh>

namespace mikoto::scene {

    // TODO: test flecs
    // #if defined(MKT_USE_ECS_ENTT)
    // # elif defined(MKT_USE_ECS_FLECS)

    class Entity {
    public:

        MKT_NODISCARD auto Get() const -> decltype( auto ) { return (mHandle); }

        /**
         * Returns true if this entity contains all the components listed in the parameter pack
         * @returns true if this entity contains all components of the parameter pack list
         * @tparam ComponentTypeList parameter pack containing all the components to be checked
         * */
        template<typename... ComponentTypeList>
        MKT_NODISCARD auto HasAllComponents() const -> bool {
            return mRegistry->all_of<ComponentTypeList...>(mHandle);
        }

        /**
         * Returns true if this entity contains at least one of the components listed in the parameter pack
         * @returns true if this entity contains at least one of the components of the parameter pack list
         * @tparam ComponentTypeList parameter pack containing all the components to be checked
         * */
        template<typename... ComponentTypeList>
        MKT_NODISCARD auto HasAnyOfComponents() const -> bool {
            return mRegistry->any_of<ComponentTypeList...>(mHandle);
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
            return mRegistry != nullptr && mRegistry->valid(mHandle);
        }

        /**
         * Returns the component with specified type
         * @returns specified component
         * @tparam ComponentType type of the component to be looked for
         * */
        template<typename ComponentType>
        auto GetComponent() -> ComponentType& {
            return mRegistry->get<ComponentType>(mHandle);
        }

        /**
         * Returns the component with specified type
         * @returns specified component
         * @tparam ComponentType type of the component to be looked for
         * */
        template<typename ComponentType>
        auto GetComponent() const -> const ComponentType& {
            return mRegistry->get<ComponentType>(mHandle);
        }

        /**
         * Returns the list of components specified as a tuple
         * @returns list of specified components
         * */
        template<typename... ComponentTypeList>
        auto GetComponentList() -> decltype(auto) {
            return std::forward_as_tuple(mRegistry->get<ComponentTypeList...>(mHandle));
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
            ComponentType& newComponent{ mRegistry->emplace_or_replace<ComponentType>(mHandle, std::forward<Args>(args)...) };

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

            mRegistry->remove<ComponentType>(mHandle);
        }

        /**
         * Returns true if this entity is the same as other, meaning they have the
         * same handle and are part of the same scene
         * @param other entity to compare the implicit parameter to
         * */
        auto operator==(const Entity& other) const -> bool {
            return mHandle == other.mHandle && mRegistry == other.mRegistry;
        }

        ~Entity() {
            Invalidate();
        }

    private:
        explicit Entity(entt::registry& registry)
            :   mHandle{ registry.create() }, mRegistry{ MKT_ADDRESSOF(registry) }
        {
            /**
             * See: Observe changes section from https://github.com/skypjack/entt/wiki/Entity-Component-System
             * for details on listeners.
             * */
        }

        /**
         * Puts this entity into an invalid state. Other methods are not recommended to be called
         * on the implicit parameter after a call to this function, otherwise it may result
         * in undefined behavior. This entity can be validated again via move or copy assigment
         * */
        auto Invalidate() -> void {
            // This entity does not belong to any scene anymore
            // it can be safely removed from entt structures
            mRegistry = nullptr;
            mHandle = entt::null;
        }

        friend class Scene;

    private:
        entt::entity mHandle{ entt::null };
        entt::registry* mRegistry{ nullptr };
    };

    template<typename ComponentType>
    MKT_NODISCARD static inline auto IsPresent( Entity* entity ) -> bool {
        if ( entity == nullptr ) {
            return false;
        }

        return entity->HasComponent<ComponentType>();
    }
}

#endif // MIKOTO_ENTITY_HH
