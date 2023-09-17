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
#include <Scene/Scene.hh>
#include <Scene/Entity.hh>
#include <Scene/Component.hh>
#include <Scene/EditorCamera.hh>
#include <Renderer/Renderer.hh>
#include <Renderer/RenderingUtilities.hh>

namespace Mikoto {
    auto Scene::OnUpdate(double ts) -> void {
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

                SceneObjectData& objectData{renderComponent.GetObjectData() };
                objectData.Color = material.GetColor();

                // TODO: fix rendering order for blending, objects that are nearer to the camera should be rendered first
                // Right now if an object is on top of another but it is rendered after before blending does not work
                Renderer::Submit(objectData, transform.GetTransform(), material.Get());
            }

            Renderer::Flush();
            Renderer::EndScene();
        }
    }

    auto Scene::CreateEmptyObject(std::string_view tagName, const std::shared_ptr<Scene>& scene) -> Entity {
        Entity result{ scene };
        result.AddComponent<TagComponent>(tagName);
        result.AddComponent<TransformComponent>(ENTITY_INITIAL_POSITION, ENTITY_INITIAL_SIZE, ENTITY_INITIAL_ROTATION);

        return result;
    }

    auto Scene::CreatePrefabObject(std::string_view tagName, const std::shared_ptr<Scene>& scene, PrefabSceneObject type) -> Entity {
        Entity result{ scene };

        result.AddComponent<RenderComponent>();
        auto& renderData{ result.GetComponent<RenderComponent>() };
        renderData.GetObjectData().IsPrefab = true;
        renderData.GetObjectData().PrefabType = type;

        result.AddComponent<MaterialComponent>();
        auto& materialData{ result.GetComponent<MaterialComponent>() };
        materialData.SetMaterial(Material::Create(Material::Type::STANDARD));

        result.AddComponent<TagComponent>(tagName);
        result.AddComponent<TransformComponent>(ENTITY_INITIAL_POSITION, ENTITY_INITIAL_SIZE, ENTITY_INITIAL_ROTATION);

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

    auto Scene::OnEditorUpdate(double timeStep, const EditorCamera &camera) -> void {
        ScenePrepareData prepareData{};
        prepareData.StaticCamera = &camera;

        Renderer::BeginScene(prepareData);

        auto view{ m_Registry.view<TagComponent, TransformComponent, RenderComponent, MaterialComponent>() };

        for (auto& sceneObject : view) {
            TransformComponent& transform{ view.get<TransformComponent>(sceneObject) };
            RenderComponent& renderComponent{ view.get<RenderComponent>(sceneObject) };
            MaterialComponent& material{ view.get<MaterialComponent>(sceneObject) };

            SceneObjectData& objectData{ renderComponent.GetObjectData() };
            objectData.Color = material.GetColor();

            // TODO: fix rendering order for blending, objects that are nearer to the camera should be rendered first
            // Right now if an object is on top of another but it is rendered after before blending does not work
            Renderer::Submit(objectData, transform.GetTransform(), material.Get());
        }

        Renderer::Flush();
        Renderer::EndScene();
    }

    auto Scene::UpdateScripts() -> void {

    }
}