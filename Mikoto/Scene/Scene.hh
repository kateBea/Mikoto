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
#include <Common/Common.hh>
#include <Models/SceneMetaData.hh>
#include <STL/DataStructures/GenTree.hh>
#include <STL/Random/Random.hh>
#include <STL/Utility/Types.hh>
#include <Scene/EditorCamera.hh>
#include <Scene/Entity.hh>

namespace Mikoto {

    /**
     * @brief Scene wrapper for game objects (entities)
     * Represents a scene, manages entities and their components
     * using an entity-component system (ECS).
     * */
    class Scene final {
    public:
        /**
         * Constructs a scene with a given name.
         * @param name The name of the scene.
         * */
        explicit Scene( const std::string_view name = "Mikoto" )
            : m_Name{ name } {}


        /**
         * Move constructor for a Scene object.
         * */
        Scene( Scene&& other ) = default;


        /**
         * Move assignment operator for a Scene object.
         * */
        auto operator=( Scene&& other ) -> Scene& = default;


        /**
         * Equality operator to compare Scene objects.
         * @param other The Scene object to compare.
         * @return Returns true if the Scenes have the same name.
         * */
        auto operator==( const Scene& other ) const noexcept -> bool { return m_Name == other.m_Name; }


        /**
         * Updates the scene during runtime (scene not being edited).
         * @param ts The time step for the update.
         * */
        auto OnRuntimeUpdate( double ts ) -> void;


        /**
         * Updates the scene in the editor.
         * @param timeStep The time step for the update.
         * @param camera The EditorCamera used in the scene.
         * */
        auto OnEditorUpdate( double timeStep, const EditorCamera& camera ) -> void;


        /**
         * Handles viewport resizing of the scene.
         * @param width The new width of the viewport.
         * @param height The new height of the viewport.
         * */
        auto OnViewPortResize( UInt32_T width, UInt32_T height ) -> void;


        /**
         * Retrieves the entity registry.
         * TODO: remove, shouldn't be part of public interface
         * @return A reference to the entity registry.
         * */
        auto GetRegistry() -> entt::registry& { return m_Registry; }

        /**
         * Retrieves the entity registry.
         * @return A reference to the entity registry.
         * */
        auto GetHierarchy() -> GenTree<Entity>& { return m_Hierarchy; }

        /**
         * Retrieves the name of the scene.
         * @return The name of the scene.
         * */
        MKT_NODISCARD auto GetName() const -> const std::string& { return m_Name; }


        /**
         * Retrieves updated information about this scene.
         * @return This scene's meta data.
         * */
        MKT_NODISCARD auto GetSceneMetaData() -> SceneMetaData&;

        /**
         * Destroys an entity within the scene.
         * @param target The entity to be destroyed.
         * */
        auto DestroyEntity( Entity& target ) -> bool;


        /**
         * Adds an empty object to the scene.
         * @param tagName The tag name of the object.
         * @param root Parent entity
         * @param guid The globally unique ID for the object.
         * @return The created entity in the scene.
         * */
        auto CreateEmptyEntity( std::string_view tagName, const Entity* root = nullptr, UInt64_T guid = GenerateGUID() ) -> Entity;


        /**
         * Adds a prefab object to the scene.
         * @param tagName The tag name of the object.
         * @param model Model from which we create the prefab
         * @param root New entity's parent
         * @param guid The globally unique ID for the object.
         * @return The created entity in the scene.
         * */
        auto CreatePrefabEntity( std::string_view tagName, Model* model, Entity* root = nullptr,
                           UInt64_T guid = GenerateGUID() ) -> Entity;


        /**
         * Clears the scene, removing all entities and components.
         * */
        auto Clear() -> void;


        /**
         * Default destructor.
         * */
        ~Scene() = default;

    private:
        GenTree<Entity> m_Hierarchy{};

        std::string m_Name{};
        entt::registry m_Registry{};
        SceneMetaData m_MetaData{};
    };
}

#endif// MIKOTO_SCENE_HH
