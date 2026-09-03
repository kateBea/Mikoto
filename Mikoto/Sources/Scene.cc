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

#include <EASTL/algorithm.h>
#include <EASTL/memory.h>
#include <EASTL/optional.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/utility.h>

#include <entt/entt.hpp>

#include <yaml-cpp/yaml.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Profiler.hh>
#include <Core/Platform.hh>
#include <Core/RuntimeConsole.hh>

#include <Math/Random.hh>

#include <Audio/AudioService.hh>

#include <Physics/PhysicsWorld.hh>
#include <Physics/PhysicSystem.hh>

#include <Animation/AnimationSystem.hh>

#include <Renderer/Core/RenderSystem.hh>
#include <Scripting/ScriptingService.hh>

#include <Scene/Scene.hh>
#include <Scene/Component.hh>

namespace YAML {
    using namespace mikoto::core;

    template<>
    struct convert<float3> {
        static auto encode( const float3& rhs ) -> Node {
            Node node{};
            node.push_back( rhs.x );
            node.push_back( rhs.y );
            node.push_back( rhs.z );
            return node;
        }

        static auto decode( const Node& node, float3& rhs ) -> bool {
            if ( !node.IsSequence() || node.size() != float3::length() ) {
                return false;
            }

            rhs.x = node[0].as<float3::type::value_type>();
            rhs.y = node[1].as<float3::type::value_type>();
            rhs.z = node[2].as<float3::type::value_type>();

            return true;
        }
    };

    template<>
    struct convert<float4> {
        static auto encode( const float4& rhs ) -> Node {
            Node node{};
            node.push_back( rhs.x );
            node.push_back( rhs.y );
            node.push_back( rhs.z );
            node.push_back( rhs.w );
            return node;
        }

        static auto decode( const Node& node, float4& rhs ) -> bool {
            if ( !node.IsSequence() || node.size() != glm::vec4::length() ) {
                return false;
            }

            rhs.x = node[0].as<float4::type::value_type>();
            rhs.y = node[1].as<float4::type::value_type>();
            rhs.z = node[2].as<float4::type::value_type>();
            rhs.w = node[3].as<float4::type::value_type>();

            return true;
        }
    };

    template<>
    struct convert<float4x4> {
        static auto encode(const float4x4& rhs) -> Node {
            Node node{};
            node.push_back(rhs[0]);
            node.push_back(rhs[1]);
            node.push_back(rhs[2]);
            node.push_back(rhs[3]);
            return node;
        }

        static auto decode(const Node& node, float4x4& rhs) -> bool {
            if (!node.IsSequence() || node.size() != 4) {
                return false;
            }

            rhs[0] = node[0].as<float4>();
            rhs[1] = node[1].as<float4>();
            rhs[2] = node[2].as<float4>();
            rhs[3] = node[3].as<float4>();

            return true;
        }
    };
}// namespace YAML

namespace mikoto::scene {

    // https://skypjack.github.io/entt/index.html

    using namespace mikoto::core;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;

    static auto UpdateWorldTransform( Entity& e, float4x4 parentWorld, Scene* scene ) -> void {
        auto& t{ e.GetComponent<TransformComponent>() };

        t.SetWorldTransform( parentWorld * t.GetTransform() );

        auto& rc{ e.GetComponent<RelationComponent>() };

        for ( auto child: rc.GetChildren() ) {
            Entity* childEntity{ scene->FindByID( child ) };
            if ( childEntity == nullptr ) {
                continue;
            }

            UpdateWorldTransform( *childEntity, t.GetWorldTransform(), scene );
        }
    }

    static auto OnSkyboxMaterialAdded( entt::registry& reg, entt::entity e ) -> void {
        auto& sbComponent{ reg.get<SkyboxMaterialComponent>( e ) };
        sbComponent.SetMaterial( AssetsService::Get()->CreateMaterial( SkyboxMaterialDescription{} ) );
    }

    static auto OnMeshRendererAdded( entt::registry& reg, entt::entity e ) -> void {
        if ( !reg.any_of<MaterialComponent>( e ) ) {
            reg.emplace<MaterialComponent>( e );
        }

        auto& meshComponent{ reg.get<MeshComponent>( e ) };
        auto meshNode{ meshComponent.GetMesh() };

        auto& material{ reg.get<MaterialComponent>( e ) };

        if ( meshNode ) {
            material.SetMaterial( AssetsService::Get()->CreateMaterial( meshNode->GetProperties() ) );
        } else {
            material.SetMaterial( AssetsService::Get()->CreateMaterial( PhysicMaterialDescription{} ) );
        }
    }

    static auto OnSkinnedMeshRendererAdded( entt::registry& reg, entt::entity e ) -> void {
    }

    static auto OnAudioListenerAdded( entt::registry& reg, entt::entity e ) -> void {
        static bool listenerActive{ false };

        auto& listenerComponent{ reg.get<AudioListenerComponent>( e ) };
        listenerComponent.SetListener( AudioService::Get()->CreateListener() );

        // Generally we need one listener, if we switch scenes we might want
        // to disable the listener for the active scene
        if ( !listenerActive ) {
            listenerActive = true;
        } else {
            RuntimeConsole::Get()->Warning( "There is already an active listener. Consider reusing it." );
        }
    }

    static auto OnAnimatorAdded( entt::registry& reg, entt::entity e ) -> void {
        auto& animator{ reg.get<AnimatorComponent>( e ) };
    }

    Scene::Scene( const eastl::string_view name )
        : mName{ name } {

        // Renderer
        mRegistry.on_construct<MeshComponent>().connect<&OnMeshRendererAdded>();

        mRegistry.on_construct<SkyboxMaterialComponent>().connect<&OnSkyboxMaterialAdded>();

        // Animation
        mRegistry.on_construct<AnimatorComponent>().connect<&OnAnimatorAdded>();
        mRegistry.on_construct<SkinnedMeshRenderer>().connect<&OnSkinnedMeshRendererAdded>();

        // Audio
        mRegistry.on_construct<AudioListenerComponent>().connect<&OnAudioListenerAdded>();

        // Script
        mRegistry.on_construct<ScriptComponent>().connect<&Scene::OnScriptAdded>( this );

        // Physics
        mRegistry.on_construct<RigidBodyComponent>().connect<&Scene::OnRigidBodyAdded>( this );
        mRegistry.on_construct<MeshColliderComponent>().connect<&Scene::OnColliderAdded>( this );

        physics::PhysicsWorldCreateInfo spec{
            .mScene = this,
            .mGravity = physics::PhysicsWorld::GetGravityFor( physics::GravityBody::eEarth ) };
        mPhysicsWorld = physics::PhysicSystem::Get()->CreatePhysicsWorld( spec );

#if MIKOTO_DEBUG
        mRegistry.storage<entt::entity>()
            .on_destroy()
            .connect<&Scene::OnEntityRemoved>( this );

        mRegistry.storage<entt::entity>()
            .on_construct()
            .connect<&Scene::OnEntityAdded>( this );
#endif
    }

    auto Scene::UpdateIdle( double timeStep ) -> void {

    }

    auto Scene::UpdateSimulate( double timeStep ) -> void {
        physics::PhysicSystem::Get()->SetSimulationTarget( this );

        // Update scripts (Ideally only when playing not when simulating)
        auto scriptEntities{ mRegistry.view<ScriptComponent>() };
        for ( auto& entity: scriptEntities ) {
            ScriptComponent& script{ mRegistry.get<ScriptComponent>( entity ) };
            ScriptHandle handle{ script.GetHandle() };

            if ( !handle.IsEmpty() ) {
                handle->Update( timeStep );
            }
        }
    }

    auto Scene::UpdatePlaying( double timeStep ) -> void {
        // Update scripts (Ideally only when playing not when simulating)
        auto scriptEntities{ mRegistry.view<ScriptComponent>() };
        for ( auto& entity: scriptEntities ) {
            ScriptComponent& script{ mRegistry.get<ScriptComponent>( entity ) };
            ScriptHandle handle{ script.GetHandle() };

            if ( !handle.IsEmpty() ) {
                handle->Update( timeStep );
            }
        }
    }

    auto Scene::OnEntityAdded( entt::registry& reg, entt::entity e ) -> void {

    }

    auto Scene::OnEntityRemoved( entt::registry& reg, entt::entity e ) -> void {

    }

    auto Scene::OnRigidBodyAdded( entt::registry& reg, entt::entity e ) -> void {
        if ( !reg.any_of<BoxColliderComponent>( e ) ) {
            reg.emplace_or_replace<BoxColliderComponent>( e );
        } else if ( !reg.any_of<SphereColliderComponent>( e ) ) {
            reg.emplace_or_replace<SphereColliderComponent>( e );
        }else if ( !reg.any_of<CapsuleColliderComponent>( e ) ) {
            reg.emplace_or_replace<CapsuleColliderComponent>( e );
        }else if ( !reg.any_of<MeshColliderComponent>( e ) ) {
            reg.emplace_or_replace<MeshColliderComponent>( e );
        }

        const TagComponent& tag{ reg.get<TagComponent>( e ) };
        Entity* entity{ FindByID( tag.GetGuid() ) };
        mPhysicsWorld->AddRigidBody( entity );
    }

    auto Scene::OnColliderAdded( entt::registry& reg, entt::entity e ) -> void {
        // It is possible to have a collider and no rigid body component,
        // this is because a collisions do not only happen with rigid bodies
        // collision areas can be used to trigger effects upon entering certain areas
        const TagComponent& tag{ reg.get<TagComponent>( e ) };
        Entity* entity{ FindByID( tag.GetGuid() ) };
        mPhysicsWorld->AddCollider( entity );
    }

    auto Scene::OnRigidBodyRemoved( entt::registry& reg, entt::entity e ) -> void {
        if (!mRegistry.valid( e ) || !mRegistry.any_of<RigidBodyComponent>( e )) {
            return;
        }

        const TagComponent& tag{ reg.get<TagComponent>( e ) };
        Entity* entity{ FindByID( tag.GetGuid() ) };
        mPhysicsWorld->RemoveRigidBody( entity );
    }

    auto Scene::OnColliderRemoved( entt::registry& reg, entt::entity e ) -> void {
        if (mRegistry.orphan(e)) {
            return;
        }

        // This does not remove the rigid body component. The idea is that
        // if there is no collider nobody will not respond to any type of collisions
        // Could use a custom collision (empty) in Jolt's side maybe? WIP
        // An alternative is Empty shape
        // https://jrouwe.github.io/JoltPhysics/class_empty_shape.html

        // More complex shapes can be created with vertices
        const TagComponent& tag{ reg.get<TagComponent>( e ) };
        Entity* entity{ FindByID( tag.GetGuid() ) };
        mPhysicsWorld->RemoveColliderBody( entity );
    }

    auto Scene::OnScriptAdded( entt::registry& reg, entt::entity e ) -> void {
        TagComponent& tag{ reg.get<TagComponent>( e ) };
        ScriptComponent& scriptComponent{ reg.get<ScriptComponent>( e ) };

        ScriptHandle scriptHandle{};
        if ( Entity* entity{ FindByID( tag.GetGuid() ) } ) {
            if (!scriptComponent.GetFilePath().IsEmpty()) {
                // Component created with a path to an existing script
                scriptHandle = ScriptingService::Get()->LoadScript( scriptComponent.GetFilePath(), entity );
            } else {
                // No script specified, created blank script
                scriptHandle = ScriptingService::Get()->CreateScript( entity );
            }
        }

        scriptComponent.SetScript( scriptHandle );
    }

    auto Scene::SetupMeshComponent( Entity* entity, ModelHandle model, i32 index ) -> void {
        MKT_ASSERT( index > -1, "Index must be a positive integer" );
        MKT_ASSERT( entity != nullptr, "Entity cannot bet null" );
        MKT_ASSERT( !model.IsEmpty(), "Model cannot be empty" );

        entity->AddComponent<MeshComponent>( model, index );

        MeshNode& meshNode{ model->GetMeshNode( index ) };

        // Meshes have their own transformation from whatever modeling tool they were used
        // We need to apply it to the mesh local transform in order to properly place it in our world
        TransformComponent& transform{ entity->GetComponent<TransformComponent>() };
        transform.SetTransform( meshNode.GetTransform() );

        if ( !entity->HasComponent<MaterialComponent>() ) {
            entity->AddComponent<MaterialComponent>( AssetsService::Get()->CreateMaterial( meshNode.GetProperties() ) );
        }
    }

    auto Scene::RemoveEntity( u64 uniqueID ) -> void {
        mEntityCommands.emplace_back( EntityCommand{
                .Type = EntityCommand::Type::eDestroy,
                .EntityID = uniqueID,
        } );
    }

    auto Scene::SetState( const SceneState state ) -> void {
        mSceneState = state;
    }

    auto Scene::UpdateWorldTransformations() -> void {
        float4x4 Identity{ 1.0f };

        for ( Entity* rootEntity: mRootEntities ) {
            UpdateWorldTransform( *rootEntity, Identity, this );
        }
    }

    auto Scene::UpdateAudioListenerAndSources() -> void {
        // Update listeners
        auto viewListeners{ mRegistry.view<TransformComponent, AudioListenerComponent>() };
        for ( const auto& entity: viewListeners ) {
            TransformComponent& transformComponent{ mRegistry.get<TransformComponent>( entity ) };
            AudioListenerComponent& audioListenerComponent{ mRegistry.get<AudioListenerComponent>( entity ) };

            if ( audioListenerComponent.IsActive() ) {
                audioListenerComponent.GetListener().SetPosition( transformComponent.GetTranslation() );
                audioListenerComponent.GetListener().Apply();
            }
        }

        // Update sources
        auto viewSources{ mRegistry.view<TransformComponent, AudioSourceComponent>() };
        for ( const auto& entity: viewSources ) {
            TransformComponent& transformComponent{ mRegistry.get<TransformComponent>( entity ) };
            AudioSourceComponent& audioSourceComponent{ mRegistry.get<AudioSourceComponent>( entity ) };

            AudioSourceHandle source{ audioSourceComponent.GetSource() };
            if ( !source.IsEmpty() ) {
                source->SetPosition( transformComponent.GetTranslation() );
            }
        }
    }

    auto Scene::Update( const float timeStep ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        ProcessPendingCommands();

        switch ( mSceneState ) {
            case SceneState::eIdle:
                UpdateIdle( timeStep );
                break;
            case SceneState::eSimulating:
                UpdateSimulate( timeStep );
                break;
            case SceneState::ePlaying:
                UpdatePlaying( timeStep );
                break;
            default:;
        }

        UpdateWorldTransformations();
        UpdateAudioListenerAndSources();
    }

    auto Scene::Serialize( const filesystem::Path& filename ) const -> void {

    }

    auto Scene::Deserialize( const filesystem::Path& filename ) const -> void {

    }

    auto Scene::SetName( const eastl::string_view name ) -> void {
        mName = name;
    }

    auto Scene::FindByID( const u64 uniqueID ) -> Entity* {
        const auto it{ mEntities.find( uniqueID ) };
        return it != mEntities.end() ? it->second.get() : nullptr;
    }

    auto Scene::FindFirstByName( const eastl::string_view name ) -> Entity* {
        const auto it{ std::ranges::find_if( mEntities, [&]( auto& pair ) -> bool {
            const auto& entity{ pair.second };

            // All entities have tag component you can only create
            // them from the CreateEntity method
            const auto& tag{ entity->template GetComponent<TagComponent>() };
            return tag.GetTag() == name;
        } ) };

        return it != mEntities.end() ? it->second.get() : nullptr;
    }

    auto Scene::FindByID( const u64 uniqueID ) const -> const Entity* {
        const auto it{ mEntities.find( uniqueID ) };
        return it != mEntities.end() ? it->second.get() : nullptr;
    }

    auto Scene::FindFirstByName( const eastl::string_view name ) const -> const Entity* {
        const auto it{ std::ranges::find_if( mEntities, [&]( auto& pair ) -> bool {
            const auto& entity{ pair.second };

            // All entities have tag component you can only create
            // them from the CreateEntity method
            const auto& tag{ entity->template GetComponent<TagComponent>() };
            return tag.GetTag() == name;
        } ) };

        return it != mEntities.end() ? it->second.get() : nullptr;
    }

    auto Scene::ExistsByID( const u64 uniqueID ) -> bool {
        return FindByID( uniqueID ) != nullptr;
    }

    auto Scene::ExistsByName( const eastl::string_view name ) -> bool {
        return FindFirstByName( name ) != nullptr;
    }

    auto Scene::GetName() const -> eastl::string_view {
        return mName;
    }

    auto Scene::GetEntities() const -> const ankerl::unordered_dense::map<u64, eastl::unique_ptr<Entity>>& {
        return mEntities;
    }

    auto Scene::Clear() -> void {
        mEntities.clear();
        mRegistry.clear();
    }

    auto Scene::CreateEntityDefault( const EntityCreateInfo& info ) -> Entity* {
        const auto entity{ new Entity{ mRegistry, info.mEntityType } };

        // [Constants for default entity parameters]
        constexpr float3 initialSize{ 1.0f, 1.0f, 1.0f };
        constexpr float3 initialPosition{ 0.0, 0.0, 0.0 };
        constexpr float3 initialRotation{ 0.0f, 0.0f, 0.0f };

        if ( info.mRoot != nullptr ) {
            const TagComponent& parentTag{ info.mRoot->GetComponent<TagComponent>() };
            entity->AddComponent<RelationComponent>( std::make_optional( parentTag.GetGuid() ) );
        } else {
            entity->AddComponent<RelationComponent>();
        }

        entity->AddComponent<HighlightComponent>();
        entity->AddComponent<TagComponent>( info.mName );

        entity->AddComponent<TransformComponent>( initialPosition, initialSize, initialRotation );

        if ( info.mRoot == nullptr ) {
            mRootEntities.emplace( entity );
        }

        return entity;
    }

    auto Scene::DuplicateEntity( Entity* other ) -> Entity* {
        if (!other) {
            return nullptr;
        }

        const TagComponent& otherTagComponent{ other->GetComponent<TagComponent>() };
        const RelationComponent& otherRelationComponent{ other->GetComponent<RelationComponent>() };

        Entity* otherParent{ nullptr };
        if (otherRelationComponent.HasParent()) {
            otherParent = FindByID( *otherRelationComponent.GetParent() );
        }

        EntityCreateInfo createInfo{
            .mRoot{ otherParent },
            .mName{ otherTagComponent.GetTag() },
            .mEntityType = other->GetType() };
        if (other->HasComponent<MeshComponent>()) {
            const MeshComponent& otherMeshComponent{ other->GetComponent<MeshComponent>() };
            //createInfo.mModel = otherMeshComponent.GetModel();
        }

        Entity* result{ CreateEntityDefault( createInfo ) };
        const u64 guid{ result->GetComponent<TagComponent>().GetGuid() };
        const auto [it, success]{
            mEntities.try_emplace( guid, result ) };
        if ( success ) {
            result = it->second.get();

            if ( createInfo.mEntityType == EntityType::eLight ) {
                result->AddComponent<LightComponent>( createInfo.mLightType );
            }

            if ( createInfo.mEntityType == EntityType::eText ) {
                result->AddComponent<TextComponent>(
                        createInfo.mInitialContents,
                        createInfo.mTextSize,
                        createInfo.mTextSpacing,
                        createInfo.mIsWorldText );
            }

            // if root is not empty this entity must be registered as child of root entity
            if ( createInfo.mRoot != nullptr ) {
                Entity* parent{ createInfo.mRoot };
                RelationComponent& parentRelation{ parent->GetComponent<RelationComponent>() };

                parentRelation.RegisterChild( guid );
            }

            // in root model is not empty, we create the children for this entity each children well hold a mesh
            if ( !createInfo.mModel.IsEmpty() ) {
                result->SetType( EntityType::eMesh );

                if ( createInfo.mModel->GetMeshNodeCount() > 1 ) {
                    u64 animatorID{};

                    if ( createInfo.mModel->IsSkinned() ) {
                        animatorID = AnimationSystem::Get()->RegisterAnimation( createInfo.mModel );
                        result->AddComponent<AnimatorComponent>( animatorID );
                    }

                    for ( usize index{}; index < createInfo.mModel->GetMeshNodeCount(); index++ ) {
                        AddSingleEntityWithRoot( result, createInfo.mModel, index, animatorID );
                    }

                } else {
                    if ( createInfo.mModel->IsSkinned() ) {
                        u64 animatorID{ AnimationSystem::Get()->RegisterAnimation( createInfo.mModel ) };
                        result->AddComponent<AnimatorComponent>( animatorID );
                        result->AddComponent<SkinnedMeshRenderer>( animatorID );
                    }

                    SetupMeshComponent( result, createInfo.mModel, 0 );
                }
            }
        }
    }

    auto Scene::CreateEntitySingle( const EntityCreateInfo& createInfo ) -> Entity* {
        MKT_BEGIN_PROFILER_NAMED();

        Entity* result{ CreateEntityDefault( createInfo ) };

        const u64 guid{ result->GetComponent<TagComponent>().GetGuid() };
        const auto [it, success]{
            mEntities.try_emplace( guid, result )
        };

        if ( success ) {
            result = it->second.get();

            if ( createInfo.mEntityType == EntityType::eLight ) {
                result->AddComponent<LightComponent>( createInfo.mLightType );
            }

            if ( createInfo.mEntityType == EntityType::eText ) {
                result->AddComponent<TextComponent>(
                        createInfo.mInitialContents,
                        createInfo.mTextSize,
                        createInfo.mTextSpacing,
                        createInfo.mIsWorldText );
            }

            // if root is not empty this entity must be registered as child of root entity
            if ( createInfo.mRoot != nullptr ) {
                Entity* parent{ createInfo.mRoot };
                RelationComponent& parentRelation{ parent->GetComponent<RelationComponent>() };

                parentRelation.RegisterChild( guid );
            }

            // in root model is not empty, we create the children for this entity each children well hold a mesh
            if ( !createInfo.mModel.IsEmpty() ) {
                result->SetType( EntityType::eMesh );

                if ( createInfo.mModel->GetMeshNodeCount() > 1 ) {
                    u64 animatorID{};

                    if ( createInfo.mModel->IsSkinned() ) {
                        animatorID = AnimationSystem::Get()->RegisterAnimation( createInfo.mModel );
                        result->AddComponent<AnimatorComponent>( animatorID );
                    }

                    for ( usize index{}; index < createInfo.mModel->GetMeshNodeCount(); index++ ) {
                        AddSingleEntityWithRoot( result, createInfo.mModel, index, animatorID );
                    }

                } else {
                    if ( createInfo.mModel->IsSkinned() ) {
                        u64 animatorID{ AnimationSystem::Get()->RegisterAnimation( createInfo.mModel ) };
                        result->AddComponent<AnimatorComponent>( animatorID );
                        result->AddComponent<SkinnedMeshRenderer>( animatorID );
                    }

                    SetupMeshComponent( result, createInfo.mModel, 0 );
                }
            }
        }

        return result;
    }

    auto Scene::CreateEntity( const EntityCreateInfo& createInfo ) -> Entity* {
        return CreateEntitySingle( createInfo );
    }

    auto Scene::GetPhysicsWorld() -> physics::PhysicsWorld* {
        return mPhysicsWorld;
    }

    auto Scene::GetRootEntities() const -> const ankerl::unordered_dense::set<Entity*>& {
        return mRootEntities;
    }

    auto Scene::PushEntity( eastl::string_view name ) -> void {
        const EntityCreateInfo info{
            .mRoot = nullptr,
            .mName = name.data(),
        };

        PushEntity( info );
    }

    auto Scene::PushEntity( const EntityCreateInfo& createInfo ) -> void {
        // Lock and push the creation command
        std::lock_guard lock( mCommandQueueMutex );
        mEntityCommands.push_back( { EntityCommand::Type::eCreate, createInfo } );
    }

    auto Scene::CreateEntity( eastl::string_view name ) -> Entity* {
        const EntityCreateInfo info{
            .mRoot{ nullptr },
            .mName{ name.data() },
            .mModel{ ModelHandle::CreateEmpty() }
        };

        return CreateEntity( info );
    }

    auto Scene::CreateEntity( Entity* root, eastl::string_view name ) -> Entity* {
        const EntityCreateInfo info{
            .mRoot{ root },
            .mName{ name.data() },
            .mModel{ ModelHandle::CreateEmpty() }
        };

        return CreateEntity( info );
    }

    auto Scene::GetEntityCount() const -> size_t {
        return mEntities.size();
    }

    auto Scene::OnViewPortResize( const float width, const float height ) -> void {
        // Resize non-fixed aspect ratio cameras
        for ( const auto& entity: mRegistry.view<CameraComponent>() ) {
            CameraComponent& cameraComponent{ mRegistry.get<CameraComponent>( entity ) };

            if ( !cameraComponent.IsAspectRatioFixed() ) {
                cameraComponent.GetCamera().SetViewportSize( width, height );
            }
        }
    }

    auto Scene::ProcessPendingCommands() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Temporary vector to hold commands to process this frame
        eastl::vector<EntityCommand> commandsCopy{};

        {
            // Lock the command queue so no other thread modifies it while we swap
            std::lock_guard lock( mCommandQueueMutex );

            // Swap the current queue with our temporary vector
            // This effectively takes ownership of all pending commands
            // and clears the original queue so other threads can keep pushing, swap is O(1)
            commandsCopy.swap( mEntityCommands );
        }

        // Now we can process the commands without holding the lock
        // This avoids blocking other threads that want to enqueue new commands
        for ( auto& [Type, CreateInfo, EntityID]: commandsCopy ) {
            switch ( Type ) {
                case EntityCommand::Type::eCreate:
                    // Create the entity immediately on the main thread
                    // Safe because only the main thread writes to the registry
                    ( void )CreateEntitySingle( CreateInfo );
                    break;

                case EntityCommand::Type::eDestroy:
                    // Destroy the entity immediately on the main thread
                    // Safe because only the main thread writes to the registry
                    ( void )DestroyEntitySingle( EntityID );
                    break;
            }
        }
    }

    auto Scene::DestroyEntitySingle( u64 entityID ) -> bool {
        if ( !mEntities.contains( entityID ) ) {
            return false;
        }

        RelationComponent& relationComponent{ mEntities[entityID]->GetComponent<RelationComponent>() };

        for ( const auto& childID: relationComponent.GetChildren() ) {
            ( void )DestroyEntitySingle( childID );
        }

        // Do entity cleanup
        OnRigidBodyRemoved( mRegistry, mEntities[entityID]->mHandle );
        OnColliderRemoved( mRegistry, mEntities[entityID]->mHandle );

        // Remove from ECS
        mRegistry.destroy( mEntities[entityID]->mHandle );

        // Remove from scene tracker
        const auto it{ mEntities.find( entityID ) };
        // Use iterator because we want to remove the pointer from root entities set
        // it is an entity that exists so we do not check against .end()
        if ( mRootEntities.contains( it->second.get() ) ) {
            mRootEntities.erase( it->second.get() );
        }
        mEntities.erase( it );

        return mEntities.erase( entityID ) != 0;
    }

    auto Scene::GetRegistry() -> entt::registry& {
        return mRegistry;
    }

    auto Scene::GetRegistry() const -> const entt::registry& {
        return mRegistry;
    }

    auto Scene::WorkerDestroyEntity( u64 entityID ) -> void {
        std::lock_guard lock{ mCommandQueueMutex };
        mEntityCommands.push_back( { EntityCommand::Type::eDestroy, {}, entityID } );
    }
    auto Scene::WorkerCreateEntity( const EntityCreateInfo& info ) -> void {
        std::lock_guard lock{ mCommandQueueMutex };
        mEntityCommands.push_back( { EntityCommand::Type::eCreate, info, 0 } );
    }

    auto Scene::AddSingleEntityWithRoot( Entity* root, ModelHandle model, i32 index, u64 animatorID ) -> void {
        eastl::string name{ model->GetMeshNode( index ).GetName() };
        if ( name.empty() ) {
            name = string::Format( "{} ({})", model->GetName(), index );
        }

        const EntityCreateInfo entityCreateInfo{
            .mRoot = root,
            .mName = name.c_str() };
        if ( Entity * child{ CreateEntity( entityCreateInfo ) } ) {
            if ( !model.IsEmpty() && model->IsSkinned() ) {
                child->AddComponent<SkinnedMeshRenderer>( animatorID );
            }

            SetupMeshComponent( child, model, index );
        }
    }

    #define MKT_SERIALIZE_COMPONENT_IF_PRESENT( TYPE, KEY_NAME )           \
    if ( root->HasComponent<TYPE>() ) {                            \
        emitter << YAML::Key << KEY_NAME << YAML::Value;           \
        SerializeComponent( root->GetComponent<TYPE>(), emitter ); \
    }

    static auto operator<<( YAML::Emitter& out, const float4& v ) -> YAML::Emitter& {
        out << YAML::Flow;
        out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
        return out;
    }

    static auto operator<<( YAML::Emitter& out, const float3& v ) -> YAML::Emitter& {
        out << YAML::Flow;
        out << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
        return out;
    }

    static auto operator<<(YAML::Emitter& emitter, const float4x4& matrix) -> YAML::Emitter& {
        emitter << YAML::Flow << YAML::BeginSeq;

        emitter << matrix[0];
        emitter << matrix[1];
        emitter << matrix[2];
        emitter << matrix[3];

        emitter << YAML::EndSeq;
        return emitter;
    }

    static auto SerializeComponent( const TransformComponent& transform, YAML::Emitter& emitter ) -> void {
        const auto& position{ transform.GetTranslation() };
        const auto& rotation{ transform.GetRotation() };
        const auto& scale{ transform.GetScale() };
        const auto& uniformScale{ transform.HasUniformScale() };

        emitter << YAML::BeginMap;
        emitter << YAML::Key << "Position" << YAML::Value << position;
        emitter << YAML::Key << "Rotation" << YAML::Value << rotation;
        emitter << YAML::Key << "Scale" << YAML::Value << scale;
        emitter << YAML::Key << "Uniform Scale" << YAML::Value << uniformScale;

        // Store only WorldTransform because we want to know where it was in world space
        // Local space can be recomputed from translation, rotation and scale vectors
        const float4x4 worldTransform{ transform.GetWorldTransform() };
        emitter << YAML::Key << "World Transform" << YAML::Value << worldTransform;

        emitter << YAML::EndMap;
    }

    static auto SerializeComponent( const TagComponent& tag, YAML::Emitter& emitter ) -> void {
        emitter << YAML::BeginMap;
        emitter << YAML::Key << "Name" << YAML::Value << tag.GetTag().c_str();
        emitter << YAML::Key << "Visibility" << YAML::Value << tag.IsActive();
        emitter << YAML::EndMap;
    }

    static auto SerializeComponent( const MeshComponent& meshComponent, YAML::Emitter& emitter ) -> void {
        emitter << YAML::BeginMap;

        emitter << YAML::Key << "MeshIndex" << YAML::Value << meshComponent.GetMeshIndex();
        emitter << YAML::Key << "ModelPath" << YAML::Value << meshComponent.GetModelPath().GetC_Str();

        emitter << YAML::EndMap;
    }

    static auto SerializeComponent( const MaterialComponent& materialComponent, YAML::Emitter& emitter ) -> void {
        emitter << YAML::BeginMap;

        emitter << YAML::EndMap;
    }

    static auto SerializeComponent( const LightComponent& lightComponent, YAML::Emitter& emitter ) -> void {
        emitter << YAML::BeginMap;

        emitter << YAML::EndMap;
    }

    static auto SerializeComponent( const AudioSourceComponent& audioComponent, YAML::Emitter& emitter ) -> void {
        emitter << YAML::BeginMap;

        emitter << YAML::EndMap;
    }

    static auto SerializeComponent( const RigidBodyComponent& physicsComponent, YAML::Emitter& emitter ) -> void {
        emitter << YAML::BeginMap;

        emitter << YAML::EndMap;
    }

    static auto SerializeComponent( const MeshColliderComponent& physicsComponent, YAML::Emitter& emitter ) -> void {
        emitter << YAML::BeginMap;

        emitter << YAML::EndMap;
    }

    static auto SerializeComponent( const CameraComponent& cameraComponent, YAML::Emitter& emitter ) -> void {
        emitter << YAML::BeginMap;

        emitter << YAML::EndMap;
    }

    static auto SerializeComponent( const TextComponent& textComponent, YAML::Emitter& emitter ) -> void {
        emitter << YAML::BeginMap;

        emitter << YAML::Key << "Contents" << YAML::Value << textComponent.GetContents().c_str();
        emitter << YAML::Key << "Color" << YAML::Value << textComponent.GetColor();
        emitter << YAML::Key << "IsWorldText" << YAML::Value << textComponent.IsWorldText();
        emitter << YAML::Key << "Size" << YAML::Value << textComponent.GetSize();
        emitter << YAML::Key << "Spacing" << YAML::Value << textComponent.GetSpacing();

        emitter << YAML::EndMap;
    }

    static auto SerializeComponent( const ScriptComponent& scriptComponent, YAML::Emitter& emitter ) -> void {
        emitter << YAML::BeginMap;

        emitter << YAML::EndMap;
    }

    static auto SerializeNode( YAML::Emitter& emitter, const Entity* root, const Scene& scene ) -> void {
        if ( root == nullptr ) {
            return;
        }

        emitter << YAML::BeginMap;
        emitter << YAML::Key << "Game Object";

        // Tag and Transform components are always present
        emitter << YAML::Key << "TagComponent" << YAML::Value;
        SerializeComponent( root->GetComponent<TagComponent>(), emitter );

        emitter << YAML::Key << "TransformComponent" << YAML::Value;
        SerializeComponent( root->GetComponent<TransformComponent>(), emitter );

        MKT_SERIALIZE_COMPONENT_IF_PRESENT( MeshComponent, "MeshComponent" );
        MKT_SERIALIZE_COMPONENT_IF_PRESENT( MaterialComponent, "MaterialComponent" );
        MKT_SERIALIZE_COMPONENT_IF_PRESENT( LightComponent, "LightComponent" );
        MKT_SERIALIZE_COMPONENT_IF_PRESENT( AudioSourceComponent, "AudioSourceComponent" );
        MKT_SERIALIZE_COMPONENT_IF_PRESENT( CameraComponent, "CameraComponent" );
        MKT_SERIALIZE_COMPONENT_IF_PRESENT( TextComponent, "TextComponent" );
        MKT_SERIALIZE_COMPONENT_IF_PRESENT( ScriptComponent, "ScriptComponent" );

        // I do not serialize the relationship component the hierarchy is stored explicitly by the nesting of the nodes in the YAML file.
        // So if an entity has children they will be nested under it in the YAML file and if it does not have children it will just be a leaf node.
        // Entity Unique ID is a runtime property that is established when it is created.
        for ( const auto& childID: root->GetComponent<RelationComponent>().GetChildren() ) {
            SerializeNode( emitter, scene.FindByID( childID ), scene );
        }

        emitter << YAML::EndMap;
    }

    auto Scene::Serialize( FileHandle file ) const -> void {
        YAML::Emitter emitter{};

        emitter << YAML::BeginMap;
        emitter << YAML::Key << "Scene Properties" << YAML::Value << YAML::BeginSeq;

        emitter << YAML::Key << "Scene" << YAML::Value << GetName().data();
        emitter << YAML::Key << "Objects" << YAML::Value << YAML::BeginSeq;

        for ( const auto& root: mRootEntities ) {
            SerializeNode( emitter, root, *this );
        }
        emitter << YAML::EndSeq;
        emitter << YAML::EndMap;

        file->SetContents( emitter.c_str() );
        file->FlushContents();
    }

    auto Scene::Deserialize( filesystem::FileHandle file ) const -> void {
        YAML::Node data{ YAML::Load( file->GetContentsString().c_str() ) };

        if (data.IsNull()) {
            auto message{ string::Format("File opened '{}' but contains no data for deserialization", file->GetPath().GetC_Str()) };
            MKT_CORE_LOGGER_WARN( "{}", message );

            return;
        }

        if (data["Scene"].IsNull()) {
            auto message{ string::Format("File opened [{}] but contains Scene Node", file->GetPath().GetC_Str()) };

            MKT_CORE_LOGGER_WARN( "{}", message );
            return;
        }

        // Recreate a new scene on top of which we are going to deserialize
        const std::string sceneName{ data["Scene"].as<std::string>() };
        const auto sceneEntities{ data["Objects"] };
        if ( !sceneEntities.IsNull() ) {

#if false
            EntityCreateInfo entityCreateInfo{};

            for (const auto& object : sceneEntities) {
                Entity entity{};

                // Get the entity ID
                const std::string uuid{ object["Object"].as<std::string>() };
                const std::string name{ object["TagComponent"]["Name"].as<std::string>() };

                // Get Render component
                if (!object["RenderComponent"].IsNull()) {
                    const bool isPrefab{ object["RenderComponent"]["IsPrefab"].as<bool>() };

                    entityCreateInfo.mName = name;
                    entityCreateInfo.PrefabType = PrefabSceneObject::NO_PREFAB_OBJECT;

                    if (isPrefab) {
                        // Get the type of prefab if it was one
                        entityCreateInfo.PrefabType = PrefabTypeFromName(object["RenderComponent"]["PrefabType"].as<std::string>());
                    }

                    //SceneManager::AddEntityToScene(newScene, entityCreateInfo);
                }

                // Get Material component
                if (!object["MaterialComponent"].IsNull()) {
                    const auto color{ object["MaterialComponent"]["Color"].as<glm::vec4>() };
                }

                // Get the Tag component
                TagComponent tagComponent{};
                tagComponent.SetTag(name);
                tagComponent.SetVisibility(object["TagComponent"]["Visibility"].as<bool>());
                MKT_CORE_LOGGER_INFO("Found entity with name {}", tagComponent.GetTag());

                // Get Transform component
                TransformComponent transformComponent{};
                const auto position{ object["TransformComponent"]["Position"].as<glm::vec3>() };
                const auto rotation{ object["TransformComponent"]["Rotation"].as<glm::vec3>() };
                const auto scale{ object["TransformComponent"]["Scale"].as<glm::vec3>() };
                transformComponent.ComputeTransform(position, scale, rotation);

                entity.GetComponent<TransformComponent>() = transformComponent;
                entity.GetComponent<TagComponent>() = tagComponent;
            }
        }
        else {
            MKT_CORE_LOGGER_INFO("File opened '{}' but has no scene objects", saveFilePath.string());
        }
#endif
        }

    }

    auto EntityCreateInfo::SetName( eastl::string_view name ) -> EntityCreateInfo& {
        this->mName = name;
        return *this;
    }

    auto EntityCreateInfo::SetRoot( Entity* root ) -> EntityCreateInfo& {
        this->mRoot = root;
        return *this;
    }

    auto EntityCreateInfo::SetModel( ModelHandle modelMesh ) -> EntityCreateInfo& {
        this->mModel = modelMesh;
        return *this;
    }
}// namespace Mikoto