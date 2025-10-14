/**
 * Scene.cc
 * Created by kate on 6/24/23.
 * */

// C++ Standard Library
#include <memory>
#include <utility>
#include <algorithm>

// Third-Party Libraries
#include <entt/entt.hpp>

// Project Headers
#include <Library/Random/Random.hh>
#include <Scene/Component.hh>
#include <Scene/Scene.hh>

namespace Mikoto {
    Scene::Scene( const std::string_view name )
        : m_Name{ name } {
        // Install component listeners
    }

    auto Scene::SetName( std::string_view name ) -> void {
        m_Name = name;
    }

    auto Scene::CreateEntity( std::string_view name ) -> Entity * {
        return nullptr;
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

    auto Scene::Create( std::string_view name ) -> Unique<Scene> {
        return CreateScope<Scene>( name );
    }

    Scene::~Scene() {

    }
}