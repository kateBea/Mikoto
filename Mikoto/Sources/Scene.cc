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

#include <flecs.h>
#include <entt/entt.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Profiler.hh>
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

namespace mikoto::scene {

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

    static auto OnMeshRendererAdded( entt::registry& reg, entt::entity e ) -> void {
        if ( !reg.any_of<MaterialComponent>( e ) ) {
            reg.emplace<MaterialComponent>( e );
        }

        auto& meshComponent{ reg.get<MeshComponent>( e ) };
        auto meshNode{ meshComponent.GetMesh() };

        auto& material{ reg.get<MaterialComponent>( e ) };

        if (meshNode) {
            material.SetMaterial( AssetsService::Get()->CreateMaterial( meshNode->GetProperties() ) );
        } else {
            material.SetMaterial( AssetsService::Get()->CreateMaterial() );
        }
    }

    static auto OnSkinnedMeshRendererAdded( entt::registry& reg, entt::entity e ) -> void {

    }

    static auto OnAudioListenerAdded( entt::registry& reg, entt::entity e ) -> void {
        static bool listenerActive{ false };

        auto& listenerComponent{ reg.get<AudioListenerComponent>( e ) };
        listenerComponent.SetListener(AudioService::Get()->CreateListener());

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

        // Animation
        mRegistry.on_construct<AnimatorComponent>().connect<&OnAnimatorAdded>();
        mRegistry.on_construct<SkinnedMeshRenderer>().connect<&OnSkinnedMeshRendererAdded>();

        // Audio
        mRegistry.on_construct<AudioListenerComponent>().connect<&OnAudioListenerAdded>();

        // Script
        mRegistry.on_construct<ScriptComponent>().connect<&Scene::OnScriptAdded>(this);

        // Physics
        mRegistry.on_construct<RigidBodyComponent>().connect<&Scene::OnRigidBodyAdded>(this);
        mRegistry.on_construct<ColliderComponent>().connect<&Scene::OnColliderAdded>(this);
        mRegistry.on_destroy<RigidBodyComponent>().connect<&Scene::OnRigidBodyRemoved>(this);
        mRegistry.on_destroy<ColliderComponent>().connect<&Scene::OnColliderRemoved>(this);

        physics::PhysicsWorldCreateInfo spec{
            .mScene = this,
            .mGravity = physics::PhysicsWorld::GetGravityFor( physics::GravityBody::eEarth )
        };
        mPhysicsWorld = physics::PhysicSystem::Get()->CreatePhysicsWorld( spec );
    }

     auto Scene::UpdateIdle( double ) -> void {
         // This is only done on simulate but here too for debugging purposes
         physics::PhysicSystem::Get()->SetSimulationTarget( this );

         // Mark scripts as updatable so the scripting system updates their states
         auto scriptEntities{ mRegistry.view<ScriptComponent>() };
         for (auto& entity : scriptEntities) {
             ScriptComponent& script{ mRegistry.get<ScriptComponent>(entity) };
             ScriptHandle handle{ script.GetHandle() };

             if (!handle.IsEmpty()) {
                 // Should be false here. But will remain true for testing purposes
                 handle->SetEnable( true );
             }
         }
     }

     auto Scene::UpdateSimulate( double ) -> void {
         physics::PhysicSystem::Get()->SetSimulationTarget( this );

         // Update scripts
         auto scriptEntities{ mRegistry.view<ScriptComponent>() };
         for (auto& entity : scriptEntities) {
             ScriptComponent& script{ mRegistry.get<ScriptComponent>(entity) };
             ScriptHandle handle{ script.GetHandle() };

             if (!handle.IsEmpty()) {
                 handle->SetEnable( true );
             }
         }
     }

     auto Scene::OnRigidBodyAdded(entt::registry& reg, entt::entity e ) const -> void {
         // Add the component if it does not exist
         if (!reg.any_of<TransformComponent>( e ) ) {
             reg.emplace_or_replace<RigidBodyComponent>(e);
         }

         RigidBodyComponent& rigidBody{ reg.get<RigidBodyComponent>(e) };
         TransformComponent& transform{ reg.get<TransformComponent>(e) };
     }

     auto Scene::OnColliderAdded( entt::registry& reg, entt::entity e ) const -> void {
         // It is possible to have a collider and no rigid body component,
         // this is because a collisions do not only happen with rigid bodies
         // collision areas can be used to trigger effects upon entering certain areas
     }

     auto Scene::OnRigidBodyRemoved( entt::registry& reg, entt::entity e ) const -> void {
         // Add the component if it does not exist
         if (!reg.any_of<TransformComponent>( e ) ) {
             reg.emplace_or_replace<RigidBodyComponent>(e);
         }

         RigidBodyComponent& rb{ reg.get<RigidBodyComponent>(e) };
     }

     auto Scene::OnColliderRemoved( entt::registry& reg, entt::entity e ) const -> void {
         // This does not remove the rigid body component. The idea is that
         // if there is no collider nobody will not respond to any type of collisions
         // Could use a custom collision (empty) in Jolt's side maybe? WIP
         // An alternative is Empty shape
         // https://jrouwe.github.io/JoltPhysics/class_empty_shape.html

         // More complex shapes can be created with vertices
     }

     auto Scene::OnScriptAdded( entt::registry& reg, entt::entity e ) -> void {
         TagComponent& tag{ reg.get<TagComponent>(e) };
         ScriptComponent& scriptComponent{ reg.get<ScriptComponent>(e) };

         ScriptHandle scriptHandle{};
         if (Entity* entity{ FindByID( tag.GetGUID() ) }) {

         }
     }

     auto Scene::SetupMeshComponent( Entity *entity, ModelHandle model, i32 index ) -> void {
         MKT_ASSERT( index > -1, "Index must be a positive integer" );
         MKT_ASSERT( entity != nullptr, "Entity cannot bet null" );
         MKT_ASSERT( !model.IsEmpty(), "Model cannot be empty" );

         entity->AddComponent<MeshComponent>( model, index );

         MeshNode& meshNode{ model->GetMeshNode( index ) };

         // Meshes have their own transformation from whatever modeling tool they were used
         // We need to apply it to the mesh local transform in order to properly place it in our world
         TransformComponent& transform{ entity->GetComponent<TransformComponent>() };
         transform.SetTransform( meshNode.GetTransform() );

         if (!entity->HasComponent<MaterialComponent>()) {
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

         for ( Entity* rootEntity : mRootEntities ) {
             UpdateWorldTransform( *rootEntity, Identity, this );
         }
     }

     auto Scene::UpdateAudioListenerAndSources() -> void {
         // Update listeners
         auto viewListeners{ mRegistry.view<TransformComponent, AudioListenerComponent>() };
         for ( const auto& entity: viewListeners ) {
             TransformComponent& transformComponent{ mRegistry.get<TransformComponent>( entity ) };
             AudioListenerComponent& audioListenerComponent{ mRegistry.get<AudioListenerComponent>( entity ) };

             if (audioListenerComponent.IsActive()) {
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
             if (!source.IsEmpty()) {
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
        }

        UpdateWorldTransformations();
        UpdateAudioListenerAndSources();
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

     auto Scene::CreateEntityDefault(const EntityCreateInfo& info ) -> Entity* {
         const auto entity{ new Entity{ mRegistry } };

         // [Constants for default entity parameters]
         constexpr float3 initialSize{ 1.0f, 1.0f, 1.0f };
         constexpr float3 initialPosition{ 0.0, 0.0, 0.0 };
         constexpr float3 initialRotation{ 0.0f, 0.0f, 0.0f };

         if ( info.mRoot != nullptr ) {
             const TagComponent& parentTag{ info.mRoot->GetComponent<TagComponent>() };
             entity->AddComponent<RelationComponent>( std::make_optional( parentTag.GetGUID() ) );
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

     auto Scene::CreateEntitySingle( const EntityCreateInfo& createInfo ) -> Entity* {
         MKT_BEGIN_PROFILER_NAMED();

         Entity* result{ CreateEntityDefault( createInfo) };

         const u64 guid{ result->GetComponent<TagComponent>().GetGUID() };

         const auto [it, success]{
             mEntities.try_emplace( guid, result )
         };

         if ( success ) {
             result = it->second.get();

             if ( createInfo.mIsLight ) {
                 result->AddComponent<LightComponent>( createInfo.mLightType );
             }

             if ( createInfo.mIsText ) {
                 result->AddComponent<TextComponent>(
                     createInfo.mInitialContents,
                     createInfo.mTextSize,
                     createInfo.mTextSpacing,
                     createInfo.mIsWorldText);
             }

             // if root is not empty this entity must be registered as child of root entity
             if ( createInfo.mRoot != nullptr ) {
                 Entity* parent{ createInfo.mRoot };
                 RelationComponent& parentRelation{ parent->GetComponent<RelationComponent>() };

                 parentRelation.RegisterChild( guid );
             }

             // in root model is not empty, we create the children for this entity each children well hold a mesh
             if ( !createInfo.mModel.IsEmpty() ) {
                 if ( createInfo.mModel->GetMeshNodeCount() > 1 ) {
                     u64 animatorID{};

                     if (createInfo.mModel->IsSkinned()) {
                         animatorID = AnimationSystem::Get()->RegisterAnimation( createInfo.mModel );
                         result->AddComponent<AnimatorComponent>( animatorID );
                     }

                     for ( size_t index{}; index < createInfo.mModel->GetMeshNodeCount(); index++ ) {
                         AddSingleEntityWithRoot( result, createInfo.mModel, index, animatorID );
                     }

                 } else {
                     if (createInfo.mModel->IsSkinned()) {
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

     auto Scene::CreateEntity( Entity *root, eastl::string_view name ) -> Entity * {
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
         for ( const auto &entity: mRegistry.view<CameraComponent>() ) {
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
         if (!mEntities.contains( entityID )) {
             return false;
         }

         RelationComponent& relationComponent{ mEntities[entityID]->GetComponent<RelationComponent>() };

         for (const auto& childID : relationComponent.GetChildren()) {
             (void)DestroyEntitySingle(childID);
         }

         mRegistry.destroy( mEntities[entityID]->m_Handle );

         const auto it{ mEntities.find( entityID ) };
         // Use iterator because we want to remove the pointer from root entities set
         // it is an entity that exists so we do not check against .end()
         if (mRootEntities.contains( it->second.get() )) {
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
             .mName = name.c_str(),
         };

         if ( Entity* child{ CreateEntity( entityCreateInfo ) } ) {
             if (!model.IsEmpty() && model->IsSkinned()) {
                 child->AddComponent<SkinnedMeshRenderer>( animatorID );
             }

             SetupMeshComponent(child, model, index);
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