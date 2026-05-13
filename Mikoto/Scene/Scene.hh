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

#include <memory>
#include <mutex>

#include <flecs.h> // TODO: test
#include <entt/entt.hpp>

#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/string_view.h>

#include <ankerl/unordered_dense.h>

#include <Assets/Model.hh>

#include <Scene/Entity.hh>
#include <Scene/Component.hh>

#include <Physics/PhysicsWorld.hh>

namespace mikoto::scene {

    enum class SceneState {
        eIdle,
        eSimulating
    };

    enum class EntityCreateType {
        eGameObject,
        eLightObject,
        eTextObject,
        eMeshObject,
    };

    struct EntityCreateInfo {
        Entity* mRoot{};
        eastl::string mName{};
        ModelHandle mModel{};

        EntityCreateType mEntityType{ EntityCreateType::eGameObject };

        // Light config
        bool mIsLight{ false };
        LightType mLightType{ LightType::eDirectional };

        // Text config
        bool mIsText{ false };
        bool mIsWorldText{ false };
        float mTextSize{ 12.0f };
        float mTextSpacing{ 1.0f };
        eastl::string mInitialContents{};

        // PostProcess material
        TextureHandle mTextureHandle{}; // Post process apply their effects on a given texture

        auto SetName( eastl::string_view name ) -> EntityCreateInfo&;
        auto SetRoot( Entity* root ) -> EntityCreateInfo&;
        auto SetModel( ModelHandle modelMesh ) -> EntityCreateInfo&;
    };

    class Scene final {
    public:
        explicit Scene( eastl::string_view name = "New Scene" );

        auto Update( float timeStep ) -> void;

        auto SetState( SceneState state ) -> void;
        auto SetName( eastl::string_view name ) -> void;

        auto RemoveEntity( u64 uniqueID ) -> void;

        MKT_NODISCARD auto FindByID( u64 uniqueID ) -> Entity*;
        MKT_NODISCARD auto FindFirstByName( eastl::string_view name ) -> Entity*;

        MKT_NODISCARD auto FindByID( u64 uniqueID ) const -> const Entity*;
        MKT_NODISCARD auto FindFirstByName( eastl::string_view name ) const -> const Entity*;

        MKT_NODISCARD auto ExistsByID( u64 uniqueID ) -> bool;
        MKT_NODISCARD auto ExistsByName( eastl::string_view name ) -> bool;

        MKT_NODISCARD auto CreateEntity( eastl::string_view name ) -> Entity*;
        MKT_NODISCARD auto CreateEntity( Entity* root, eastl::string_view name ) -> Entity*;
        MKT_NODISCARD auto CreateEntity( const EntityCreateInfo& createInfo = {} ) -> Entity*;

        MKT_NODISCARD auto GetPhysicsWorld() -> physics::PhysicsWorld*;

        MKT_NODISCARD auto GetRootEntities() const -> const ankerl::unordered_dense::set<Entity*>&;

        template<typename EntityFunction>
        auto ApplyToChildren(Entity* parent, const EntityFunction& callable) -> void;

        template<typename Callback, typename... ComponentTypes>
        auto ForAll(const Callback& c) -> void;

        auto PushEntity( eastl::string_view name ) -> void;
        auto PushEntity( const EntityCreateInfo& createInfo = {} ) -> void;

        // Call if the viewport where this scene is being rendered is resized
        auto OnViewPortResize( float width, float height ) -> void;

        MKT_NODISCARD auto GetName() const -> eastl::string_view;
        MKT_NODISCARD auto GetEntityCount() const -> size_t;
        MKT_NODISCARD auto GetEntities() const -> const ankerl::unordered_dense::map<u64, eastl::unique_ptr<Entity>>&;

        auto GetRegistry() -> entt::registry&;
        MKT_NODISCARD auto GetRegistry() const -> const entt::registry&;

        auto Clear() -> void;

        ~Scene() = default;

    private:
        friend class PhysicsWorld;

        struct EntityCommand {
            enum class Type {
                eCreate, eDestroy
            } Type{ Type::eCreate };

            EntityCreateInfo CreateInfo{};  // only for CREATE
            u64 EntityID{ 0 };           // only for DESTROY
        };

        auto ProcessPendingCommands() -> void;

        MKT_NODISCARD auto DestroyEntitySingle( u64 entityID) -> bool;
        MKT_NODISCARD auto CreateEntitySingle( const EntityCreateInfo& createInfo) -> Entity*;

        auto UpdateIdle( double deltaTime ) -> void;
        auto UpdateSimulate( double deltaTime ) -> void;

        auto OnScriptAdded(entt::registry& reg, entt::entity e ) -> void;

        auto OnRigidBodyAdded(entt::registry& reg, entt::entity e ) const -> void;
        auto OnColliderAdded(entt::registry& reg, entt::entity e ) const -> void;
        auto OnRigidBodyRemoved(entt::registry& reg, entt::entity e ) const -> void;
        auto OnColliderRemoved(entt::registry& reg, entt::entity e ) const -> void;

        auto SetupMeshComponent(Entity* entity, ModelHandle model, i32 index) -> void;

        auto CreateEntityDefault(const EntityCreateInfo& info ) -> Entity*;

        auto UpdateWorldTransformations() -> void;
        auto UpdateAudioListenerAndSources() -> void;

    private:
        auto AddSingleEntityWithRoot(Entity * root, ModelHandle model, i32 index, u64 animatorID = 0 ) -> void;

        auto WorkerDestroyEntity(u64 entityID) -> void;
        auto WorkerCreateEntity(const EntityCreateInfo& info) -> void;

    private:
        eastl::string mName{};
        entt::registry mRegistry{};

        physics::PhysicsWorld* mPhysicsWorld{};

        SceneState mSceneState{ SceneState::eIdle };

        // Deletion and creation is deferred until we call update
        std::mutex mCommandQueueMutex{};
        eastl::vector<EntityCommand> mEntityCommands{};

        // Unique because iterators are invalidated on resize
        ankerl::unordered_dense::map<u64, eastl::unique_ptr<Entity>> mEntities{};

        // Entities with no parent
        // used to calculate hierarchical transform
        ankerl::unordered_dense::set<Entity*> mRootEntities{};
    };
}

#include <Scene/Scene.inl>

#endif
