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
#include <Assets/AssetsManager.hh>
#include <Common/Constants.hh>
#include <Common/RenderingUtils.hh>
#include <Renderer/Core/RenderQueue.hh>
#include <Renderer/Core/Renderer.hh>
#include <STL/Random/Random.hh>
#include <Scene/Entity/Scene.hh>

#include "Models/StandardMaterialCreateData.hh"

namespace Mikoto {

#if false // NOT USED YET
    auto Scene::OnRuntimeUpdate(double ts) -> void {
        auto sceneHasMainCam{ false };
        std::shared_ptr<SceneCamera> mainCam{};

        const auto viewForCameraLookUp{ m_Registry.view<TransformComponent, CameraComponent>() };
        for ( const auto& entity : viewForCameraLookUp) {
            const auto& transform{ viewForCameraLookUp.get<TransformComponent>(entity) };
            CameraComponent& camera{ viewForCameraLookUp.get<CameraComponent>(entity) };
            sceneHasMainCam = camera.IsMainCamera();

            if (camera.IsMainCamera()) {
                sceneHasMainCam = true;
                mainCam = camera.GetCameraPtr();

                // The camera's position and rotation depend on its transform component
                mainCam->SetPosition(transform.GetTranslation());
                mainCam->SetRotation(transform.GetRotation());
                break;
            }
        }

        if (sceneHasMainCam) {
            ScenePrepareData prepareData{};
            prepareData.RuntimeCamera = mainCam.get();

            Renderer::BeginScene(prepareData);

            for ( const auto view{ m_Registry.view<TagComponent, TransformComponent, RenderComponent, MaterialComponent>() };
                auto& sceneObject: view)
            {
                TagComponent& tag{ view.get<TagComponent>(sceneObject) };
                TransformComponent& transform{ view.get<TransformComponent>(sceneObject) };
                RenderComponent& renderComponent{ view.get<RenderComponent>(sceneObject) };
                MaterialComponent& material{ view.get<MaterialComponent>(sceneObject) };

                GameObject& objectData{renderComponent.GetObjectData() };

                objectData.Transform.Transform = transform.GetTransform();

                RenderQueue::Submit(std::make_unique<RenderCommandPushDraw>( std::to_string(tag.GetGUID()), objectData, *material.GetMaterialInfo().MeshMat));
            }

            Renderer::EndScene();
        }
    }
#endif

    auto Scene::Render(const SceneRenderData& data ) -> void {
        const ScenePrepareData prepareData{
            .StaticCamera{ data.Camera },
            .CameraPosition{ glm::vec4{ data.Camera->GetPosition(), 1.0f } }
        };

        Renderer::BeginScene( prepareData );

        RenderQueue::Submit( std::make_unique<RenderCommandSetClearColor>( data.ClearColor ) );

        // Register models
        for ( auto& sceneObject: m_Registry.view<RenderComponent>() ) {
            Entity entity{ sceneObject, m_Registry };
            auto& tagComponent{ entity.GetComponent<TagComponent>() };
            auto& transformComponent{ entity.GetComponent<TransformComponent>() };
            auto& renderComponent{ entity.GetComponent<RenderComponent>() };
            auto& materialComponent{ entity.GetComponent<MaterialComponent>() };

            auto& objectData{ renderComponent.GetObjectData() };
            if ( tagComponent.IsVisible() && objectData.MeshData.Data != nullptr ) {
                objectData.Transform.Transform = transformComponent.GetTransform();
                objectData.Transform.View = prepareData.StaticCamera->GetViewMatrix();
                objectData.Transform.Projection = prepareData.StaticCamera->GetProjection();

                RenderQueue::Submit(std::make_unique<RenderCommandPushDraw>( std::to_string(tagComponent.GetGUID()), objectData, *materialComponent.GetMaterialInfo().MeshMat));
            }
        }

        // Register Lights
        for ( auto& lightSource: m_Registry.view<LightComponent>() ) {
            Entity lightEntity{ lightSource, m_Registry };

            auto& lightComponent{ lightEntity.GetComponent<LightComponent>() };
            auto& tagComponent{ lightEntity.GetComponent<TagComponent>() };
            auto& transformComponent{ lightEntity.GetComponent<TransformComponent>() };

            LightRenderInfo info{
                .Type{ lightComponent.GetType() },
                .Data{ lightComponent.GetData() },
                .IsActive{ tagComponent.IsVisible() }
            };

            info.Data.DireLightData.Position = glm::vec4{ transformComponent.GetTranslation(), 1.0f };
            info.Data.SpotLightData.Position = glm::vec4{ transformComponent.GetTranslation(), 1.0f };
            info.Data.PointLightDat.Position = glm::vec4{ transformComponent.GetTranslation(), 1.0f };

            Renderer::AddLightObject(std::to_string( tagComponent.GetGUID() ), info);
        }

        Renderer::EndScene();
    }

    auto Scene::CreateEmptyEntity(std::string_view tagName, const Entity *root, UInt64_T guid) -> Entity {
        Entity newEntity{m_Registry.create(), m_Registry };
        newEntity.AddComponent<TagComponent>(tagName, guid);
        newEntity.AddComponent<TransformComponent>(ENTITY_INITIAL_POSITION, ENTITY_INITIAL_SIZE, ENTITY_INITIAL_ROTATION);

        if (root == nullptr) {
            m_Hierarchy.Insert( newEntity.m_EntityHandle, m_Registry );
        } else {
            m_Hierarchy.InsertChild([&root](const auto& parent) -> bool {
                return parent.Get() == root->Get();
            },
            newEntity.m_EntityHandle, m_Registry );
        }

        return newEntity;
    }

    auto Scene::CreatePrefabEntity(std::string_view tagName, Model *model, Entity *root, UInt64_T guid) -> Entity {
        if (model != nullptr) {
            std::vector<Entity> children{};

            for (auto& mesh : model->GetMeshes()) {
                // We treat each mesh as an individual game object
                Entity child{ m_Registry.create(), m_Registry };

                // Setup tag and transform
                child.AddComponent<TagComponent>(mesh.GetName(), GenerateGUID());
                child.AddComponent<TransformComponent>(ENTITY_INITIAL_POSITION, ENTITY_INITIAL_SIZE, ENTITY_INITIAL_ROTATION);

                auto& material{ child.AddComponent<MaterialComponent>() };
                auto& renderData{ child.AddComponent<RenderComponent>() };

                StandardMaterialCreateData spec{};

                for (auto& textureIt: mesh.GetTextures()) {
                    switch ( textureIt->GetType() ) {
                        case MapType::TEXTURE_2D_DIFFUSE:
                            spec.DiffuseMap = textureIt;
                            break;
                        case MapType::TEXTURE_2D_SPECULAR:
                            spec.SpecularMap = textureIt;
                            break;
                        default:
                            MKT_CORE_LOGGER_INFO("Scene::CreatePrefabEntity - Mesh has no data for the requested texture type.");
                    }
                }

                renderData.GetObjectData().MeshData.Data = std::addressof(mesh);
                material.GetMaterialInfo().MeshMat = Material::CreateStandardMaterial(spec);

                children.emplace_back(child);
            }

            if (root == nullptr) {
                Entity rootEntity = { m_Registry.create(), m_Registry };
                rootEntity.AddComponent<TagComponent>(tagName, guid);
                rootEntity.AddComponent<TransformComponent>(ENTITY_INITIAL_POSITION, ENTITY_INITIAL_SIZE, ENTITY_INITIAL_ROTATION);
                m_Hierarchy.Insert( rootEntity.m_EntityHandle, m_Registry );

                m_Hierarchy.InsertMultiple( [&rootEntity](const auto& ent) { return ent == rootEntity; },
                children.begin(), children.end());

                return rootEntity;
            } else {
                m_Hierarchy.InsertMultiple( [&root](const auto& ent) { return ent == *root; },
                children.begin(), children.end());

                return *root;
            }
        }

        return Entity{};
    }

    auto Scene::DestroyEntity(Entity& target) -> bool {
        auto destroyIfLight{
            [](Entity& ent) {
                if (ent.HasComponent<LightComponent>()) {
                    Renderer::RemoveLightObject( std::to_string( ent.GetComponent<TagComponent>().GetGUID() ) );
                }
            }
        };

        destroyIfLight(target);

        // Contains all the nodes that have to be deleted (the target and all of its children)
        std::vector<entt::entity> children{};

        children.emplace_back( target.Get() );

        // Remove parent and children from drawing queue
        const auto& tag{ target.GetComponent<TagComponent>() };
        RenderQueue::Submit( std::make_unique<RenderCommandPopDraw>( std::to_string(tag.GetGUID() ) ) );
        m_Hierarchy.ForAllChildren(
            [&children, &destroyIfLight](Entity& ent) {
                const auto& tagComponent{ ent.GetComponent<TagComponent>() };
                destroyIfLight(ent);
                RenderQueue::Submit( std::make_unique<RenderCommandPopDraw>( std::to_string(tagComponent.GetGUID()) ) );

                children.emplace_back(ent.Get());
            },
            [&target](const auto& ent) { return ent == target; } );

        // Erase node and its children from the hierarchy
        const auto result{ m_Hierarchy.Erase(
            [&target](const auto& ent) {
                return target == ent;
            } )
        };

        // Erase node and its children from the registry
        for ( const auto& child : children) {
            m_Registry.destroy( child );
        }

        return result;
    }

    auto Scene::ResizeViewport( const UInt32_T width, const UInt32_T height) -> void {
        // Resize non-fixed aspect ratio cameras

        for ( const auto view{ m_Registry.view<TransformComponent, CameraComponent>() }; const auto& entity : view) {
            TransformComponent& transform{ view.get<TransformComponent>(entity) };
            CameraComponent& camera{ view.get<CameraComponent>(entity) };

            if (!camera.IsAspectRatioFixed())
                camera.GetCameraPtr()->SetViewportSize(width, height);

        }
    }

    auto Scene::Clear() -> void {
        m_Registry.clear();
    }

    Scene::~Scene() {

    }

    auto Scene::GetSceneMetaData() -> SceneMetaData& {
        m_MetaData.EntityCount = m_Registry.view<TagComponent>().size();

        const auto viewLights{ m_Registry.view<LightComponent>() };
        m_MetaData.LightsCount = viewLights.size();

        for (auto& light : viewLights) {
            LightComponent& lightComponent{ m_Registry.get<LightComponent>(light) };

            // Directional light count
            if (lightComponent.GetType() == LightType::DIRECTIONAL_LIGHT_TYPE) {
                m_MetaData.DirLightsCount += 1;
                m_MetaData.DirActiveLightCount += lightComponent.IsActive() ? 1 : 0;
            }

            // Point light count
            if (lightComponent.GetType() == LightType::POINT_LIGHT_TYPE) {
                m_MetaData.PointLightsCount += 1;
                m_MetaData.PointActiveLightCount += lightComponent.IsActive() ? 1 : 0;
            }

            // Spot-light count
            if (lightComponent.GetType() == LightType::SPOT_LIGHT_TYPE) {
                m_MetaData.SpotLightsCount += 1;
                m_MetaData.SpotActiveLightCount += lightComponent.IsActive() ? 1 : 0;
            }
        }

        m_MetaData.ActiveLightCount = m_MetaData.DirActiveLightCount + m_MetaData.PointActiveLightCount + m_MetaData.SpotActiveLightCount;

        return m_MetaData;
    }
}