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

    using namespace mikoto::core;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;

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
        renderer::LightType mLightType{ renderer::LightType::eDirectional };

        // Text config
        bool mIsText{ false };
        bool mIsWorldText{ false };

        core::f32 mTextSize{ 12.0f };
        core::f32 mTextSpacing{ 1.0f };
        eastl::string mInitialContents{};

        // PostProcess material
        renderer::rhi::TextureHandle mTextureHandle{}; // Post process apply their effects on a given texture

        auto SetName( eastl::string_view name ) -> EntityCreateInfo&;
        auto SetRoot( Entity* root ) -> EntityCreateInfo&;
        auto SetModel( asset::ModelHandle modelMesh ) -> EntityCreateInfo&;
    };

    class Scene final {
    public:
        explicit Scene( eastl::string_view name = "New Scene" );

        auto Update( float timeStep ) -> void;

        auto SetState( SceneState state ) -> void;
        auto SetName( eastl::string_view name ) -> void;

        auto RemoveEntity( core::u64 uniqueID ) -> void;

        MKT_NODISCARD auto FindByID( core::u64 uniqueID ) -> Entity*;
        MKT_NODISCARD auto FindFirstByName( eastl::string_view name ) -> Entity*;

        MKT_NODISCARD auto FindByID( core::u64 uniqueID ) const -> const Entity*;
        MKT_NODISCARD auto FindFirstByName( eastl::string_view name ) const -> const Entity*;

        MKT_NODISCARD auto ExistsByID( core::u64 uniqueID ) -> bool;
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

        // Adjusts all cameras aspect ratio to take into account the provided dimensions
        auto OnViewPortResize( core::f32 width, core::f32 height ) -> void;

        MKT_NODISCARD auto GetName() const -> eastl::string_view;
        MKT_NODISCARD auto GetEntityCount() const -> core::usize;
        MKT_NODISCARD auto GetEntities() const -> const ankerl::unordered_dense::map<core::u64, eastl::unique_ptr<Entity>>&;

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
            core::u64 EntityID{ 0 };           // only for DESTROY
        };

        auto ProcessPendingCommands() -> void;

        MKT_NODISCARD auto DestroyEntitySingle( core::u64 entityID) -> bool;
        MKT_NODISCARD auto CreateEntitySingle( const EntityCreateInfo& createInfo) -> Entity*;

        auto UpdateIdle( double deltaTime ) -> void;
        auto UpdateSimulate( double deltaTime ) -> void;

        auto OnScriptAdded(entt::registry& reg, entt::entity e ) -> void;

        auto OnRigidBodyAdded(entt::registry& reg, entt::entity e ) -> void;
        auto OnColliderAdded(entt::registry& reg, entt::entity e ) -> void;
        auto OnRigidBodyRemoved(entt::registry& reg, entt::entity e ) const -> void;
        auto OnColliderRemoved(entt::registry& reg, entt::entity e ) const -> void;

        auto SetupMeshComponent(Entity* entity, asset::ModelHandle model, core::i32 index) -> void;

        auto CreateEntityDefault(const EntityCreateInfo& info ) -> Entity*;

        auto UpdateWorldTransformations() -> void;
        auto UpdateAudioListenerAndSources() -> void;

    private:
        auto AddSingleEntityWithRoot(Entity * root, asset::ModelHandle model, core::i32 index, core::u64 animatorID = 0 ) -> void;

        auto WorkerDestroyEntity(core::u64 entityID) -> void;
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
        ankerl::unordered_dense::map<core::u64, eastl::unique_ptr<Entity>> mEntities{};

        // Entities with no parent
        // used to calculate hierarchical transform
        ankerl::unordered_dense::set<Entity*> mRootEntities{};
    };
}

#include <Scene/Scene.inl>

#endif
