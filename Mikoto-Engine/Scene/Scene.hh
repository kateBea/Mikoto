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
#include <glm/glm.hpp>
#include <entt/entt.hpp>

// Project Headers
#include <Common/Types.hh>
#include <Common/Common.hh>
#include <Common/Random.hh>
#include <Common/RenderingUtils.hh>

#include <EditorCamera.hh>
#include <Scene/Entity.hh>

namespace Mikoto {
    struct SceneMetaData {
        // [Game objects]
        UInt64_T EntityCount{};

        // [Lighting]
        UInt64_T LightsCount{}, ActiveLightCount{};
        UInt64_T DirLightsCount{}, DirActiveLightCount{};
        UInt64_T SpotLightsCount{}, SpotActiveLightCount{};
        UInt64_T PointLightsCount{}, PointActiveLightCount{};
    };

    /**
     * @brief Scene wrapper for game objects (entities)
     * Represents a scene within the application, managing entities
     * and their components using an entity-component system (ECS).
     * */
    class Scene {
    public:
        /**
         * Constructs a scene with a given name.
         * @param name The name of the scene.
         * */
        explicit Scene( std::string_view name = "Mikoto" )
            :   m_Name{ name }
        {

        }


        /**
         * Move constructor for a Scene object.
         * */
        Scene( Scene&& other ) = default;


        /**
         * Destructor for the Scene.
         * */
        ~Scene();


        /**
         * Move assignment operator for a Scene object.
         * */
        auto operator=( Scene&& other ) -> Scene& = default;


        /**
         * Equality operator to compare Scene objects.
         * @param other The Scene object to compare.
         * @return Returns true if the Scenes have the same name.
         * */
        auto operator==( const Scene& other ) noexcept -> bool { return m_Name == other.m_Name; }


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
         * @return A reference to the entity registry.
         * */
        auto GetRegistry() -> entt::registry& { return m_Registry; }


        /**
         * Retrieves the entity registry (const version).
         * @return A const reference to the entity registry.
         * */
        MKT_NODISCARD auto GetRegistry() const -> const entt::registry& { return m_Registry; }


        /**
         * Destroys an entity within the scene.
         * @param entity The entity to be destroyed.
         * */
        auto DestroyEntity( Entity& entity ) -> void;


        /**
         * Retrieves the scene information, from last update.
         * @return This scene's meta data.
         * */
        MKT_NODISCARD auto GetSceneMetaData() const -> const SceneMetaData& { return m_MetaData; }


        /**
         * Retrieves updated information about this scene.
         * @return This scene's meta data.
         * */
        MKT_NODISCARD auto FetchSceneMetaData() -> SceneMetaData&;


        /**
         * Retrieves the name of the scene.
         * @return The name of the scene.
         * */
        MKT_NODISCARD auto GetName() const -> const std::string& { return m_Name; }


        /**
         * Adds an empty object to the scene.
         * @param tagName The tag name of the object.
         * @param guid The globally unique ID for the object.
         * @return The created entity in the scene.
         * */
        auto CreateEmptyObject( std::string_view tagName, UInt64_T guid = Random::GUID::GenerateGUID() ) -> Entity;


        /**
         * Adds a prefab object to the scene.
         * @param tagName The tag name of the object.
         * @param type The PrefabSceneObject type.
         * @param guid The globally unique ID for the object.
         * @return The created entity in the scene.
         * */
        auto CreatePrefab( std::string_view tagName, PrefabSceneObject type, UInt64_T guid = Random::GUID::GenerateGUID() ) -> Entity;


        /**
         * Retrieves the currently active scene.
         * @return A pointer to the currently active Scene.
         * */
        MKT_NODISCARD static auto GetActiveScene() -> Scene*;


        /**
         * Sets the active scene.
         * @param scene The Scene to set as active.
         * */
        static auto SetActiveScene( Scene* scene ) -> void;


        /**
         * Clears the scene, removing all entities and components.
         * */
        auto Clear() -> void;

    private:
        // [Friends]
        friend class Entity;
        friend class ScenePanel;
        friend class HierarchyPanel;
        friend class InspectorPanel;

    private:
        /**
         * Updates scripts associated with entities in the scene.
         * */
        auto UpdateScripts() -> void;

        // Currently active scene in the editor.
        static inline Scene* s_ActiveScene{ nullptr };

        // [Constants for default entity parameters]
        static constexpr glm::vec3 ENTITY_INITIAL_SIZE{ 1.0f, 1.0f, 1.0f };
        static constexpr glm::vec3 ENTITY_INITIAL_POSITION{ 0.0, 0.0, 0.0 };
        static constexpr glm::vec3 ENTITY_INITIAL_ROTATION{ 0.0f, 0.0f, 0.0f };

    private:
        std::string m_Name{};       // The name of the scene
        UInt32_T m_ViewportWidth{}; // Width of the scene viewport
        UInt32_T m_ViewportHeight{};// Height of the scene viewport
        entt::registry m_Registry{};// Entity registry for the scene

        SceneMetaData m_MetaData{};
    };
}

#endif // MIKOTO_SCENE_HH
