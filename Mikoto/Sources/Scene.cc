/**
 * Scene.cc
 * Created by kate on 6/24/23.
 * */

// C++ Standard Library
#include <algorithm>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

// Third-Party Libraries
#include <entt/entt.hpp>

// Project Headers
#include <Renderer/Core/RenderService.hh>
#include <Renderer/Core/RendererBackend.hh>
#include <Core/Profiler.hh>
#include <Library/Random/Random.hh>
#include <Physics/PhysicService.hh>
#include <Scene/Component.hh>
#include <Scene/Scene.hh>

namespace Mikoto {

    static auto SetupStandardComponents( Entity& entity, const EntityCreateInfo& info ) -> void {
        // [Constants for default entity parameters]
        constexpr Vec3F initialSize{ 1.0f, 1.0f, 1.0f };
        constexpr Vec3F initialPosition{ 0.0, 0.0, 0.0 };
        constexpr Vec3F initialRotation{ 0.0f, 0.0f, 0.0f };

        if ( info.Root != nullptr ) {
            TagComponent& parentTag{ info.Root->GetComponent<TagComponent>() };
            entity.AddComponent<RelationComponent>( std::make_optional( parentTag.GetGUID() ) );
        } else {
            entity.AddComponent<RelationComponent>();
        }

        entity.AddComponent<TagComponent>( info.Name );
        entity.AddComponent<TransformComponent>( initialPosition, initialSize, initialRotation );
    }

    static auto OnMeshRendererAdded( entt::registry& reg, entt::entity e ) -> void {
        if ( !reg.any_of<MaterialComponent>( e ) ) {
            reg.emplace<MaterialComponent>( e );
        }

        // Construct default material
        auto& material{ reg.get<MaterialComponent>( e ) };
        material.SetMaterial( AssetsService::Get()->CreateMaterial() );
    }

    Scene::Scene( const std::string_view name )
        : m_Name{ name } {

        m_Registry.on_construct<MeshComponent>().connect<&OnMeshRendererAdded>();
        m_Registry.on_construct<RigidBodyComponent>().connect<&Scene::OnRigidBodyAdded>(this);
        m_Registry.on_destroy<RigidBodyComponent>().connect<&Scene::OnRigidBodyRemoved>(this);

        PhysicsWorldCreateInfo spec{
            .TargetScene{ this },
            .Gravity{ PhysicsWorld::GetEarthGravity() }
        };
        m_PhysicsWorld = PhysicService::Get()->CreatePhysicsWorld( spec );
    }

    auto Scene::UpdateIdle( double ) -> void {
        // This is only done on simulate but here too for debugging purposes
        PhysicService::Get()->SetSimulationTarget( this );
    }

    auto Scene::UpdateSimulate( double ) -> void {
        PhysicService::Get()->SetSimulationTarget( this );
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

    auto Scene::OnRigidBodyRemoved( entt::registry& reg, entt::entity e ) const -> void {
        // Add the component if it does not exist
        if (!reg.any_of<TransformComponent>( e ) ) {
            reg.emplace_or_replace<RigidBodyComponent>(e);
        }

        RigidBodyComponent& rigidBody{ reg.get<RigidBodyComponent>(e) };
        m_PhysicsWorld->OnRigidBodyRemoved( rigidBody );
    }

    auto Scene::RemoveEntity( UInt64 uniqueID ) -> void {
        m_EntityCommands.emplace_back( EntityCommand{
                .Type{ EntityCommand::Type::DESTROY },
                .EntityID{ uniqueID },
        } );
    }

    auto Scene::AttachRigidBody( Entity* entity ) const -> void {
        if ( entity == nullptr ) {
            return;
        }

        if ( !entity->HasComponent<RigidBodyComponent>() ) {
            entity->AddComponent<RigidBodyComponent>();
        }

        m_PhysicsWorld->OnRigidBodyAdded( *entity );
    }

    auto Scene::DetachRigidBody( Entity* entity ) const -> void {
        if ( entity == nullptr || entity->HasComponent<RigidBodyComponent>() ) {
            return;
        }

        m_PhysicsWorld->OnRigidBodyRemoved( *entity );
    }

    auto Scene::SetState( const SceneState state ) -> void {
        m_SceneState = state;
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

    auto Scene::CreateEntitySingle( const EntityCreateInfo& createInfo ) -> Entity* {
        MKT_BEGIN_PROFILER_NAMED();

        Entity* result{ nullptr };

        // Register the entity and setup default components
        Unique<Entity> newEntity{ new Entity( m_Registry ) };
        SetupStandardComponents( *newEntity, createInfo );

        UInt64 guid{ newEntity->GetComponent<TagComponent>().GetGUID() };
        const auto [it, success]{
            m_Entities.try_emplace( guid, std::move( newEntity ) )
        };

        if ( success ) {
            // We managed to register the new entity
            result = it->second.get();

            // if root is not empty this entity must be registered as child of root entity
            if ( createInfo.Root != nullptr ) {
                Entity* parent{ createInfo.Root };
                RelationComponent& parentRelation{ parent->GetComponent<RelationComponent>() };

                parentRelation.RegisterChild( guid );
            }

            // in root model is not empty, we create the children for this entity each children well hold a mesh
            if ( !createInfo.Model.IsEmpty() ) {
                if ( createInfo.Model->GetMeshNodeCount() > 1 ) {
                    for ( Size index{}; index < createInfo.Model->GetMeshNodeCount(); index++ ) {
                        AddSingleEntityWithRoot( result, createInfo.Model, index );
                    }
                } else {
                    result->AddComponent<MeshComponent>( createInfo.Model, 0 );
                }
            }
        }

        return result;
    }

    auto Scene::CreateEntity( const EntityCreateInfo& createInfo ) -> Entity* {
        return CreateEntitySingle( createInfo );
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
        m_EntityCommands.push_back( { EntityCommand::Type::CREATE, createInfo, 0 } );
    }

    auto Scene::CreateEntity( std::string_view name ) -> Entity* {
        const EntityCreateInfo info{
            .Root{ nullptr },
            .Name{ name.data() },
            .Model{ ModelHandle::CreateEmpty() }
        };

        return CreateEntity( info );
    }

    auto Scene::GetEntityCount() const -> Size {
        return m_Entities.size();
    }


#if false
    auto Scene::OnTextComponentAttach( entt::registry &reg, const entt::entity entity ) -> void {
        MaterialComponent& materialComponent{ reg.emplace_or_replace<MaterialComponent>(entity) };
    }

    auto Scene::OnRenderComponentAttach( entt::registry &reg, entt::entity entity ) -> void {
        MaterialComponent& materialComponent{ reg.emplace_or_replace<MaterialComponent>(entity) };
    }

    auto Scene::OnRigidBodyComponentAttach( entt::registry &reg, entt::entity entity ) -> void {
        if (reg.all_of<TransformComponent>(entity)) {
            auto& transformComponent{ reg.get<TransformComponent>(entity) };
            auto& rigidBodyComponent{ reg.get<RigidBodyComponent>(entity) };

            CreateRigidBody(entity, transformComponent, rigidBodyComponent);
        }
    }

    auto Scene::ConstructCommonComponents( Entity *entity, std::string_view name ) -> void {
        if ( !entity ) {
            return;
        }

        // Transformation default values
        constexpr glm::vec3 ENTITY_INITIAL_SIZE{ 1.0f, 1.0f, 1.0f };
        constexpr glm::vec3 ENTITY_INITIAL_POSITION{ 0.0, 0.0, 0.0 };
        constexpr glm::vec3 ENTITY_INITIAL_ROTATION{ 0.0f, 0.0f, 0.0f };

        entity->AddComponent<RelationComponent>();

        entity->AddComponent<TagComponent>( name );

        entity->AddComponent<TransformComponent>( ENTITY_INITIAL_POSITION, ENTITY_INITIAL_SIZE, ENTITY_INITIAL_ROTATION );
    }

    auto Scene::CreateRigidBody( entt::entity entity, TransformComponent &transformComponent, RigidBodyComponent &rigidBodyComponent ) -> void {
    }

    auto Scene::UpdateIdle( double deltaTime ) -> void {
        RemoveQueuedEntities();

        m_SceneRenderer->SetCamera( *m_SceneCamera );
        m_SceneRenderer->SetProjection( m_SceneCamera->GetProjection() );

        m_SceneRenderer->BeginFrame();

        // Register models
        const auto renderObjectsView{ m_Registry.view<TagComponent, TransformComponent, RenderComponent, MaterialComponent>() };
        for (const entt::entity &entity: renderObjectsView) {
            TagComponent &tagComponent{ m_Registry.get<TagComponent>( entity ) };
            RenderComponent &renderComponent{ m_Registry.get<RenderComponent>( entity ) };
            MaterialComponent &materialComponent{ m_Registry.get<MaterialComponent>( entity ) };
            TransformComponent &transformComponent{ m_Registry.get<TransformComponent>( entity ) };

            m_SceneRenderer->AddToDrawQueue( {
                .Tag{ std::addressof( tagComponent ) },
                .RenderComponent{ std::addressof( renderComponent ) },
                .Material{ std::addressof( materialComponent ) },
                .Transform{ std::addressof( transformComponent ) }
            } );
        }

        // Register text
        const auto renderText{ m_Registry.view<TagComponent, TransformComponent, TextComponent>() };
        for (const entt::entity &entity: renderText) {
            TagComponent &tagComponent{ m_Registry.get<TagComponent>( entity ) };
            TextComponent &textComponent{ m_Registry.get<TextComponent>( entity ) };
            TransformComponent &transformComponent{ m_Registry.get<TransformComponent>( entity ) };

            textComponent.SetCamera( m_SceneCamera );

            m_SceneRenderer->AddToDrawQueue( {
                .Tag{ std::addressof( tagComponent ) },
                .TextComponent{ std::addressof( textComponent ) },
                .Transform{ std::addressof( transformComponent ) }
            } );
        }

        // Register Lights
        const auto lightObjectsView{ m_Registry.view<TagComponent, TransformComponent, LightComponent>() };
        for (const entt::entity &entity: lightObjectsView) {
            TagComponent &tagComponent{ m_Registry.get<TagComponent>( entity ) };
            LightComponent &lightComponent{ m_Registry.get<LightComponent>( entity ) };
            TransformComponent &transformComponent{ m_Registry.get<TransformComponent>( entity ) };

            lightComponent.UpdatePosition( glm::vec4{ transformComponent.GetTranslation(), 1.0f } );

            lightComponent.GetData().SpotLightData.Direction = glm::vec4{ transformComponent.GetRotation(), 1.0f };
            lightComponent.GetData().DireLightData.Direction = glm::vec4{ transformComponent.GetRotation(), 1.0f };

            if (tagComponent.IsVisible()) {
                m_SceneRenderer->AddLight(
                        tagComponent.GetGUID(),
                        lightComponent.GetData(),
                        lightComponent.GetActiveType() );
            }
        }

        m_SceneRenderer->EndFrame();
    }

    auto Scene::UpdateSimulate( double deltaTime ) -> void {
        RemoveQueuedEntities();

        m_SceneRenderer->SetCamera( *m_SceneCamera );
        m_SceneRenderer->SetProjection( m_SceneCamera->GetProjection() );

        m_SceneRenderer->BeginFrame();

        // Register models
        const auto renderObjectsView{ m_Registry.view<TagComponent, TransformComponent, RenderComponent, MaterialComponent>() };
        for ( const entt::entity &entity: renderObjectsView ) {
            TagComponent &tagComponent{ m_Registry.get<TagComponent>( entity ) };
            RenderComponent &renderComponent{ m_Registry.get<RenderComponent>( entity ) };
            MaterialComponent &materialComponent{ m_Registry.get<MaterialComponent>( entity ) };
            TransformComponent &transformComponent{ m_Registry.get<TransformComponent>( entity ) };

            m_SceneRenderer->AddToDrawQueue( { .Tag{ std::addressof( tagComponent ) },
                                               .RenderComponent{ std::addressof( renderComponent ) },
                                               .Material{ std::addressof( materialComponent ) },
                                               .Transform{ std::addressof( transformComponent ) } } );
        }

        // Register text
        const auto renderText{ m_Registry.view<TagComponent, TransformComponent, TextComponent>() };
        for ( const entt::entity &entity: renderText ) {
            TagComponent &tagComponent{ m_Registry.get<TagComponent>( entity ) };
            TextComponent &textComponent{ m_Registry.get<TextComponent>( entity ) };
            TransformComponent &transformComponent{ m_Registry.get<TransformComponent>( entity ) };

            textComponent.SetCamera( m_SceneCamera );

            m_SceneRenderer->AddToDrawQueue( { .Tag{ std::addressof( tagComponent ) },
                                               .TextComponent{ std::addressof( textComponent ) },
                                               .Transform{ std::addressof( transformComponent ) } } );
        }

        // Register Lights
        const auto lightObjectsView{ m_Registry.view<TagComponent, TransformComponent, LightComponent>() };
        for ( const entt::entity &entity: lightObjectsView ) {
            TagComponent &tagComponent{ m_Registry.get<TagComponent>( entity ) };
            LightComponent &lightComponent{ m_Registry.get<LightComponent>( entity ) };
            TransformComponent &transformComponent{ m_Registry.get<TransformComponent>( entity ) };

            lightComponent.UpdatePosition( glm::vec4{ transformComponent.GetTranslation(), 1.0f } );

            lightComponent.GetData().SpotLightData.Direction = glm::vec4{ transformComponent.GetRotation(), 1.0f };
            lightComponent.GetData().DireLightData.Direction = glm::vec4{ transformComponent.GetRotation(), 1.0f };

            if ( tagComponent.IsVisible() ) {
                m_SceneRenderer->AddLight(
                        tagComponent.GetGUID(),
                        lightComponent.GetData(),
                        lightComponent.GetActiveType() );
            }
        }

        m_SceneRenderer->EndFrame();


        auto Scene::AddEmptyEntity( const std::string_view tagName, const Entity *root ) -> Entity * {
            Scope_T<Entity> newEntity{ CreateScope<Entity>( m_Registry ) };

            SetupEntityBaseProperties( *newEntity, tagName );

            if ( root == nullptr ) {
                m_Hierarchy.Insert( newEntity.get() );
            } else {
                m_Hierarchy.InsertChild( [&root]( const Entity *parent ) -> bool { return parent->Get() == root->Get(); }, newEntity.get() );
            }

            m_Entities.emplace_back( std::move( newEntity ) );
            return m_Entities.back().get();
        }

        auto Scene::RemoveFromLights( const UInt64_T uniqueID ) -> void {
            const auto findLightsIt{ std::ranges::find_if( m_Lights, [&]( Entity *entity ) -> bool { return entity->GetComponent<TagComponent>().GetGUID() == uniqueID; } ) };

            if ( findLightsIt != m_Lights.end() ) { m_Lights.erase( findLightsIt ); }
        }

        auto Scene::RemoveFromEntities( const UInt64_T uniqueID ) -> Scope_T<Entity> {
            const auto result{ std::ranges::find_if( m_Entities, [&]( const Scope_T<Entity> &entity ) -> bool { return entity->GetComponent<TagComponent>().GetGUID() == uniqueID; } ) };

            Scope_T<Entity> entity{ nullptr };

            if ( result != m_Entities.end() ) {
                entity = std::move( *result );
                m_Entities.erase( result );

                if ( entity->HasAnyOfComponents<RenderComponent, TextComponent>() ) {
                    m_SceneRenderer->RemoveFromDrawQueue( entity->GetComponent<TagComponent>().GetGUID() );
                }

                if ( entity->HasComponent<LightComponent>() ) { m_SceneRenderer->RemoveLight( uniqueID ); }
            }

            return entity;
        }

        auto Scene::RemoveFromHierarchy( Entity & target ) -> void {
            // List of entities to erase from hierarchy
            std::vector<entt::entity> entitiesToErase{};

            // we add the root node
            entitiesToErase.emplace_back( target.Get() );

            // If they are lights erase them
            m_Hierarchy.ForAllChildren(
                    [&]( Entity *ent ) {
                        if ( ent->HasComponent<LightComponent>() ) { RemoveFromLights( ent->GetComponent<TagComponent>().GetGUID() ); }

                        // Add children to be erased
                        entitiesToErase.emplace_back( ent->Get() );
                    },
                    [&target]( Entity *ent ) {
                        return ent->GetComponent<TagComponent>().GetGUID() ==
                               target.GetComponent<TagComponent>().GetGUID();
                    } );

            // Erase node and its children from the hierarchy
            const auto result{ m_Hierarchy.Erase(
                    [&target]( Entity *ent ) {
                        return ent->GetComponent<TagComponent>().GetGUID() ==
                               target.GetComponent<TagComponent>().GetGUID();
                    } ) };

            if ( result ) {
                // Erase entities from entt structures
                for ( const entt::entity &entity: entitiesToErase ) { m_Registry.destroy( entity ); }
            }
        }

        auto Scene::RemoveQueuedEntities() -> void {
            if ( m_ToRemoveEntities.empty() ) { return; }

            for ( const UInt64_T entityID: m_ToRemoveEntities ) { DestroyEntity( entityID ); }

            m_ToRemoveEntities.clear();
        }

        auto Scene::OnDestroyRenderComponent( entt::registry &, entt::entity ) -> void {
        }

        auto Scene::OnConstructRenderComponent( entt::registry &, entt::entity ) -> void {
        }

        auto Scene::DestroyEntity( const UInt64_T uniqueID ) -> bool {
            const Scope_T<Entity> target{ RemoveFromEntities( uniqueID ) };

            if ( target == nullptr ) { return false; }

            // Erase children from hierarchy, entities and lights
            const auto children{ FindChildrenByID( uniqueID ) };

            // Temporarily hold the children to remove them from lights
            std::vector<Scope_T<Entity>> childrenPtrs{};

            for ( Entity *child: children ) {
                // Hold the pointer otherwise it gets deleted and then we can't remove it from lights because it's not valid
                childrenPtrs.emplace_back( RemoveFromEntities( child->GetComponent<TagComponent>().GetGUID() ) );

                if ( childrenPtrs.back()->HasComponent<LightComponent>() ) { RemoveFromLights( child->GetComponent<TagComponent>().GetGUID() ); }
            }

            // Erase parent (which erases children too)
            if ( target->HasComponent<LightComponent>() ) { RemoveFromLights( uniqueID ); }

            RemoveFromHierarchy( *target );

            return true;
        }

        auto Scene::RemoveEntity( UInt64_T uniqueID ) -> void { m_ToRemoveEntities.emplace_back( uniqueID ); }

        auto Scene::FindEntityByID( const UInt64_T uniqueID ) -> Entity * {

            const auto result{ std::ranges::find_if( m_Entities, [&]( const Scope_T<Entity> &entity ) -> bool { return entity->GetComponent<TagComponent>().GetGUID() == uniqueID; } ) };

            return result == m_Entities.end() ? nullptr : result->get();
        }

        auto Scene::FindFirstEntityByName( const std::string_view name ) -> Entity * {
            const auto result{
                std::ranges::find_if( m_Entities,
                                      [&]( const Scope_T<Entity> &entity ) -> bool { return entity->GetComponent<TagComponent>().GetTag() == name; } )
            };

            return result == m_Entities.end() ? nullptr : result->get();
        }

        auto Scene::FindChildrenByID( UInt64_T uniqueID ) -> std::vector<Entity *> {
            std::vector<Entity *> children{};

            m_Hierarchy.ForAllChildren(
                    [&]( Entity *entity ) { children.emplace_back( entity ); },
                    [&uniqueID]( Entity *entity ) { return entity->GetComponent<TagComponent>().GetGUID() == uniqueID; } );

            return children;
        }

        auto Scene::CreateEntity( const EntityCreateInfo &createInfo ) -> Entity * {
            if ( createInfo.ModelMesh == nullptr ) { return AddEmptyEntity( createInfo.Name, createInfo.Root ); }

            // For each mesh from the model we create an entity,
            // The idea later is to be able to construct one mesh from individual meshes
            // and not split them as it is right now. Although if the root is not empty
            Entity *newEntityRoot{ AddEmptyEntity( createInfo.Name, createInfo.Root ) };

            for ( auto &mesh: createInfo.ModelMesh->GetMeshes() ) {
                // If there is only one child, and we already have a root entity,
                // There's no need to create a new root entity for this single child mesh
                Entity *child{ createInfo.ModelMesh->GetMeshes().size() == 1 &&
                                               createInfo.Root != nullptr
                                       ? newEntityRoot
                                       : AddEmptyEntity( mesh->GetName(), newEntityRoot ) };

                newEntityRoot->GetComponent<RelationComponent>().RegisterChild( child->GetComponent<TagComponent>().GetGUID() );

                MaterialComponent &materialComponent{ child->AddComponent<MaterialComponent>() };
                RenderComponent &renderComponent{ child->AddComponent<RenderComponent>() };

                PBRMaterialCreateSpec pbrMaterialCreateSpec{
                    .Name{ fmt::format( "Material standard - {}", mesh->GetName() ) }
                };

                SetMaterialTextures( mesh, pbrMaterialCreateSpec );

                renderComponent.SetMesh( mesh.get() );
                materialComponent.SetMaterial( PBRMaterial::Create( pbrMaterialCreateSpec ) );

                // StandardMaterialCreateInfo spec{
                //     .name{ fmt::format( "Material standard - {}", mesh->GetName() ) },
                //     .DiffuseMap{ nullptr },
                //     .SpecularMap{ nullptr },
                // };
                // materialComponent.SetMaterial(StandardMaterial::Create(spec));
            }

            return newEntityRoot;
        }

        auto Scene::Clear() -> void {
            // Remove entities from draw queue
            for ( const auto &entity: m_Entities ) {
                if ( entity->HasAnyOfComponents<RenderComponent, TextComponent>() ) {
                    m_SceneRenderer->RemoveFromDrawQueue( entity->GetComponent<TagComponent>().GetGUID() );
                }

                if ( entity->HasComponent<LightComponent>() ) { RemoveFromLights( entity->GetComponent<TagComponent>().GetGUID() ); }
            }

            m_Lights.clear();
            m_Hierarchy.Clear();
            m_Entities.clear();

            // Clear entt registry
            m_Registry.clear();

            m_SceneCamera = nullptr;
            m_SceneRenderer = nullptr;
        }

        auto Scene::SetCamera( const SceneCamera &camera ) -> void { m_SceneCamera = std::addressof( camera ); }
        auto Scene::SetRenderer( RendererBackend & renderer ) -> void { m_SceneRenderer = std::addressof( renderer ); }

        auto Scene::OnViewPortResize( const float width, const float height ) -> void {
            // Resize non-fixed aspect ratio cameras
            const auto view{ m_Registry.view<TransformComponent, CameraComponent>() };

            for ( const auto &entity: view ) {
                TransformComponent &transformComponent{ view.get<TransformComponent>( entity ) };
                CameraComponent &cameraComponent{ view.get<CameraComponent>( entity ) };

                if ( !cameraComponent.IsAspectRatioFixed() ) { cameraComponent.GetCamera().SetViewportSize( width, height ); }
            }
        }

    }

    auto Scene::RemoveEntity( UInt64_T uniqueID ) -> void {
    }

    auto Scene::FindByID( UInt64_T uniqueID ) -> Entity * {
    }

    auto Scene::FindFirstByName( std::string_view name ) -> Entity * {
    }

    auto Scene::CreateEntity( const EntityCreateInfo &createInfo ) -> Entity * {
    }

    auto Scene::GetName() const -> const std::string & {
    }

    auto Scene::GetEntities() -> ankerl::unordered_dense::map<UInt64_T, Entity> & {
    }

    auto Scene::GetEntities() const -> const ankerl::unordered_dense::map<UInt64_T, Entity> & {
    }

    auto Scene::Clear() -> void {
    }

    auto Scene::RemoveQueuedEntities() -> void {
    }

    auto Scene::DestroyEntity( UInt64_T uniqueID ) -> bool {
    }

    auto Scene::AddEmptyEntity( std::string_view tagName, const Entity *root ) -> Entity * {
    }

#endif

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
        m_Registry.destroy( m_Entities[entityID]->m_Handle );
        m_Entities.erase( entityID );

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

    auto Scene::AddSingleEntityWithRoot( Entity* root, ModelHandle model, Int32 index ) -> void {
        std::string name{ model->GetMeshNode( index ).GetName() };
        if ( name.empty() ) {
            name = fmt::format( "{} ({})", model->GetName(), index );
        }

        const EntityCreateInfo entityCreateInfo{
            .Root{ root },
            .Name{ name.c_str() },
        };

        Entity* child{ CreateEntity( entityCreateInfo ) };

        if ( child != nullptr ) {
            child->AddComponent<MeshComponent>( model, index );
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