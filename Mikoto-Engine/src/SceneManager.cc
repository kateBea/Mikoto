//
// Created by kate on 10/8/23.
//

#include "entt/entt.hpp"
#include "fmt/format.h"

#include <Renderer/Material/StandardMaterial.hh>
#include "ConsoleManager.hh"
#include "Scene/SceneManager.hh"

namespace Mikoto {
    auto SceneManager::Init() -> void {
        // Add an empty scene on which we can start working
        // They usually come with various game objects already set
        // directional light (sunlight sort of), a simple mesh and a scene camera
        auto& newScene{ MakeNewScene("Empty Scene") };
        SetActiveScene(newScene);
    }

    auto SceneManager::MakeNewScene(std::string_view name) -> Scene& {
        const std::string sceneName{ name };

        auto it{ s_Scenes.find(sceneName) };

        if (it != s_Scenes.end()) {
            MKT_CORE_LOGGER_INFO("Scene with name {} already existed!", name);
            s_Scenes.erase(it);
        }

        Scene& newScene{ s_Scenes.emplace(sceneName, Scene{ sceneName }).first->second };

        // setup new scene
        EntityCreateInfo entityCreateInfo{};
        entityCreateInfo.Name = "Ground";
        entityCreateInfo.IsPrefab = true;
        entityCreateInfo.PrefabType = PrefabSceneObject::CUBE_PREFAB_OBJECT;

        // Ground
        auto result{ AddEntityToScene(newScene, entityCreateInfo) };
        TransformComponent& transformComponent{ result.GetComponent<TransformComponent>() };
        transformComponent.SetScale( { 10.0f, 0.2f, 10.0f } );
        transformComponent.SetTranslation( { 0.0f, -5.0f, 1.0f } );

        // Sphere
        entityCreateInfo.Name = "Sphere";
        entityCreateInfo.IsPrefab = true;
        entityCreateInfo.PrefabType = PrefabSceneObject::SPHERE_PREFAB_OBJECT;
        auto sphere{ AddEntityToScene(newScene, entityCreateInfo) };
        TransformComponent& sphereTransformComponent{ sphere.GetComponent<TransformComponent>() };
        sphereTransformComponent.SetScale( { 0.2f, 0.2f, 0.2f } );
        sphereTransformComponent.SetTranslation( { 0.0f, 0.0f, 8.0f } );

        auto& sphereRenderData{ sphere.GetComponent<RenderComponent>().GetObjectData() };

        for (auto& mesh : sphereRenderData.ObjectModel->GetMeshes()) {
            mesh.GetMaterialList().clear();

            PBRMaterialCreateSpec spec{};
            for (auto& texture : mesh.GetTextures()) {
                switch ( texture->GetType() ) {
                    case MapType::TEXTURE_2D_DIFFUSE:
                        spec.AlbedoMap = texture;
                        break;

                    case MapType::TEXTURE_2D_NORMAL:
                        spec.NormalMap = texture;
                        break;

                    case MapType::TEXTURE_2D_ROUGHNESS:
                        spec.RoughnessMap = texture;
                        break;

                    case MapType::TEXTURE_2D_METALLIC:
                        spec.MetallicMap = texture;
                        break;

                    case MapType::TEXTURE_2D_AMBIENT_OCCLUSION:
                        spec.AlbedoMap = texture;
                        break;

                    case MapType::TEXTURE_2D_SPECULAR:
                    case MapType::TEXTURE_2D_EMISSIVE:
                        // Unused for now
                        break;

                    case MapType::TEXTURE_2D_COUNT:
                    case MapType::TEXTURE_2D_INVALID:
                        MKT_CORE_LOGGER_WARN("Unknown type of texture");
                        break;
                }
            }

            mesh.AddMaterial(Material::CreatePBRMaterial(spec));
        }


        // directional light
        entityCreateInfo.Name = "Point light";
        entityCreateInfo.IsPrefab = false;
        entityCreateInfo.PrefabType = PrefabSceneObject::NO_PREFAB_OBJECT;
        auto directionalLight{ AddEntityToScene(newScene, entityCreateInfo) };
        auto light{ directionalLight.AddComponent<LightComponent>() };
        light.SetType(LightType::POINT_LIGHT_TYPE);


        // scene camera
        entityCreateInfo.Name = "Camera";
        entityCreateInfo.IsPrefab = false;
        entityCreateInfo.PrefabType = PrefabSceneObject::NO_PREFAB_OBJECT;
        auto mainCam{ AddEntityToScene(newScene, entityCreateInfo) };
        mainCam.AddComponent<CameraComponent>(std::make_shared<SceneCamera>());

        return newScene;
    }

    auto SceneManager::IsEntitySelected() -> bool {
        return s_CurrentlySelectedEntity.IsValid();
    }

    auto SceneManager::GetCurrentlySelectedEntity() -> Entity& {
        return s_CurrentlySelectedEntity;
    }

    auto SceneManager::SetCurrentlyActiveEntity(const Entity& entity) -> void {
        s_CurrentlySelectedEntity = Entity{ entity.Get(), GetActiveScene().GetRegistry() };
    }

    auto SceneManager::DisableCurrentlyActiveEntity() -> void {
        s_CurrentlySelectedEntity.Invalidate();
    }

    auto SceneManager::GetActiveScene() -> Scene& {
        return *Scene::GetActiveScene();
    }

    auto SceneManager::DestroyEntityFromCurrentlyActiveScene(Entity &entity) -> void {
        GetActiveScene().DestroyEntity(entity);
    }

    auto SceneManager::AddEntity(const EntityCreateInfo &createInfo) -> Entity {
        return AddEntityToScene(GetActiveScene(), createInfo);
    }

    auto SceneManager::Shutdown() -> void {
        Scene::SetActiveScene(nullptr);

        s_Scenes.clear();
    }

    auto SceneManager::DestroyScene(const Scene& scene) -> void {
        auto it{ s_Scenes.find(scene.GetName()) };

        if (it != s_Scenes.end()) {
            s_Scenes.erase(it);
        }
    }

    auto SceneManager::DestroyActiveScene() -> void {
        DestroyScene(GetActiveScene());

        Scene::SetActiveScene(nullptr);
    }

    auto SceneManager::SetActiveScene(Scene& scene) -> void {
        Scene::SetActiveScene(std::addressof(scene));
    }

    auto SceneManager::AddEntityToScene(Scene& scene, const EntityCreateInfo& createInfo) -> Entity {
        Entity result{};

        ConsoleManager::PushMessage(ConsoleLogLevel::CONSOLE_INFO, fmt::format("Adding new game object '{}' to scene '{}'",
                                                                       createInfo.Name,
                                                                       scene.GetName()));

        if (!createInfo.IsPrefab) {
            result = scene.CreateEmptyObject( createInfo.Name );
        }
        else {
            result = scene.CreatePrefab( createInfo.Name, createInfo.PrefabType );
        }

        return result;
    }

    auto SceneManager::DisableTargetedEntity() -> void {
        s_CurrentlySelectedEntity.Invalidate();
    }

    auto SceneManager::GetCurrentSelection() -> std::optional<std::reference_wrapper<Entity>> {
        if (s_CurrentlySelectedEntity.IsValid()) {
            return { s_CurrentlySelectedEntity };
        }

        return {};
    }
}