/**
 * Scene.hh
 * Created by kate on 6/24/23.
 * */

#ifndef MIKOTO_SCENE_HH
#define MIKOTO_SCENE_HH

// C++ Standard Library
#include <mutex>
#include <memory>
#include <string_view>

// Third-Party Libraries
#include <entt/entt.hpp>
#include <ankerl/unordered_dense.h>

// Project Headers
#include <Assets/Model.hh>
#include <Common/Common.hh>
#include <Library/Utility/Types.hh>
#include <Scene/Entity.hh>

namespace Mikoto {

    class PhysicsWorld;

    /**
     * @brief Enum representing the current state of the scene renderer.
     *
     * This enum is used to track whether the renderer is idle or actively simulating the scene.
     */
    enum class SceneState {
        IDLE,
        SIMULATING
    };

    struct EntityCreateInfo {
        Entity* Root{};
        std::string Name{};
        ModelHandle Model{};

        auto WithName( std::string_view name ) -> EntityCreateInfo&;
        auto WithRoot( Entity* root ) -> EntityCreateInfo&;
        auto WithModelMesh( ModelHandle modelMesh ) -> EntityCreateInfo&;
    };

    class Scene final {
    public:
        explicit Scene( std::string_view name = "New Scene" );

        /**
         * @brief Sets the state of the scene renderer.
         * This function changes the state of the renderer, such as transitioning from idle to simulating.
         * @param state The new state to set for the renderer.
         */
        auto SetState( SceneState state ) -> void;

        auto Update( float timeStep ) -> void;

        // Remove recursively check if its child of any entity
        auto RemoveEntity( UInt64 uniqueID ) -> void;

        auto SetName( std::string_view name ) -> void;

        MKT_NODISCARD auto FindByID( UInt64 uniqueID ) -> Entity*;
        MKT_NODISCARD auto FindFirstByName( std::string_view name ) -> Entity*;

        MKT_NODISCARD auto ExistsByID( UInt64 uniqueID ) -> bool;
        MKT_NODISCARD auto ExistsByName( std::string_view name ) -> bool;

        MKT_NODISCARD auto CreateEntity( std::string_view name ) -> Entity*;
        MKT_NODISCARD auto CreateEntity( const EntityCreateInfo& createInfo = {} ) -> Entity*;

        auto QueueCreateEntity( std::string_view name ) -> void;
        auto QueueCreateEntity( const EntityCreateInfo& createInfo = {} ) -> void;

        MKT_NODISCARD auto GetName() const -> const std::string& { return m_Name; }

        MKT_NODISCARD auto GetEntityCount() const -> Size;

        MKT_NODISCARD auto GetEntities() const -> const ankerl::unordered_dense::map<Size, Unique<Entity>>& { return m_Entities; }

        auto AttachRigidBody(Entity* entity ) const -> void;
        auto DetachRigidBody(Entity* entity ) const -> void;

        auto GetRegistry() -> entt::registry&;
        auto GetRegistry() const -> const entt::registry&;

        auto Clear() -> void;

        ~Scene();

    private:
        friend class PhysicsWorld;

        struct EntityCommand {
            enum class Type {
                CREATE, DESTROY
            } Type;

            EntityCreateInfo CreateInfo{};  // only for CREATE
            UInt64 EntityID{ 0 };          // only for DESTROY
        };

        auto ProcessPendingCommands() -> void;

        MKT_NODISCARD auto DestroyEntitySingle( UInt64 entityID) -> bool;
        MKT_NODISCARD auto CreateEntitySingle( const EntityCreateInfo& createInfo) -> Entity*;

        auto UpdateIdle( double deltaTime ) -> void;
        auto UpdateSimulate( double deltaTime ) -> void;

        auto OnRigidBodyAdded(entt::registry& reg, entt::entity e ) const -> void;
        auto OnRigidBodyRemoved(entt::registry& reg, entt::entity e ) const -> void;
    private:
        auto AddSingleEntityWithRoot(Entity * root, ModelHandle model, Int32 index ) -> void;

        auto WorkerDestroyEntity(UInt64 entityID) -> void;
        auto WorkerCreateEntity(const EntityCreateInfo& info) -> void;
    private:
        std::string m_Name{};
        entt::registry m_Registry{};

        PhysicsWorld* m_PhysicsWorld{};

        SceneState m_SceneState{ SceneState::IDLE };

        // Deletion and creation is deferred until we call update
        std::mutex m_CommandQueueMutex{};
        std::vector<EntityCommand> m_EntityCommands{};

        // Unique because iterators are invalidated on resize
        ankerl::unordered_dense::map<Size, Unique<Entity>> m_Entities{};
    };
}// namespace Mikoto

#endif// MIKOTO_SCENE_HH
