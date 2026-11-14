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

#ifndef MIKOTO_SCENE_SCENE_HH
#define MIKOTO_SCENE_SCENE_HH

#include <mutex>
#include <chrono>
#include <memory>

#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/optional.h>
#include <EASTL/shared_ptr.h>
#include <EASTL/string_view.h>
#include <EASTL/numeric_limits.h>

#include <ankerl/unordered_dense.h>

#include <entt/entt.hpp>

#include <yaml-cpp/yaml.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Serializer.hh>
#include <Core/ReferenceCounted.hh>

#include <Assets/AssetModel.hh>

#include <Renderer/Text/Font.hh>
#include <Renderer/ForwardRenderer/Passes/TonemapModule.hh>
#include <Renderer/ForwardRenderer/Passes/AtmosphericScatteringModule.hh>

#include <Scene/Entity.hh>
#include <Scene/Utility.hh>
#include <Scene/Component.hh>

namespace mikoto::physics {
    class PhysicsWorld;
    class PhysicSystem;
}// namespace mikoto::physics

namespace mikoto::renderer {
    class ParticleSystem;
    class ISceneRenderer;
    struct SceneGeometryCache;
}

namespace mikoto::scene {

    inline constexpr core::usize kNoTransformParent{ eastl::numeric_limits<core::usize>::max() };

    enum class SceneState {
        eIdle,
        ePlaying,
        eSimulating
    };
    
    enum class EnvironmentLightingSource : core::u8 {
        eEnvironmentMap,
        eSceneLightsOnly,
        eAtmosphere
    };

    struct EntityCreateInfo {
        Entity* mRoot{};
        eastl::string mName{};
        asset::ModelHandle mModel{};
        asset::AssetRef<asset::Model> mModelAsset{};

        EntityType mEntityType{ EntityType::eEmpty };

        // Light config
        renderer::LightType mLightType{ renderer::LightType::eDirectional };

        // Text config
        renderer::FontHandle mFont{};
        bool mIsWorldText{ false };

        core::f32 mTextSize{ 12.0f };
        core::f32 mTextSpacing{ 1.0f };
        eastl::string mInitialContents{};

        // PostProcess material

        auto SetRoot( Entity* root ) -> EntityCreateInfo&;
        auto SetName( eastl::string_view name ) -> EntityCreateInfo&;
        auto SetModel( asset::ModelHandle modelMesh ) -> EntityCreateInfo&;
    };

    /** Snapshot of a model creation command, read on the scene thread. */
    struct ModelCreationProgress {
        eastl::string mError{};
        core::usize mNextMesh{};
        asset::AssetRef<asset::Model> mReference{};
        asset::AssetState mState{ asset::AssetState::eUnloaded };
    };

    struct SceneStatistics {
        core::usize mEntityCount{ 0 };
        core::usize mMeshCount{ 0 };
        core::usize mLightCount{ 0 };
        core::usize mCameraCount{ 0 };
        core::usize mTextCount{ 0 };
        core::usize mRigidBodyCount{ 0 };
        core::usize mColliderCount{ 0 };
        core::usize mSpotLightCount{ 0 };
        core::usize mPointLightCount{ 0 };
        core::usize mDirectionalLightCount{ 0 };
        core::usize mScriptCount{ 0 };
        core::usize mParticleEmitterCount{ 0 };
        core::usize mAudioSourceCount{ 0 };
    };

    // Parents precede children. Dirty flags enter as local changes and leave as
    // world changes; the caller supplies fresh local flags before each update.
    // This pass touches only contiguous arrays, without ECS or hierarchy lookups.
    class Scene final : public core::ISerializable {
    public:
        // Observers retain the signal, not the scene. Connections can therefore
        // be released safely after a workspace destroys its previous scene.
        using HierarchySignal = entt::sigh<void( bool )>;

        explicit Scene( eastl::string_view name = "New Scene" );

        auto Update( core::f32 timeStep ) -> void;

        auto UpdateStreamingView( const void* owner, const Camera& camera, bool retainDistant = false ) const -> void;

        auto Update( renderer::ISceneRenderer* renderer ) -> void;
        auto SetEnvironmentLightingSource( EnvironmentLightingSource source ) -> void;

        auto SetAtmosphereSettings( const renderer::AtmosphereSettings& settings ) -> void;
        auto SetToneMapping( renderer::ToneMapper type ) -> void;

        MKT_NODISCARD auto GetEnvironmentLightingSource() const -> EnvironmentLightingSource;
        MKT_NODISCARD auto GetAtmosphereSettings() const -> const renderer::AtmosphereSettings&;

        MKT_NODISCARD auto GetToneMapping() const -> renderer::ToneMapper;
        MKT_NODISCARD auto GetEnvironmentMaterial() const -> material::MaterialHandle;

        MKT_NODISCARD auto SerializeText( const filesystem::Path &filename, bool copyMaterials = false ) const -> eastl::string;

        auto Deserialize( const filesystem::Path& filename ) -> void override;
        auto Serialize( const filesystem::Path& filename ) const -> void override;

        auto Deserialize( filesystem::FileHandle file ) -> void override;
        auto Serialize( filesystem::FileHandle file ) const -> void override;

        static auto ConvertSceneAssetPaths( YAML::Node node, const filesystem::Path& sceneFile, bool relative ) -> void;

        auto SetState( SceneState state ) -> void;
        auto SetName( eastl::string_view name ) -> void;

        auto RemoveEntity( core::u64 uniqueID ) -> void;

        // Highlight a subtree only when its root changes. Entity creation and
        // destruction maintain membership without scanning the scene each frame.
        auto SetHighlightRoot( core::u64 uniqueID ) -> void;
        auto SetHighlightRoots( const eastl::vector<core::u64>& roots ) -> void;

        MKT_NODISCARD auto GetHierarchySignal() const -> eastl::shared_ptr<HierarchySignal>;
        MKT_NODISCARD auto GetGeometryCache() const -> eastl::shared_ptr<renderer::SceneGeometryCache>&;

        auto NotifyHierarchyChanged( bool structureChanged ) -> void;

        MKT_NODISCARD auto ReparentEntity( core::u64 entityID, core::u64 parentID ) -> bool;
        MKT_NODISCARD auto SetLocalTransform( core::u64 entityID, const core::float4x4& local, bool transformChildren = true ) -> bool;

        MKT_NODISCARD auto IsPlaying() const -> bool;
        MKT_NODISCARD auto GetVolumeTime() const -> core::f64 { return mVolumeTime; }
        MKT_NODISCARD auto IsSimulating() const -> bool;

        MKT_NODISCARD auto GetSceneStats() const -> const SceneStatistics&;

        MKT_NODISCARD auto FindByID( core::u64 uniqueID ) -> Entity*;
        MKT_NODISCARD auto FindByHandle( entt::entity handle ) -> Entity*;
        MKT_NODISCARD auto FindFirstByName( eastl::string_view name ) -> Entity*;

        MKT_NODISCARD auto DuplicateEntity( Entity* other ) -> Entity*;

        MKT_NODISCARD auto FindByID( core::u64 uniqueID ) const -> const Entity*;
        MKT_NODISCARD auto FindFirstByName( eastl::string_view name ) const -> const Entity*;

        MKT_NODISCARD auto ExistsByID( core::u64 uniqueID ) -> bool;
        MKT_NODISCARD auto ExistsByName( eastl::string_view name ) -> bool;

        MKT_NODISCARD auto CreateGroup( eastl::string_view name, Entity* container = nullptr ) -> Entity*;

        MKT_NODISCARD auto SetEntityGroup( core::u64 entityID, core::u64 groupID ) -> bool;
        MKT_NODISCARD auto GetEntityGroup( core::u64 entityID ) const -> core::u64;
        MKT_NODISCARD auto GetHierarchyParent( core::u64 entityID ) const -> core::u64;
        
        MKT_NODISCARD auto MoveEntityInHierarchy( core::u64 entityID, core::u64 targetID ) -> bool;

        MKT_NODISCARD auto CreateEntity( eastl::string_view name ) -> Entity*;
        MKT_NODISCARD auto CreateEntity( Entity* root, eastl::string_view name ) -> Entity*;
        MKT_NODISCARD auto CreateEntity( const EntityCreateInfo& createInfo = {} ) -> Entity*;

        MKT_NODISCARD auto GetPhysicsWorld() const -> physics::PhysicsWorld*;

        MKT_NODISCARD auto GetRootEntities() const -> const ankerl::unordered_dense::set<Entity*>&;

        template<typename EntityFunction>
        auto ApplyToChildren( Entity* parent, const EntityFunction& callable ) -> void;

        template<typename Callback, typename... ComponentTypes>
        auto ForAll( const Callback& c ) -> void;

        template<typename Callback, typename... ComponentTypes>
        auto ForEachGroup( const Callback& c ) -> void;

        auto PushEntity( eastl::string_view name ) -> void;
        auto PushEntity( const EntityCreateInfo& createInfo = {} ) -> void;

        auto OnViewPortResize( core::f32 width, core::f32 height ) -> void;

        MKT_NODISCARD auto GetName() const -> eastl::string_view;
        MKT_NODISCARD auto GetEntityCount() const -> core::usize;
        MKT_NODISCARD auto GetRegistry() const -> const entt::registry&;
        MKT_NODISCARD auto GetModelCreationProgress( core::u64 rootID ) const -> eastl::optional<ModelCreationProgress>;
        MKT_NODISCARD auto GetEntities() const -> const ankerl::unordered_dense::map<core::u64, eastl::unique_ptr<Entity>>&;

        auto Clear() -> void;
        auto GetRegistry() -> entt::registry&;

        ~Scene() override;

    private:
        // TODO: Remove these are from a different module
        friend class physics::PhysicsWorld;
        friend class physics::PhysicSystem;
        friend class renderer::ParticleSystem;

        struct EntityCommand {
            enum class Type {
                eCreate,
                eDestroy
            };

            Type mType{ Type::eCreate };
            EntityCreateInfo mCreateInfo{};
            core::u64 mEntityID{};
            core::u64 mParentID{};
            core::u64 mModelRootID{};
            core::u64 mAnimatorID{};
            core::usize mNextMesh{};
            asset::AssetHandle<asset::Model> mModelRequest{};
            bool mFailureReported{};
        };

    private:
        MKT_NODISCARD static auto UpdateWorldTransforms(
            eastl::span<const core::usize> parents,
            eastl::span<const core::float4x4> localTransforms,
            eastl::span<core::float4x4> worldTransforms,
            eastl::span<core::u8> dirty ) -> core::usize;

        auto ProcessPendingCommands() -> void;
        auto CollectEntityCommands() -> void;
        auto ProcessDestroyCommands() -> void;

        auto UpdateMeshResidency() -> void;
        auto ResumeModelCreation( core::u64 rootID, const asset::AssetRef<asset::Model>& reference, core::usize nextMesh ) -> void;
        
        MKT_NODISCARD auto ProcessCreateCommand( EntityCommand& command, core::usize& created ) -> bool;
        MKT_NODISCARD auto ResolveCommandModel( EntityCommand& command ) -> asset::ModelHandle;

        MKT_NODISCARD auto IsMeshResidencyRequired( entt::entity entity, const MeshComponent& mesh,
            const TransformComponent& transform, const TagComponent& tag ) const -> bool;

        MKT_NODISCARD auto DestroyEntitySingle( core::u64 entityID ) -> bool;
        MKT_NODISCARD auto WouldCreateHierarchyCycle( core::u64 entityID, core::u64 parentID ) const -> bool;
        MKT_NODISCARD auto CreateEntitySingle( const EntityCreateInfo& createInfo ) -> Entity*;

        auto UpdatePlaying( core::f64 timeStep ) -> void;

        auto OnScriptAdded( entt::registry& reg, entt::entity e ) -> void;
        auto OnRigidBodyAdded( entt::registry& reg, entt::entity e ) -> void;
        auto OnParticleEmitterAdded( entt::registry& reg, entt::entity e ) -> void;

        template<typename Collider>
        auto OnColliderAdded( entt::registry& reg, entt::entity e ) -> void;
        auto OnColliderRemoved( entt::registry& reg, entt::entity e ) -> void;
        auto OnRigidBodyRemoved( entt::registry& reg, entt::entity e ) -> void;

        auto OnTransformStorageChanged( entt::registry& registry, entt::entity entity ) -> void;

        auto RebuildTransformOrder() -> void;
        auto UpdateWorldTransformations() -> void;
        auto UpdateAudioListenerAndSources( core::f32 timeStep ) -> void;
        
        auto WorkerDestroyEntity( core::u64 entityID ) -> void;
        auto WorkerCreateEntity( const EntityCreateInfo &info ) -> void;

        auto CreateEntityDefault( const EntityCreateInfo& info ) -> Entity*;
        auto SetupMeshComponent( Entity* entity, asset::ModelHandle model, core::i32 index ) -> void;
        auto AddSingleEntityWithRoot( Entity *root, asset::ModelHandle model, core::i32 index, core::u64 animatorID = 0 ) -> void;
    
private:
        struct StreamingView {
            const void *mOwner{};
            core::float3 mPosition{};
            core::f32 mRange{};
            std::chrono::steady_clock::time_point mLastSeen{};
        };

        struct ImportedMaterials {
            asset::ModelHandle mModel{};
            ankerl::unordered_dense::map<core::u64, material::MaterialHandle> mMaterials{};
        };

    private:
        eastl::string mName{};
        entt::registry mRegistry{};
        SceneState mSceneState{ SceneState::eIdle };

        // Keep entity IDs that configure the scene
        
        renderer::AtmosphereSettings mAtmosphereSettings{};
        renderer::ToneMapper mToneMapping{ renderer::ToneMapper::eAces };

        EnvironmentLightingSource mEnvironmentLightingSource{ EnvironmentLightingSource::eEnvironmentMap };

        physics::PhysicsWorld* mPhysicsWorld{};

        core::f64 mVolumeTime{};

        // Deletion and creation is deferred until we call update
        mutable std::mutex mCommandQueueMutex{};
        eastl::vector<EntityCommand> mEntityCommands{};
        eastl::vector<EntityCommand> mPendingEntityCommands{};

        core::u64 mCommandGeneration{};
        bool mProcessingEntityCommands{};

        eastl::vector<entt::entity> mScriptUpdateEntities{};

        // Unique because iterators are invalidated on resize
        ankerl::unordered_dense::map<core::u64, eastl::unique_ptr<Entity>> mEntities{};

        // Entities with no parent
        // used to calculate hierarchical transform
        ankerl::unordered_dense::set<Entity*> mRootEntities{};

        // Parallel arrays in parent-first order. Entity handles remain valid
        // across EnTT storage compaction; component pointers would not.
        eastl::vector<core::u8> mTransformDirty{};
        eastl::vector<core::usize> mTransformParents{};
        eastl::vector<entt::entity> mTransformEntities{};
        eastl::vector<core::float4x4> mLocalTransforms{};
        eastl::vector<core::float4x4> mWorldTransforms{};

        bool mTransformOrderDirty{ true };
        mutable eastl::shared_ptr<renderer::SceneGeometryCache> mGeometryCache{};

        // Asset streaming
        mutable eastl::vector<StreamingView> mStreamingViews{};
        std::chrono::steady_clock::time_point mResidencyUpdate{};

        // Scene-owned so Play edits cannot mutate the editor scene's materials.
        ankerl::unordered_dense::map<const asset::Model*, ImportedMaterials> mImportedMaterials{};

        core::u64 mHighlightRoot{};
        eastl::vector<core::u64> mHighlightRoots{};
        eastl::vector<core::u64> mHighlightPending{};
        ankerl::unordered_dense::set<core::u64> mHighlightedEntities{};
        ankerl::unordered_dense::set<core::u64> mNextHighlightedEntities{};

        bool mHighlightsDirty{ false };
        eastl::shared_ptr<HierarchySignal> mHierarchySignal{};

        // This is updated from when we create/destroy entities
        SceneStatistics mStatistics{};
    };

    using SceneHandle = core::Ref<Scene>;

    class SceneSerializer final : public core::ISerializer {
    public:
        auto Serialize( const core::ISerializable* serializable, const filesystem::Path& saveFilePath ) -> void override;
        auto Deserialize( const filesystem::Path& saveFilePath ) -> core::Ref<core::ISerializable> override;
    };
}// namespace mikoto::scene

#include <Scene/Scene.inl>

#endif// MIKOTO_SCENE_SCENE_HH
