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
#include <Common/Random.hh>
#include <Common/RenderingUtils.hh>
#include <Renderer/Renderer.hh>

#include "Scene/Component.hh"
#include "Scene/EditorCamera.hh"
#include "Scene/Entity.hh"
#include "Scene/Scene.hh"

namespace Mikoto {
    auto Scene::OnRuntimeUpdate(double ts) -> void {
        UpdateScripts();

        std::shared_ptr<SceneCamera> mainCam{};
        bool sceneHasMainCam{ false };
        auto viewForCameraLookUp{ m_Registry.view<TransformComponent, CameraComponent>() };

        for (const auto& entity : viewForCameraLookUp) {
            TransformComponent& transform{ viewForCameraLookUp.get<TransformComponent>(entity) };
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
            prepareData.RuntimeCamera = &(*mainCam);

            Renderer::BeginScene(prepareData);

            auto view{ m_Registry.view<TagComponent, TransformComponent, RenderComponent, MaterialComponent>() };

            for (auto& sceneObject: view) {
                TagComponent& tag{ view.get<TagComponent>(sceneObject) };
                TransformComponent& transform{ view.get<TransformComponent>(sceneObject) };
                RenderComponent& renderComponent{ view.get<RenderComponent>(sceneObject) };
                MaterialComponent& material{ view.get<MaterialComponent>(sceneObject) };

                SceneObjectData& objectData{ renderComponent.GetObjectData() };

                Renderer::Submit(objectData, transform.GetTransform());
            }

            Renderer::Flush();
            Renderer::EndScene();
        }
    }

    auto Scene::CreateEmptyObject(std::string_view tagName, UInt64_T guid) -> Entity {
        Entity result{ m_Registry.create(), m_Registry };
        result.AddComponent<TagComponent>(tagName, guid);
        result.AddComponent<TransformComponent>(ENTITY_INITIAL_POSITION, ENTITY_INITIAL_SIZE, ENTITY_INITIAL_ROTATION);

        return result;
    }

    auto Scene::CreatePrefab(std::string_view tagName, PrefabSceneObject type, UInt64_T guid) -> Entity {
        Entity result{ m_Registry.create(), m_Registry };

        // Setup tag and transform
        result.AddComponent<TagComponent>(tagName, guid);
        result.AddComponent<TransformComponent>(ENTITY_INITIAL_POSITION, ENTITY_INITIAL_SIZE, ENTITY_INITIAL_ROTATION);

        // Setup material component
        result.AddComponent<MaterialComponent>();
        auto& materialComponent{ result.GetComponent<MaterialComponent>() };

        // Setup renderer component
        result.AddComponent<RenderComponent>();
        auto& renderData{ result.GetComponent<RenderComponent>() };
        renderData.GetObjectData().IsPrefab = true;
        renderData.GetObjectData().PrefabType = type;

        // Add a material for each one of the meshes of this model
        auto& model{ AssetsManager::GetModifiableModelPrefabByType(type) };

        for (auto& mesh : model.GetMeshes()) {
            DefaultMaterialCreateSpec spec{};
            PBRMaterialCreateSpec specPbr{};

            for (auto textureIt{ mesh.GetTextures().begin() }; textureIt != mesh.GetTextures().end(); ++textureIt) {
                switch ( (*textureIt)->GetType() ) {
                    case MapType::TEXTURE_2D_DIFFUSE:
                        spec.DiffuseMap = *textureIt;
                        specPbr.AlbedoMap = *textureIt;
                        break;
                    case MapType::TEXTURE_2D_SPECULAR:
                        spec.SpecularMap = *textureIt;
                        break;
                }
            }

            mesh.AddMaterial(Material::CreateStandardMaterial(spec));
            //mesh.AddMaterial(Material::CreatePBRMaterial(specPbr));
        }

        renderData.GetObjectData().ObjectModel = std::addressof(model);

        return result;
    }

    auto Scene::DestroyEntity(Entity& entity) -> void {
        m_Registry.destroy(entity.m_EntityHandle);
        entity.Invalidate();
    }

    auto Scene::OnViewPortResize(UInt32_T width, UInt32_T height) -> void {
        // Resize non-fixed aspect ratio cameras
        auto view{ m_Registry.view<TransformComponent, CameraComponent>() };

        for (const auto& entity : view) {
            TransformComponent& transform{ view.get<TransformComponent>(entity) };
            CameraComponent& camera{ view.get<CameraComponent>(entity) };

            if (!camera.IsAspectRatioFixed())
                camera.GetCameraPtr()->SetViewportSize(width, height);

        }
    }

    auto Scene::OnEditorUpdate(MKT_UNUSED_VAR double timeStep, const EditorCamera& camera) -> void {
        // Prepare scene data
        ScenePrepareData prepareData{};
        prepareData.StaticCamera = std::addressof(camera);

        // Begin scene
        Renderer::BeginScene( prepareData );

        // Setup lighting data
        Size_T dirIndexCount{}, spotIndexCount{}, pointIndexCount{};

        Renderer::SetLightsViewPos( glm::vec4{ camera.GetPosition(), 1.0f } );

        for ( auto& lightSource: m_Registry.view<LightComponent>() ) {
            LightComponent& lightComponent{ m_Registry.get<LightComponent>( lightSource ) };

            switch ( lightComponent.GetType() ) {
                case LightType::DIRECTIONAL_LIGHT_TYPE:
                    lightComponent.GetDirLightData().Position =
                            glm::vec4{ m_Registry.get<TransformComponent>( lightSource ).GetTranslation(), 1.0f };
                    Renderer::SetDirLightInfo( lightComponent.GetDirLightData(), dirIndexCount++ );
                    break;
                case LightType::POINT_LIGHT_TYPE:
                    lightComponent.GetPointLightData().Position =
                            glm::vec4{ m_Registry.get<TransformComponent>( lightSource ).GetTranslation(), 1.0f };
                    Renderer::SetPointLightInfo( lightComponent.GetPointLightData(), pointIndexCount++ );
                    break;
                case LightType::SPOT_LIGHT_TYPE:
                    lightComponent.GetSpotLightData().Position =
                            glm::vec4{ m_Registry.get<TransformComponent>( lightSource ).GetTranslation(), 1.0f };
                    Renderer::SetSpotLightInfo( lightComponent.GetSpotLightData(), spotIndexCount++ );
                    break;
            }

            Renderer::SetActivePointLightsCount( pointIndexCount );
            Renderer::SetActiveDirLightsCount( dirIndexCount );
            Renderer::SetActiveSpotLightsCount( spotIndexCount );
        }

        // Push meshes to render queue
        auto view{ m_Registry.view<TagComponent, TransformComponent, RenderComponent, MaterialComponent>() };
        for ( auto& sceneObject: view ) {
            TagComponent& tag{ view.get<TagComponent>( sceneObject ) };
            TransformComponent& transform{ view.get<TransformComponent>( sceneObject ) };
            RenderComponent& renderComponent{ view.get<RenderComponent>( sceneObject ) };
            MaterialComponent& material{ view.get<MaterialComponent>( sceneObject ) };

            SceneObjectData& objectData{ renderComponent.GetObjectData() };

            if ( tag.IsVisible() && objectData.ObjectModel  ) {
                Renderer::Submit( objectData, transform.GetTransform() );
            }
        }

        Renderer::EndScene();
        Renderer::Flush();
    }

    auto Scene::UpdateScripts() -> void {
        auto view{ m_Registry.view<TagComponent, TransformComponent, NativeScriptComponent>() };

        for (const auto& entity : view) {
            TagComponent& tag{ view.get<TagComponent>(entity) };
            TransformComponent& transform{ view.get<TransformComponent>(entity) };
            NativeScriptComponent& script{ view.get<NativeScriptComponent>(entity) };

            // Update scripts
            {

            }

        }
    }

    auto Scene::Clear() -> void {
        auto view{ m_Registry.view<TagComponent>() };
        m_Registry.destroy(view.begin(), view.end());
    }

    auto Scene::GetActiveScene() -> Scene* {
        return s_ActiveScene;
    }

    auto Scene::SetActiveScene(Scene* scene) -> void {
        s_ActiveScene = scene;
    }

    auto Scene::FetchSceneMetaData() -> SceneMetaData& {
        m_MetaData.EntityCount = m_Registry.view<TagComponent>().size();

        auto viewLights{ m_Registry.view<LightComponent>() };
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