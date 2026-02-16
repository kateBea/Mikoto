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

#ifndef MIKOTO_SCENE_HH
#define MIKOTO_SCENE_HH

#include <mutex>
#include <memory>
#include <string_view>

#include <entt/entt.hpp>
#include <ankerl/unordered_dense.h>

#include <Assets/Model.hh>
#include <Common/Common.hh>
#include <Library/Utility/Types.hh>
#include <Scene/Entity.hh>
#include <Scene/Component.hh>

namespace Mikoto {

    class PhysicsWorld;

    enum class SceneState {
        IDLE,
        SIMULATING
    };

    enum class SceneBackground {
        SKYBOX,
        CLEAR_COLOR,
    };

    struct EntityCreateInfo {
        Entity* Root{};
        std::string Name{};
        ModelHandle Model{};

        // Light config
        bool IsLight{ false };
        LightType TypeLight{ LightType::DIRECTIONAL_LIGHT_TYPE };

        // Text config
        bool IsText{ false };
        bool IsWorldText{ false };
        float TextSize{ 12.0f };
        float TextSpacing{ 1.0f };
        std::string InitialContents{};

        auto WithName( std::string_view name ) -> EntityCreateInfo&;
        auto WithRoot( Entity* root ) -> EntityCreateInfo&;
        auto WithModelMesh( ModelHandle modelMesh ) -> EntityCreateInfo&;
    };

    class Scene final {
    public:
        explicit Scene( std::string_view name = "New Scene" );

        auto SetState( SceneState state ) -> void;

        auto Update( float timeStep ) -> void;

        auto SetName( std::string_view name ) -> void;

        auto RemoveEntity( UInt64 uniqueID ) -> void;

        MKT_NODISCARD auto FindByID( UInt64 uniqueID ) -> Entity*;
        MKT_NODISCARD auto FindFirstByName( std::string_view name ) -> Entity*;

        MKT_NODISCARD auto ExistsByID( UInt64 uniqueID ) -> bool;
        MKT_NODISCARD auto ExistsByName( std::string_view name ) -> bool;

        MKT_NODISCARD auto CreateEntity( std::string_view name ) -> Entity*;
        MKT_NODISCARD auto CreateEntity( Entity* root, std::string_view name ) -> Entity*;
        MKT_NODISCARD auto CreateEntity( const EntityCreateInfo& createInfo = {} ) -> Entity*;

        MKT_NODISCARD auto GetPhysicsWorld() -> PhysicsWorld*;

        template<typename EntityFunction>
        auto ApplyToChildren(Entity* parent, const EntityFunction& callable) -> void;

        // [Deprecated] These will be moved to a camera component
        auto SetSceneBackground(SceneBackground background) -> void;
        MKT_NODISCARD auto GetSceneBackground() const -> SceneBackground;
        MKT_NODISCARD auto IsSceneBackground(SceneBackground background) const -> bool;
        auto SetSkybox( TextureHandle cubeMap ) -> void;
        MKT_NODISCARD auto GetSkybox( ) -> TextureHandle;

        auto QueueCreateEntity( std::string_view name ) -> void;
        auto QueueCreateEntity( const EntityCreateInfo& createInfo = {} ) -> void;

        // Call if the viewport where this scene is being rendered is resized
        auto OnViewPortResize( float width, float height ) -> void;

        MKT_NODISCARD auto GetLightCount() const -> UInt32;
        MKT_NODISCARD auto GetActiveLightCount() const -> UInt32;

        MKT_NODISCARD auto GetName() const -> const std::string& { return m_Name; }

        MKT_NODISCARD auto GetGamma() const -> float;
        MKT_NODISCARD auto GetExposure() const -> float;

        // To remove, deprecated
        auto SetGamma( float gamma ) -> void;
        auto SetExposure( float exposure ) -> void;

        MKT_NODISCARD auto GetEntityCount() const -> Size;

        MKT_NODISCARD auto GetEntities() const -> const ankerl::unordered_dense::map<Size, Unique<Entity>>& { return m_Entities; }

        auto GetRegistry() -> entt::registry&;
        MKT_NODISCARD auto GetRegistry() const -> const entt::registry&;

        auto Clear() -> void;

        ~Scene();

    private:
        friend class PhysicsWorld;

        struct EntityCommand {
            enum class Type {
                CREATE, DESTROY
            } Type{ Type::CREATE };

            EntityCreateInfo CreateInfo{};  // only for CREATE
            UInt64 EntityID{ 0 };           // only for DESTROY
        };

        auto ProcessPendingCommands() -> void;

        MKT_NODISCARD auto DestroyEntitySingle( UInt64 entityID) -> bool;
        MKT_NODISCARD auto CreateEntitySingle( const EntityCreateInfo& createInfo) -> Entity*;

        auto UpdateIdle( double deltaTime ) -> void;
        auto UpdateSimulate( double deltaTime ) -> void;

        auto OnScriptAdded(entt::registry& reg, entt::entity e ) -> void;

        auto OnRigidBodyAdded(entt::registry& reg, entt::entity e ) const -> void;
        auto OnRigidBodyRemoved(entt::registry& reg, entt::entity e ) const -> void;

        auto SetupMeshComponent(Entity* entity, ModelHandle model, Int32 index) -> void;

        auto CreateEntityDefault(const EntityCreateInfo& info ) -> Entity*;

    private:
        auto AddSingleEntityWithRoot(Entity * root, ModelHandle model, Int32 index ) -> void;

        auto WorkerDestroyEntity(UInt64 entityID) -> void;
        auto WorkerCreateEntity(const EntityCreateInfo& info) -> void;

        auto ComputeStats() -> void;

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

        // Stats
        UInt32 m_TotalLightCount{ 0 };
        UInt32 m_ActiveLightCount{ 0 };

        TextureHandle m_Skybox{ nullptr };
        Vec4F m_ClearColor{ 0.3f, 0.2f, 0.6f, 1.0f };

        SceneBackground m_Background{ SceneBackground::CLEAR_COLOR };

        float m_Gamma{ 2.20f };
        float m_Exposure{ 1.0f };

    };
}

#include <Scene/Scene.inl>

#endif
