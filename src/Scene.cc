//
// Created by kate on 6/24/23.
//

#include <utility>

#include <entt/entt.hpp>

#include <Renderer/Renderer.hh>

#include "Scene/Scene.hh"
#include <Scene/Component.hh>
#include <Scene/Entity.hh>

namespace kaTe {

    Scene::Scene() {
        entt::entity entity{ m_Registry.create() };
        m_Registry.emplace<TransformComponent>(entity, TransformComponent());
    }

    auto Scene::OnUpdate() -> void {
        // Update scripts
        {

        }

        std::shared_ptr<SceneCamera> mainCam{};
        bool sceneHasMainCam{ false };
        {
            auto view{ m_Registry.view<TransformComponent, CameraComponent>() };

            for (const auto& entity : view) {
                TransformComponent& transform{ view.get<TransformComponent>(entity) };
                CameraComponent& camera{ view.get<CameraComponent>(entity) };
                sceneHasMainCam = camera.IsMainCamera();

                if (sceneHasMainCam) {
                    mainCam = std::move(camera.GetCameraPtr());

                    // The camera's position and rotation depends on its transform component
                    mainCam->SetPosition(transform.GetTranslation(), transform.GetRotation());
                    break;
                }
            }
        }

        if (sceneHasMainCam) {
            // Render stuff if the scene has a camera
            Renderer::BeginScene(mainCam);

            auto view{ m_Registry.view<TagComponent, TransformComponent, SpriteRendererComponent>() };

            for (const auto& entity : view) {
                TagComponent& tag{ view.get<TagComponent>(entity) };
                TransformComponent& transform{ view.get<TransformComponent>(entity) };
                SpriteRendererComponent& sprite{ view.get<SpriteRendererComponent>(entity) };

                // TODO: fix rendering order for blending, objects that are nearer to the camera should be rendered first
                // Right if an object is on top of another but it is rendered after before blending does not work
                Renderer::SubmitQuad(transform.GetTransform(), sprite.GetColor());
            }
            Renderer::EndScene();
        }
    }

    auto Scene::CreateEntity(std::string_view entityNameTag, std::shared_ptr<Scene> scene) -> Entity {
        Entity result{ std::move(scene) };

        // By default, all entities will have a transform component and a tag
        result.AddComponent<TagComponent>(entityNameTag);
        result.AddComponent<TransformComponent>(glm::vec3{ 0.0, 0.0, 0.0 },  glm::vec3{ 1.0f, 1.0f, 0.0f }, glm::vec3{ 0.0f, 0.0f, 0.0f });

        return result;
    }

    auto Scene::DestroyEntity(Entity& entity) -> void {
        // Cast necessary to call appropriate overload
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
}