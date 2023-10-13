/**
 * Scene.cc
 * Created by kate on 6/24/23.
 * */

// C++ Standard Library
#include <memory>
#include <utility>

// Third-Party Libraries
#include <entt/entt.hpp>

// Project Headers
#include <Utility/Random.hh>
#include <Scene/Entity.hh>
#include <Scene/Scene.hh>
#include <Scene/Component.hh>
#include <Renderer/Renderer.hh>
#include <Renderer/RenderingUtilities.hh>
#include <Scene/Camera/EditorCamera.hh>

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

                Renderer::Submit(objectData, transform.GetTransform(), material.Get());
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

        result.AddComponent<TagComponent>(tagName, guid);
        result.AddComponent<TransformComponent>(ENTITY_INITIAL_POSITION, ENTITY_INITIAL_SIZE, ENTITY_INITIAL_ROTATION);

        result.AddComponent<RenderComponent>();
        auto& renderData{ result.GetComponent<RenderComponent>() };
        renderData.GetObjectData().IsPrefab = true;
        renderData.GetObjectData().PrefabType = type;

        result.AddComponent<MaterialComponent>();
        auto& materialData{ result.GetComponent<MaterialComponent>() };
        materialData.SetMaterial(Material::Create(Material::Type::STANDARD));

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

            if (tag.IsVisible()) {
                Renderer::Submit(objectData, transform.GetTransform(), material.Get());
            }
        }

        Renderer::Flush();
        Renderer::EndScene();
    }

    auto Scene::UpdateScripts() -> void {

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