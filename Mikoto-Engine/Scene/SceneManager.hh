//
// Created by kate on 10/8/23.
//

#ifndef MIKOTO_SCENE_MANAGER_HH
#define MIKOTO_SCENE_MANAGER_HH

#include <memory>
#include <string>
#include <unordered_map>
#include <string_view>

#include "Common/Random.hh"
#include "Common/Types.hh"
#include "Entity.hh"
#include "Scene.hh"

namespace Mikoto {
    struct EntityCreateInfo {
        std::string Name{};
        bool IsPrefab{};
        PrefabSceneObject PrefabType{};
        UInt64_T EntityGuid{};
    };

    class SceneManager {
    public:
        static auto Init() -> void;

        static auto MakeNewScene(std::string_view name) -> Scene &;

        /**
         * Returns true if the entity is a valid entity
         * for the given scene, returns false otherwise
         * @param scene scene to to be inspected
         * @param entity to be inspected
         * @returns true if the entity belongs to given scene
         * */
        static auto IsSceneValidEntity(const Scene& scene, const Entity& entity) -> bool;

        /**
         * Tells if there's an entity currently targeted in the scene
         * @returns true if there's an entity selected in the scene, false otherwise
         * */
        static auto IsEntitySelected() -> bool;

        /**
         * Returns the currently focused entity in the editor viewport.
         * @returns currently targeted entity
         * */
        static auto GetCurrentlySelectedEntity() -> Entity&;

        /**
         * Establishes the currently active entity in the viewport
         * @param entity currently active entity
         * */
        static auto SetCurrentlyActiveEntity(const Entity& entity) -> void;

        /**
         * Disables the currently active entity
         * */
        static auto DisableCurrentlyActiveEntity() -> void;

        /**
         * Returns the currently active scene. The currently active
         * is the one currently being edited on the main Viewport panel.
         * @returns reference to the active scene
         * */
        static auto GetActiveScene() -> Scene&;


        static auto DisableTargetEntity() -> void;

        static auto SetActiveScene(Scene& scene) -> void;

        /**
         * Execute a function for all entities with the given components
         * */
        template<typename... ComponentTypes>
        static auto ForEachWithComponentsOnScene(Scene& scene, const std::function<void(Entity&)>& task) -> void {
            auto view{ scene.GetRegistry().view<ComponentTypes...>() };

            for (auto& entityHandle : view) {
                Entity current{ entityHandle, GetActiveScene().GetRegistry() };
                task(current);
            }
        }

        /**
         * Execute a function for all entities with the given components
         * */
        template<typename... ComponentTypes>
        static auto ForEachWithComponents(const std::function<void(Entity&)>& task) -> void {
            ForEachWithComponentsOnScene<ComponentTypes...>(GetActiveScene(), task);
        }

        /**
         * Destroys the given entity from the given scene. After this operation
         * entity is no longer valid for the the given scene and all of the entities
         * resources are released.
         * @param scene scene from which we have to remove the entity
         * @param entity entity to be removed
         * */
        static auto DestroyEntityFromScene(Scene& scene, Entity& entity) -> void;

        /**
         * Same as DestroyEntityFromScene(Scene&, Entity&) but the scene provided
         * is the currently active scene.
         * @param entity entity to be removed
         * */
        static auto DestroyEntityFromCurrentlyActiveScene(Entity& entity) -> void;


        /**
         * Destroys the given scene releasing all the resources and entities associated with it
         * @param scene scene to be destroyed
         * */
        static auto DestroyScene(const Scene& scene) -> void;

        static auto DestroyActiveScene() -> void;

        /**
         * Creates an entity on the currently active scene.
         * @param createInfo spec to create an entity
         * @returns newly created entity
         * */
        static auto AddEntity(const EntityCreateInfo& createInfo) -> Entity;

        static auto AddEntityToScene(Scene &scene, const EntityCreateInfo& createInfo) -> Entity;


        static auto Shutdown() -> void;

    private:
        static inline Entity s_CurrentlySelectedEntity{};
        static inline std::unordered_map<std::string, Scene> s_Scenes{};
    };
}

#endif // MIKOTO_SCENE_MANAGER_HH
