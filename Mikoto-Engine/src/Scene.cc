/**
 * Scene.cc
 * Created by kate on 6/24/23.
 * */

// C++ Standard Library
#include <memory>
#include <utility>
#include <algorithm>

// Third-Party Libraries
#include "entt/entt.hpp"

// Project Headers
#include "../Common/Random.hh"

#include <Assets/AssetsManager.hh>

#include "EditorCamera.hh"
#include "Scene/Component.hh"
#include "Scene/Entity.hh"
#include "Scene/Scene.hh"

#include "Common/RenderingUtils.hh"
#include "Renderer/Renderer.hh"

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

            auto it{ std::find_if(mesh.GetTextures().begin(), mesh.GetTextures().end(),
                                  [](const std::shared_ptr<Texture2D>& texture) -> bool { return texture->GetType() == MapType::TEXTURE_2D_DIFFUSE; }) };

            if (it != mesh.GetTextures().end()) {
                // If this mesh has got a diffuse map we can use it with the standard material
                // otherwise we have to apply a base material which only allowes to change base aspects like the color
                DefaultMaterialCreateSpec spec{};
                spec.DiffuseMap = it != mesh.GetTextures().end() ? *it : nullptr;

                meshMetaData.MeshMaterial = Material::CreateStandardMaterial(spec);
                renderData.GetObjectData().MeshMeta.push_back(std::move(meshMetaData));
            }
            else {
                // Apply base material
                meshMetaData.MeshMaterial = Material::CreateColoredMaterial();
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

        Renderer::Flush();
        Renderer::EndScene();
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
}