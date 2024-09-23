//
// Created by kate on 10/8/23.
//

#include "entt/entt.hpp"
#include "fmt/format.h"

#include "Scene/SceneManager.hh"
#include <Assets/AssetsManager.hh>

namespace Mikoto {
    auto SceneManager::Init() -> void {

    }

    auto SceneManager::MakeNewScene(std::string_view name) -> Scene& {
        const std::string sceneName{ name };
        EntityCreateInfo entityCreateInfo{};

        auto it{ s_Scenes.find(sceneName) };
        if (it != s_Scenes.end()) {
            MKT_CORE_LOGGER_INFO("Scene with name {} already existed!", name);
            return it->second;
        }

        Scene& newScene{ s_Scenes.emplace(sceneName, Scene{ sceneName }).first->second };

        // Ground
        entityCreateInfo.Name = "Ground";
        entityCreateInfo.PrefabType = PrefabSceneObject::CUBE_PREFAB_OBJECT;
        auto result{ AddEntityToScene(newScene, entityCreateInfo) };
        TransformComponent& transformComponent{ result.GetComponent<TransformComponent>() };
        transformComponent.SetScale( { 10.0f, 0.2f, 10.0f } );
        transformComponent.SetTranslation( { 0.0f, -5.0f, 1.0f } );


        // Point light
        entityCreateInfo.Name = "Light";
        entityCreateInfo.PrefabType = PrefabSceneObject::NO_PREFAB_OBJECT;
        auto directionalLight{ AddEntityToScene(newScene, entityCreateInfo) };
        auto light{ directionalLight.AddComponent<LightComponent>() };
        light.SetType(LightType::POINT_LIGHT_TYPE);


        // Scene camera
        entityCreateInfo.Name = "Camera";
        entityCreateInfo.PrefabType = PrefabSceneObject::NO_PREFAB_OBJECT;
        auto mainCam{ AddEntityToScene(newScene, entityCreateInfo) };
        mainCam.AddComponent<CameraComponent>(std::make_shared<SceneCamera>());

        return newScene;
    }

    auto SceneManager::SetCurrentSelection(const Entity& entity) -> void {
        s_TargetEntity = Entity{entity.Get(), GetActiveScene().GetRegistry() };
    }

    auto SceneManager::DisableActiveSelection() -> void {
        s_TargetEntity.Invalidate();
    }

    auto SceneManager::GetActiveScene() -> Scene& {
        return *Scene::GetActiveScene();
    }

    auto SceneManager::DestroyEntity(Entity &target) -> void {
        DeleteEntityFromScene(GetActiveScene(), target);
    }

    auto SceneManager::DeleteEntityFromScene(Scene &scene, Entity& target) -> bool {
        return scene.DestroyEntity(target);
    }

    auto SceneManager::AddEntity(const EntityCreateInfo &createInfo) -> Entity {
        return AddEntityToScene(GetActiveScene(), createInfo);
    }

    auto SceneManager::Shutdown() -> void {
        DisableActiveSelection();
        Scene::SetActiveScene(nullptr);

        s_Scenes.clear();
    }

    auto SceneManager::DestroyScene(const Scene& scene) -> void {
        auto it{ s_Scenes.find(scene.GetName()) };

        if (it != s_Scenes.end()) {
            s_Scenes.erase(it);
        }
#if !defined(NDEBUG)
        else {
            MKT_CORE_LOGGER_INFO("Scene with name {} does not exist", scene.GetName());
        }
#endif
    }


    auto SceneManager::SetActiveScene(Scene& scene) -> void {
        Scene::SetActiveScene(std::addressof(scene));
    }

    auto SceneManager::AddEntityToScene(Scene& scene, const EntityCreateInfo& createInfo) -> Entity {
        Entity result{};

        if (!createInfo.IsPrefab()) {
            result = scene.CreateEmptyObject(createInfo.Name, createInfo.Root);
        }
        else {
            result = scene.CreatePrefab(createInfo.Name, AssetsManager::GetPrefabModel(createInfo.PrefabType), createInfo.Root);
        }

        return result;
    }


    auto SceneManager::GetCurrentSelection() -> EntityOpt_T {
        if (s_TargetEntity.IsValid()) {
            return EntityOpt_T{ s_TargetEntity };
        }

        return EntityOpt_T{};
    }
}