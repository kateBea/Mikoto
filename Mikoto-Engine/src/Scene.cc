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
#include <Common/Random.hh>

#include <Assets/AssetsManager.hh>

#include "EditorCamera.hh"
#include "Scene/Component.hh"
#include "Scene/Entity.hh"
#include "Scene/Scene.hh"

#include <Common/RenderingUtils.hh>
#include <Renderer/Renderer.hh>

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
                objectData.Color = material.GetColor();

                Renderer::Submit(objectData, transform.GetTransform());
            }

            Renderer::Flush();
            Renderer::EndScene();
        }
    }

    auto Scene::AddEmptyObject(std::string_view tagName, UInt64_T guid) -> Entity {
        Entity result{ m_Registry.create(), m_Registry };
        result.AddComponent<TagComponent>(tagName, guid);
        result.AddComponent<TransformComponent>(ENTITY_INITIAL_POSITION, ENTITY_INITIAL_SIZE, ENTITY_INITIAL_ROTATION);

        return result;
    }

    auto Scene::AddPrefabObject(std::string_view tagName, PrefabSceneObject type, UInt64_T guid) -> Entity {
        Entity result{ m_Registry.create(), m_Registry };

        // Setup tag and transform
        result.AddComponent<TagComponent>(tagName, guid);
        result.AddComponent<TransformComponent>(ENTITY_INITIAL_POSITION, ENTITY_INITIAL_SIZE, ENTITY_INITIAL_ROTATION);

        // Setup renderer component
        result.AddComponent<RenderComponent>();
        auto& renderData{ result.GetComponent<RenderComponent>() };
        renderData.GetObjectData().IsPrefab = true;
        renderData.GetObjectData().PrefabType = type;

        // Add a material for each one of the meshes of this model
        auto& model{ AssetsManager::GetModelPrefabByType(type) };

        for (auto& mesh : model.GetMeshes()) {
            // Emplace mesh metadata, mesh and material
            MeshMetaData meshMetaData{};
            meshMetaData.ModelMesh = std::addressof(mesh);

            // Diffuse map and specular map
            // By default we add and standard material
            {
                DefaultMaterialCreateSpec spec{};
                for (auto textureIt{ mesh.GetTextures().begin() }; textureIt != mesh.GetTextures().end(); ++textureIt) {
                    // we assume there is only one specular and one diffuse
                    if ((*textureIt)->GetType() == MapType::TEXTURE_2D_DIFFUSE) {
                        spec.DiffuseMap = *textureIt;
                    }

                    if ((*textureIt)->GetType() == MapType::TEXTURE_2D_SPECULAR) {
                        spec.SpecularMap = *textureIt;
                    }
                }

                meshMetaData.MeshMaterial = Material::CreateStandardMaterial(spec);
                renderData.GetObjectData().MeshMeta.push_back(std::move(meshMetaData));
            }
        }

        // Setup material component
        result.AddComponent<MaterialComponent>();
        auto& materialData{ result.GetComponent<MaterialComponent>() };

        return result;
    }

    auto Scene::DestroyEntity(Entity& entity) -> void {
        m_Registry.destroy(entity.m_EntityHandle);
        entity.Invalidate();
    }

    auto Scene::OnViewPortResize(UInt32_T width, UInt32_T height) -> void {
        m_ViewportWidth = width;
        m_ViewportHeight = height;

        // Resize non-fixed aspect ratio cameras
        auto view{ m_Registry.view<TransformComponent, CameraComponent>() };

        for (const auto& entity : view) {
            TransformComponent& transform{ view.get<TransformComponent>(entity) };
            CameraComponent& camera{ view.get<CameraComponent>(entity) };

            if (!camera.IsAspectRatioFixed())
                camera.GetCameraPtr()->SetViewportSize(width, height);

        }
    }

    auto Scene::OnEditorUpdate(double timeStep, const EditorCamera& camera) -> void {
        ScenePrepareData prepareData{};
        prepareData.StaticCamera = std::addressof(camera);

        Renderer::BeginScene(prepareData);


        // If the scene has no lights
        if (m_Registry.view<LightComponent>().empty()) {
            auto view{ m_Registry.view<TagComponent, TransformComponent, RenderComponent, MaterialComponent>() };

            for (auto& sceneObject : view) {
                TagComponent& tag{ view.get<TagComponent>(sceneObject) };
                TransformComponent& transform{ view.get<TransformComponent>(sceneObject) };
                RenderComponent& renderComponent{ view.get<RenderComponent>(sceneObject) };
                MaterialComponent& material{ view.get<MaterialComponent>(sceneObject) };

                SceneObjectData& objectData{ renderComponent.GetObjectData() };
                objectData.Color = material.GetColor();

                if (tag.IsVisible() && !objectData.MeshMeta.empty()) {
                    Renderer::Submit(objectData, transform.GetTransform());
                }
            }

            Renderer::SetActiveLightsCount(0);

            Renderer::Flush();
            Renderer::EndScene();
        }
        else {
            // Forward render lights.
            Size_T index{};
            Renderer::SetLightViewPos(glm::vec4{ camera.GetPosition(), 1.0f });

            for (auto& lightSource : m_Registry.view<LightComponent>()) {
                LightComponent& lightComponent{ m_Registry.get<LightComponent>(lightSource) };


                // Compute light affecting each game object
                auto view{ m_Registry.view<TagComponent, TransformComponent, RenderComponent, MaterialComponent>() };
                for (auto& sceneObject : view) {
                    TagComponent& tag{ view.get<TagComponent>(sceneObject) };
                    TransformComponent& transform{ view.get<TransformComponent>(sceneObject) };
                    RenderComponent& renderComponent{ view.get<RenderComponent>(sceneObject) };
                    MaterialComponent& material{ view.get<MaterialComponent>(sceneObject) };

                    SceneObjectData& objectData{ renderComponent.GetObjectData() };
                    objectData.Color = material.GetColor();

                    lightComponent.GetPointLightData().Position = glm::vec4{ m_Registry.get<TransformComponent>(lightSource).GetTranslation(), 1.0f };
                    Renderer::SetPointLightInfo( lightComponent.GetPointLightData(), index );

                    if (tag.IsVisible() && !objectData.MeshMeta.empty()) {
                        Renderer::Submit(objectData, transform.GetTransform());
                    }
                }

                Renderer::SetActiveLightsCount(++index);
            }

            Renderer::Flush();
            Renderer::EndScene();
        }
    }

    auto Scene::UpdateScripts() -> void {
        auto view{ m_Registry.view<TagComponent, TransformComponent, NativeScriptComponent>() };

        for (const auto& entity : view) {
            TagComponent& tag{ view.get<TagComponent>(entity) };
            TransformComponent& transform{ view.get<TransformComponent>(entity) };
            NativeScriptComponent& script{ view.get<NativeScriptComponent>(entity) };

            // Update scripts

        }
    }

    auto Scene::Clear() -> void {
        if (m_Registry.empty()) {
            return;
        }

        auto view{ m_Registry.view<TagComponent>() };
        m_Registry.destroy(view.begin(), view.end());
    }

    Scene::~Scene() {
        Clear();
    }

    auto Scene::GetActiveScene() -> Scene* {
        return s_ActiveScene;
    }

    auto Scene::SetActiveScene(Scene* scene) -> void {
        s_ActiveScene = scene;
    }

    auto Scene::FetchSceneMetaData() -> SceneMetaData& {
        // Game objects
        m_MetaData.EntityCount = m_Registry.view<TagComponent>().size();

        // Lighting
        auto viewLights{ m_Registry.view<LightComponent>() };
        m_MetaData.LightsCount = viewLights.size();

        for (auto& light : viewLights) {
            LightComponent& lightComponent{ m_Registry.get<LightComponent>(light) };
            // increment active lights


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
    }
}