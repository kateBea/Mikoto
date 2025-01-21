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
#include <Renderer/Core/Renderer.hh>
#include <STL/Random/Random.hh>

#include "Models/StandardMaterialCreateData.hh"
#include "Scene/Component.hh"
#include "Scene/EditorCamera.hh"
#include "Scene/Entity.hh"
#include "Scene/Scene.hh"

namespace Mikoto {

    auto Scene::OnRuntimeUpdate(double ts) -> void {
        std::shared_ptr<SceneCamera> mainCam{};
        auto sceneHasMainCam{ false };

        for ( const auto viewForCameraLookUp{ m_Registry.view<TransformComponent, CameraComponent>() }; const auto& entity : viewForCameraLookUp) {
            const auto& transform{ viewForCameraLookUp.get<TransformComponent>(entity) };
            CameraComponent& camera{ viewForCameraLookUp.get<CameraComponent>(entity) };
            sceneHasMainCam = camera.IsMainCamera();

            if (sceneHasMainCam) {
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

                Renderer::Submit(std::to_string(tag.GetGUID()), objectData, transform.GetTransform(), material.GetMaterialInfo().MeshMat);
            }

            Renderer::Flush();
            Renderer::EndScene();
        }
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
        if (model) {
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
                    switch ( (textureIt)->GetType() ) {
                        case MapType::TEXTURE_2D_DIFFUSE:
                            spec.DiffuseMap = textureIt;
                            break;
                        case MapType::TEXTURE_2D_SPECULAR:
                            spec.SpecularMap = textureIt;
                            break;
                        default:
                            MKT_APP_LOGGER_INFO("Unused type");
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
        // Contains all the nodes that have to be deleted (the target and all of its children)
        std::vector<entt::entity> children{};

        children.emplace_back( target.Get() );

        // Remove parent and children from drawing queue
        auto& tag{ target.GetComponent<TagComponent>() };
        Renderer::RemoveFromDrawQueue(std::to_string(tag.GetGUID()));
        m_Hierarchy.ForAllChildren(
            [&children](Entity& ent) {
                const auto& tagComponent{ ent.GetComponent<TagComponent>() };
                Renderer::RemoveFromDrawQueue(std::to_string(tagComponent.GetGUID()));

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

    auto Scene::OnViewPortResize( const UInt32_T width, const UInt32_T height) -> void {
        // Resize non-fixed aspect ratio cameras

        for ( const auto view{ m_Registry.view<TransformComponent, CameraComponent>() }; const auto& entity : view) {
            TransformComponent& transform{ view.get<TransformComponent>(entity) };
            CameraComponent& camera{ view.get<CameraComponent>(entity) };

            if (!camera.IsAspectRatioFixed())
                camera.GetCameraPtr()->SetViewportSize(width, height);

        }
    }


    auto Scene::OnEditorUpdate(MKT_UNUSED_VAR double timeStep, const EditorCamera& camera) -> void {
        ScenePrepareData prepareData{
            .StaticCamera{ std::addressof(camera) },
        };

        Renderer::BeginScene( prepareData );

        // Setup lighting data
        Size_T dirIndexCount{}, spotIndexCount{}, pointIndexCount{};

        Renderer::SetLightsViewPos( glm::vec4{ camera.GetPosition(), 1.0f } );

        for ( auto& lightSource: m_Registry.view<LightComponent>() ) {
            Entity entity{ lightSource, m_Registry };
            auto& lightComponent{ entity.GetComponent<LightComponent>() };
            auto& tagComponent{ entity.GetComponent<TagComponent>() };
            auto& lightData{ lightComponent.GetData() };

            LightRenderInfo info{
                .Id = std::to_string( tagComponent.GetGUID() ),
                .Type{ lightComponent.GetType() },
                .Data{ lightComponent.GetData() },
                .IsActive{ tagComponent.IsVisible() }
            };

            Renderer::AddLightObject(info);

            //
            // switch ( lightComponent.GetType() ) {
            //     case LightType::DIRECTIONAL_LIGHT_TYPE:
            //         lightComponent.GetDirLightData().Position =
            //                 glm::vec4{ m_Registry.get<TransformComponent>( lightSource ).GetTranslation(), 1.0f };
            //         Renderer::SetDirLightInfo( lightComponent.GetDirLightData(), dirIndexCount++ );
            //         break;
            //     case LightType::POINT_LIGHT_TYPE:
            //         lightComponent.GetPointLightData().Position =
            //                 glm::vec4{ m_Registry.get<TransformComponent>( lightSource ).GetTranslation(), 1.0f };
            //         Renderer::SetPointLightInfo( lightComponent.GetPointLightData(), pointIndexCount++ );
            //         break;
            //     case LightType::SPOT_LIGHT_TYPE:
            //         lightComponent.GetSpotLightData().Position =
            //                 glm::vec4{ m_Registry.get<TransformComponent>( lightSource ).GetTranslation(), 1.0f };
            //         Renderer::SetSpotLightInfo( lightComponent.GetSpotLightData(), spotIndexCount++ );
            //         break;
            // }
            //
            // Renderer::SetActivePointLightsCount( pointIndexCount );
            // Renderer::SetActiveDirLightsCount( dirIndexCount );
            // Renderer::SetActiveSpotLightsCount( spotIndexCount );
        }

        auto renderableObjectsView{ m_Registry.view<TagComponent, TransformComponent, RenderComponent, MaterialComponent>() };
        for ( auto& sceneObject: renderableObjectsView ) {
            TagComponent& tag{ renderableObjectsView.get<TagComponent>( sceneObject ) };
            TransformComponent& transform{ renderableObjectsView.get<TransformComponent>( sceneObject ) };
            RenderComponent& renderComponent{ renderableObjectsView.get<RenderComponent>( sceneObject ) };
            MaterialComponent& material{ renderableObjectsView.get<MaterialComponent>( sceneObject ) };

            GameObject& objectData{ renderComponent.GetObjectData() };
            if ( tag.IsVisible() && objectData.MeshData.Data != nullptr ) {
                Renderer::Submit(std::to_string(tag.GetGUID()), objectData, transform.GetTransform(), material.GetMaterialInfo().MeshMat);
            }
        }

        Renderer::EndScene();
        Renderer::Flush();
    }

    auto Scene::Clear() -> void {
        const auto view{ m_Registry.view<TagComponent>() };
        m_Registry.destroy(view.begin(), view.end());
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