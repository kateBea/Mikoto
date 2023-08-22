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
#include <Renderer/Renderer.hh>
#include <Renderer/RenderingUtilities.hh>

namespace Mikoto {

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
                    mainCam = camera.GetCameraPtr();

                    // The camera's position and rotation depends on its transform component
                    mainCam->SetPosition(transform.GetTranslation(), transform.GetRotation());
                    break;
                }
            }
        }

        if (sceneHasMainCam) {
            // Render stuff if the scene has a camera
            ScenePrepareData prepareData{};
            prepareData.SceneCamera = mainCam;

            // TODO: add filter Renderable and deprecate SpriteRendererComponent
            auto view{ m_Registry.view<TagComponent, TransformComponent, SpriteRendererComponent>() };

            Renderer::BeginScene(prepareData);

            for (const auto& entity : view) {
                TagComponent& tag{ view.get<TagComponent>(entity) };
                TransformComponent& transform{ view.get<TransformComponent>(entity) };
                SpriteRendererComponent& sprite{ view.get<SpriteRendererComponent>(entity) };
                DrawData& drawData{ sprite.GetDrawData() };

                // TODO: fix rendering order for blending, objects that are nearer to the camera should be rendered first
                // Right now if an object is on top of another but it is rendered after before blending does not work
                Renderer::SubmitQuad(transform.GetTransform(), sprite.GetColor(), drawData.MaterialData);
            }

            Renderer::EndScene();
        }
    }

    auto Scene::CreateEntity(std::string_view tagName, const std::shared_ptr<Scene>& scene) -> Entity {
        Entity result{ scene };

        // By default, all entities will have a transform component and a tag
        result.AddComponent<TagComponent>(tagName);
        result.AddComponent<TransformComponent>(glm::vec3{ 0.0, 0.0, 0.0 },  glm::vec3{ 1.0f, 1.0f, 0.0f }, glm::vec3{ 0.0f, 0.0f, 0.0f });

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
}