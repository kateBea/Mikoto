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

#include <memory>
#include <utility>
#include <optional>
#include <algorithm>
#include <string_view>

#include <entt/entt.hpp>

#include <Core/Profiler.hh>
#include <Core/RuntimeConsole.hh>

#include <Audio/AudioService.hh>

#include <Library/Random/Random.hh>
#include <Physics/PhysicService.hh>

#include <Animation/AnimationSystem.hh>
#include <Scripting/ScriptingService.hh>
#include <Renderer/Core/RenderService.hh>

#include <Scene/Scene.hh>
#include <Scene/Component.hh>

namespace Mikoto {

    static auto UpdateWorldTransform( Entity& e, Mat4F parentWorld, Scene* scene ) -> void {
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
    
    Scene::Scene( const std::string_view name )
        : m_Name{ name } {

        m_Registry.on_construct<MeshComponent>().connect<&OnMeshRendererAdded>();
        m_Registry.on_construct<AnimatorComponent>().connect<&OnAnimatorAdded>();
        m_Registry.on_construct<SkinnedMeshRenderer>().connect<&OnSkinnedMeshRendererAdded>();

        m_Registry.on_construct<AudioListenerComponent>().connect<&OnAudioListenerAdded>();

        m_Registry.on_construct<ScriptComponent>().connect<&Scene::OnScriptAdded>(this);

        m_Registry.on_construct<RigidBodyComponent>().connect<&Scene::OnRigidBodyAdded>(this);

        m_Registry.on_destroy<RigidBodyComponent>().connect<&Scene::OnRigidBodyRemoved>(this);

        PhysicsWorldCreateInfo spec{
            .TargetScene{ this },
            .Gravity{ PhysicsWorld::GetGravityFor( GravityBody::EARTH ) }
        };
        m_PhysicsWorld = PhysicService::Get()->CreatePhysicsWorld( spec );
    }

    auto Scene::UpdateIdle( double ) -> void {
        // This is only done on simulate but here too for debugging purposes
        PhysicService::Get()->SetSimulationTarget( this );

        // Update scripts
        auto scriptEntities{ m_Registry.view<ScriptComponent>() };
        for (auto& entity : scriptEntities) {
            ScriptComponent& script{ m_Registry.get<ScriptComponent>(entity) };
            ScriptHandle handle{ script.GetHandle() };

            if (!handle.IsEmpty()) {
                // Should be false here. But will remain tru for testing purposes
                handle->SetEnable( true );
            }
        }
    }

    auto Scene::UpdateSimulate( double ) -> void {
        PhysicService::Get()->SetSimulationTarget( this );

        // Update scripts
        auto scriptEntities{ m_Registry.view<ScriptComponent>() };
        for (auto& entity : scriptEntities) {
            ScriptComponent& script{ m_Registry.get<ScriptComponent>(entity) };
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

        m_PhysicsWorld->OnRigidBodyAdded( transform, rigidBody );
    }

    auto Scene::OnScriptAdded( entt::registry& reg, entt::entity e ) -> void {
        TagComponent& tag{ reg.get<TagComponent>(e) };
        ScriptComponent& scriptComponent{ reg.get<ScriptComponent>(e) };

        ScriptHandle scriptHandle{};
        if (Entity* entity{ FindByID( tag.GetGUID() ) }) {
            if (!scriptComponent.GetFilePath().empty()) {
                // The component wants to load a specific file
                scriptHandle = ScriptingService::Get()->LoadScript( scriptComponent.GetFilePath(), entity );
            } else {
                // start from default script
                scriptHandle = ScriptingService::Get()->CreateScript( entity );
            }

            if (!scriptHandle.IsEmpty()) {
                scriptComponent.SetScript( scriptHandle );
            } else {
                MKT_CORE_LOGGER_WARN( "Scene::OnScriptAdded - Failed to add script." );
            }
        }
    }

    auto Scene::OnRigidBodyRemoved( entt::registry& reg, entt::entity e ) const -> void {
        // Add the component if it does not exist
        if (!reg.any_of<TransformComponent>( e ) ) {
            reg.emplace_or_replace<RigidBodyComponent>(e);
        }

        RigidBodyComponent& rigidBody{ reg.get<RigidBodyComponent>(e) };
        m_PhysicsWorld->OnRigidBodyRemoved( rigidBody );
    }

    auto Scene::SetupMeshComponent( Entity *entity, ModelHandle model, Int32 index ) -> void {
        MKT_ASSERT( index > -1, "Index must be a positive integer" );
        MKT_ASSERT( entity != nullptr, "Entity cannot bet null" );
        MKT_ASSERT( !model.IsEmpty(), "Model cannot be empty" );

        entity->AddComponent<MeshComponent>( model, index );

        MeshNode& meshNode{ model->GetMeshNode( index ) };

        if (!entity->HasComponent<MaterialComponent>()) {
            entity->AddComponent<MaterialComponent>( AssetsService::Get()->CreateMaterial( meshNode.GetProperties() ) );
        }
    }

    auto Scene::RemoveEntity( UInt64 uniqueID ) -> void {
        m_EntityCommands.emplace_back( EntityCommand{
                .Type{ EntityCommand::Type::DESTROY },
                .EntityID{ uniqueID },
        } );
    }

    auto Scene::SetState( const SceneState state ) -> void {
        m_SceneState = state;
    }

    auto Scene::UpdateWorldTransformations() -> void {
        Mat4F Identity{ 1.0f };

        for ( Entity* rootEntity : m_RootEntities ) {
            UpdateWorldTransform( *rootEntity, Identity, this );
        }
    }

    auto Scene::UpdateAudioListenerAndSources() -> void {
        // Update listeners
        auto viewListeners{ m_Registry.view<TransformComponent, AudioListenerComponent>() };
        for ( const auto& entity: viewListeners ) {
            TransformComponent& transformComponent{ m_Registry.get<TransformComponent>( entity ) };
            AudioListenerComponent& audioListenerComponent{ m_Registry.get<AudioListenerComponent>( entity ) };

            if (audioListenerComponent.IsActive()) {
                audioListenerComponent.GetListener().SetPosition( transformComponent.GetTranslation() );
                audioListenerComponent.GetListener().Apply();
            }
        }

        // Update sources
        auto viewSources{ m_Registry.view<TransformComponent, AudioSourceComponent>() };
        for ( const auto& entity: viewSources ) {
            TransformComponent& transformComponent{ m_Registry.get<TransformComponent>( entity ) };
            AudioSourceComponent& audioSourceComponent{ m_Registry.get<AudioSourceComponent>( entity ) };

            AudioSourceHandle source{ audioSourceComponent.GetSource() };
            if (!source.IsEmpty()) {
                source->SetPosition( transformComponent.GetTranslation() );
            }
        }
    }

    auto Scene::Update( const float timeStep ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        ProcessPendingCommands();

        switch ( m_SceneState ) {

            case SceneState::IDLE:
                UpdateIdle( timeStep );
                break;
            case SceneState::SIMULATING:
                UpdateSimulate( timeStep );
                break;
        }

        // We must ensure this transformations comes after any code
        // that changes the local transform of any entity, otherwise the world transform will be wrong

        // Important note that this assumes there is no entity that is children from more than one parent
        UpdateWorldTransformations();

        UpdateAudioListenerAndSources();

#if !defined(NDEBUG)
        ComputeStats();
#endif
    }

    auto Scene::SetName( const std::string_view name ) -> void {
        m_Name = name;
    }

    auto Scene::FindByID( const UInt64 uniqueID ) -> Entity* {
        const auto it{ m_Entities.find( uniqueID ) };

        return it != m_Entities.end() ? it->second.get() : nullptr;
    }

    auto Scene::FindFirstByName( const std::string_view name ) -> Entity* {
        const auto it{ std::ranges::find_if( m_Entities, [&]( auto& pair ) -> bool {
            const auto& entity{ pair.second };

            // All entities have tag component you can only create
            // them from the CreateEntity method
            const auto& tag{ entity->template GetComponent<TagComponent>() };
            return tag.GetTag() == name;
        } ) };

        return it != m_Entities.end() ? it->second.get() : nullptr;
    }

    auto Scene::FindByID( const UInt64 uniqueID ) const -> const Entity* {
        const auto it{ m_Entities.find( uniqueID ) };

        return it != m_Entities.end() ? it->second.get() : nullptr;
    }

    auto Scene::FindFirstByName( const std::string_view name ) const -> const Entity* {
        const auto it{ std::ranges::find_if( m_Entities, [&]( auto& pair ) -> bool {
            const auto& entity{ pair.second };

            // All entities have tag component you can only create
            // them from the CreateEntity method
            const auto& tag{ entity->template GetComponent<TagComponent>() };
            return tag.GetTag() == name;
        } ) };

        return it != m_Entities.end() ? it->second.get() : nullptr;
    }

    auto Scene::ExistsByID( const UInt64 uniqueID ) -> bool {
        return FindByID( uniqueID ) != nullptr;
    }

    auto Scene::ExistsByName( const std::string_view name ) -> bool {
        return FindFirstByName( name ) != nullptr;
    }

    auto Scene::Clear() -> void {
        m_Entities.clear();

        m_Registry.clear();
    }

    auto Scene::CreateEntityDefault(const EntityCreateInfo& info ) -> Entity* {
        const auto entity{ new Entity{ m_Registry } };

        // [Constants for default entity parameters]
        constexpr Vec3F initialSize{ 1.0f, 1.0f, 1.0f };
        constexpr Vec3F initialPosition{ 0.0, 0.0, 0.0 };
        constexpr Vec3F initialRotation{ 0.0f, 0.0f, 0.0f };

        if ( info.Root != nullptr ) {
            const TagComponent& parentTag{ info.Root->GetComponent<TagComponent>() };
            entity->AddComponent<RelationComponent>( std::make_optional( parentTag.GetGUID() ) );
        } else {
            entity->AddComponent<RelationComponent>();
        }

        entity->AddComponent<HighlightComponent>();
        entity->AddComponent<TagComponent>( info.Name );
        entity->AddComponent<TransformComponent>( initialPosition, initialSize, initialRotation );

        if ( info.Root == nullptr ) {
            m_RootEntities.emplace( entity );
        }

        return entity;
    }

    auto Scene::CreateEntitySingle( const EntityCreateInfo& createInfo ) -> Entity* {
        MKT_BEGIN_PROFILER_NAMED();

        Entity* result{ CreateEntityDefault( createInfo) };

        const UInt64 guid{ result->GetComponent<TagComponent>().GetGUID() };

        const auto [it, success]{
            m_Entities.try_emplace( guid, result )
        };

        if ( success ) {
            result = it->second.get();

            if ( createInfo.IsLight ) {
                result->AddComponent<LightComponent>( createInfo.TypeLight );
            }

            if ( createInfo.IsText ) {
                result->AddComponent<TextComponent>( 
                    createInfo.InitialContents, 
                    createInfo.TextSize, 
                    createInfo.TextSpacing, 
                    createInfo.IsWorldText);
            }

            // if root is not empty this entity must be registered as child of root entity
            if ( createInfo.Root != nullptr ) {
                Entity* parent{ createInfo.Root };
                RelationComponent& parentRelation{ parent->GetComponent<RelationComponent>() };

                parentRelation.RegisterChild( guid );
            }

            // in root model is not empty, we create the children for this entity each children well hold a mesh
            if ( !createInfo.Model.IsEmpty() ) {
                if ( createInfo.Model->GetMeshNodeCount() > 1 ) {
                    UInt64 animatorID{};

                    if (createInfo.Model->IsSkinned()) {
                        animatorID = AnimationSystem::Get()->RegisterAnimation( createInfo.Model );
                        result->AddComponent<AnimatorComponent>( animatorID );
                    }

                    for ( Size index{}; index < createInfo.Model->GetMeshNodeCount(); index++ ) {
                        AddSingleEntityWithRoot( result, createInfo.Model, index, animatorID );
                    }

                } else {
                    if (createInfo.Model->IsSkinned()) {
                        UInt64 animatorID{ AnimationSystem::Get()->RegisterAnimation( createInfo.Model ) };
                        result->AddComponent<AnimatorComponent>( animatorID );
                        result->AddComponent<SkinnedMeshRenderer>( animatorID );
                    }

                    SetupMeshComponent( result, createInfo.Model, 0 );
                }
            }
        }

        return result;
    }
    
    auto Scene::CreateEntity( const EntityCreateInfo& createInfo ) -> Entity* {
        return CreateEntitySingle( createInfo );
    }

    auto Scene::GetPhysicsWorld() -> PhysicsWorld* {
        return m_PhysicsWorld;
    }

    auto Scene::GetRootEntities() const -> const ankerl::unordered_dense::set<Entity*>& {
        return m_RootEntities;
    }

    auto Scene::SetSceneBackground( SceneBackground background ) -> void {
        m_Background = background;
    }

    auto Scene::GetSceneBackground() const -> SceneBackground {
        return m_Background;
    }

    auto Scene::IsSceneBackground(SceneBackground background) const -> bool {
        return m_Background == background;
    }

    auto Scene::SetSkybox( TextureHandle cubeMap ) -> void {
        m_Skybox = cubeMap;
    }

    auto Scene::GetSkybox() -> TextureHandle {
        return m_Skybox;
    }

    auto Scene::QueueCreateEntity( std::string_view name ) -> void {
        const EntityCreateInfo info{
            .Root{ nullptr },
            .Name{ name.data() },
            .Model{ ModelHandle::CreateEmpty() }
        };

        QueueCreateEntity( info );
    }

    auto Scene::QueueCreateEntity( const EntityCreateInfo& createInfo ) -> void {
        // Lock and push the creation command
        std::lock_guard lock( m_CommandQueueMutex );
        m_EntityCommands.push_back( { EntityCommand::Type::CREATE, createInfo } );
    }

    auto Scene::GetLightCount() const -> UInt32 {
        return m_TotalLightCount;
    }

    auto Scene::GetActiveLightCount() const -> UInt32 {
        return m_ActiveLightCount;
    }

    auto Scene::CreateEntity( std::string_view name ) -> Entity* {
        const EntityCreateInfo info{
            .Root{ nullptr },
            .Name{ name.data() },
            .Model{ ModelHandle::CreateEmpty() }
        };

        return CreateEntity( info );
    }

    auto Scene::CreateEntity( Entity *root, std::string_view name ) -> Entity * {
        const EntityCreateInfo info{
            .Root{ root },
            .Name{ name.data() },
            .Model{ ModelHandle::CreateEmpty() }
        };

        return CreateEntity( info );
    }

    auto Scene::GetEntityCount() const -> Size {
        return m_Entities.size();
    }

    auto Scene::OnViewPortResize( const float width, const float height ) -> void {
        // Resize non-fixed aspect ratio cameras
        for ( const auto &entity: m_Registry.view<CameraComponent>() ) {
            CameraComponent& cameraComponent{ m_Registry.get<CameraComponent>( entity ) };

            if ( !cameraComponent.IsAspectRatioFixed() ) {
                cameraComponent.GetCamera().SetViewportSize( width, height );
            }
        }
    }

    Scene::~Scene() {
        Clear();
    }

    auto Scene::ProcessPendingCommands() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Temporary vector to hold commands to process this frame
        std::vector<EntityCommand> commandsCopy{};

        {
            // Lock the command queue so no other thread modifies it while we swap
            std::lock_guard lock( m_CommandQueueMutex );

            // Swap the current queue with our temporary vector
            // This effectively takes ownership of all pending commands
            // and clears the original queue so other threads can keep pushing, swap is O(1)
            commandsCopy.swap( m_EntityCommands );
        }

        // Now we can process the commands without holding the lock
        // This avoids blocking other threads that want to enqueue new commands
        for ( auto& [Type, CreateInfo, EntityID]: commandsCopy ) {
            switch ( Type ) {
                case EntityCommand::Type::CREATE:
                    // Create the entity immediately on the main thread
                    // Safe because only the main thread writes to the registry
                    ( void )CreateEntitySingle( CreateInfo );
                    break;

                case EntityCommand::Type::DESTROY:
                    // Destroy the entity immediately on the main thread
                    // Safe because only the main thread writes to the registry
                    ( void )DestroyEntitySingle( EntityID );
                    break;
            }
        }
    }

    auto Scene::DestroyEntitySingle( UInt64 entityID ) -> bool {
        if (!m_Entities.contains( entityID )) {
            return false;
        }

        RelationComponent& relationComponent{ m_Entities[entityID]->GetComponent<RelationComponent>() };

        for (const auto& childID : relationComponent.GetChildren()) {
            (void)DestroyEntitySingle(childID);
        }

        m_Registry.destroy( m_Entities[entityID]->m_Handle );

        const auto it{ m_Entities.find( entityID ) };
        // Use iterator because we want to remove the pointer from root entities set
        // it is an entity that exists so we do not check against .end()
        if (m_RootEntities.contains( it->second.get() )) {
            m_RootEntities.erase( it->second.get() );
        }
        m_Entities.erase( it );

        return m_Entities.erase( entityID ) != 0;
    }

    auto Scene::GetRegistry() -> entt::registry& {
        return m_Registry;
    }

    auto Scene::GetRegistry() const -> const entt::registry& {
        return m_Registry;
    }

    auto Scene::WorkerDestroyEntity( UInt64 entityID ) -> void {
        std::lock_guard lock{ m_CommandQueueMutex };
        m_EntityCommands.push_back( { EntityCommand::Type::DESTROY, {}, entityID } );
    }
    auto Scene::WorkerCreateEntity( const EntityCreateInfo& info ) -> void {
        std::lock_guard lock{ m_CommandQueueMutex };
        m_EntityCommands.push_back( { EntityCommand::Type::CREATE, info, 0 } );
    }

    auto Scene::ComputeStats() -> void {
        m_TotalLightCount  = 0;
        m_ActiveLightCount = 0;

        // View entities that have a LightComponent
        auto view{ m_Registry.view<LightComponent>() };

        m_TotalLightCount = static_cast<UInt32>(view.size());

        for (const auto& entity : view) {
            // All entities from Scene are constructed with a Tag
            const auto& tag{ m_Registry.get<TagComponent>(entity) };
            if (!tag.IsActive())
                continue;

            ++m_ActiveLightCount;
        }
    }

    auto Scene::AddSingleEntityWithRoot( Entity* root, ModelHandle model, Int32 index, UInt64 animatorID ) -> void {
        std::string name{ model->GetMeshNode( index ).GetName() };
        if ( name.empty() ) {
            name = fmt::format( "{} ({})", model->GetName(), index );
        }

        const EntityCreateInfo entityCreateInfo{
            .Root{ root },
            .Name{ name.c_str() },
        };

        if ( Entity* child{ CreateEntity( entityCreateInfo ) } ) {
            if (!model.IsEmpty() && model->IsSkinned()) {
                child->AddComponent<SkinnedMeshRenderer>( animatorID );
            }

            SetupMeshComponent(child, model, index);
        }
    }

    auto EntityCreateInfo::WithName( std::string_view name ) -> EntityCreateInfo& {
        this->Name = name;
        return *this;
    }

    auto EntityCreateInfo::WithRoot( Entity* root ) -> EntityCreateInfo& {
        this->Root = root;
        return *this;
    }

    auto EntityCreateInfo::WithModelMesh( ModelHandle modelMesh ) -> EntityCreateInfo& {
        this->Model = modelMesh;
        return *this;
    }
}// namespace Mikoto