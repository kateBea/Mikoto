//
// Created by kate on 10/8/23.
//

#ifndef MIKOTO_SCENE_MANAGER_HH
#define MIKOTO_SCENE_MANAGER_HH

#include <memory>
#include <string>
#include <unordered_map>
#include <string_view>
#include <optional>

#include <STL/Random/Random.hh>
#include <STL/Utility/Types.hh>
#include <Scene/Entity.hh>
#include <Scene/Scene.hh>
#include <Models/EntityCreateInfo.hh>

namespace Mikoto {
    class SceneManager {
    public:
        /**
         * Optional to an entity reference wrapper
         * */
        using EntityOpt_T = std::optional<std::reference_wrapper<Entity>>;

    public:
        /**
         *
         * */
        static auto Init() -> void;


        /**
         *
         * */
        static auto MakeNewScene(std::string_view name) -> Scene&;


        /**
         * Establishes the currently active entity in the viewport
         * @param entity currently active entity
         * */
        static auto SetCurrentSelection(const Entity& entity) -> void;


        /**
         * Returns the currently selected entity. If there is no selected
         * entity the optional is empty.
         * @returns Currently active entity.
         * */
        static auto GetCurrentSelection() -> EntityOpt_T;


        /**
         * Disables the currently active entity.
         * */
        static auto DisableActiveSelection() -> void;


        /**
         * Returns the currently active scene. The currently active
         * is the one currently being edited on the main Viewport panel.
         * There's always going to be an active scene in the editor, that is why
         * we return a reference to it.
         * @returns reference to the active scene
         * */
        static auto GetActiveScene() -> Scene&;


        /**
         *
         * */
        static auto SetActiveScene(Scene& scene) -> void;


        /**
         * Execute a function for all entities with the given components
         * on the currently active scene.
         * */
        template<typename... ComponentTypes>
        static auto ForEachWithComponents(const std::function<void(Entity&)>& task) -> void {
            ForEachWithComponents<ComponentTypes...>(GetActiveScene(), task);
        }


        /**
         * Same as DestroyEntityFromScene(Scene&, Entity&) but the scene provided
         * is the currently active scene.
         * @param target entity to be removed
         * @returns True if the deletion was successful, false otherwise
         * */
        static auto DestroyEntity(Entity &target) -> void;


        /**
         * Destroys the given scene releasing all the resources and entities associated with it
         * @param scene scene to be destroyed
         * */
        static auto DestroyScene(const Scene& scene) -> void;


        /**
         * Creates an entity on the currently active scene.
         * @param createInfo spec to create an entity
         * @returns newly created entity
         * */
        static auto AddEntity(const EntityCreateInfo& createInfo) -> Entity;


        /**
         *
         * */
        static auto Shutdown() -> void;

    private:
        /**
         *
         * @param scene
         * @param createInfo
         * @returns
         * */
        static auto AddEntityToScene(Scene &scene, const EntityCreateInfo& createInfo) -> Entity;

        /**
         *
         * @param scene
         * @param target
         * @returns
         * */
        static auto DeleteEntityFromScene(Scene &scene, Entity& target) -> bool;

        /**
         * Execute a function for all entities with the given components
         * */
        template<typename... ComponentTypes>
        static auto ForEachWithComponents(Scene& scene, const std::function<void(Entity&)>& task) -> void {
            auto view{ scene.GetRegistry().view<ComponentTypes...>() };

            for (auto& entityHandle : view) {
                Entity current{ entityHandle, GetActiveScene().GetRegistry() };
                task(current);
            }
        }

    private:
      static inline Scene* s_ActiveScene{ nullptr };
        static inline Entity s_TargetEntity{};
        static inline std::unordered_map<std::string, Scene> s_Scenes{};
    };
}

#endif // MIKOTO_SCENE_MANAGER_HH
