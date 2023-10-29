//
// Created by kate on 10/8/23.
//

#include "entt/entt.hpp"
#include "fmt/format.h"

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
        auto it{ s_Scenes.find(std::string(name)) };

        const std::string sceneName{ name };
        Scene* newScenePtr{ nullptr };

        if (it == s_Scenes.end()) {
            // scene with given name is not loaded
            newScenePtr = std::addressof(s_Scenes.emplace(std::make_pair(sceneName, Scene{ sceneName })).first->second);
        }
        else {
            it->second.Clear();
        }

        return *newScenePtr;
    }

    auto SceneManager::IsSceneValidEntity(const Scene& scene, const Entity& entity) -> bool {
        return entity.BelongsTo(scene);
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

    auto SceneManager::DestroyEntityFromScene(Scene& scene, Entity& entity) -> void {
        scene.DestroyEntity(entity);
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

        // TODO: validate active scene, erase invalidates all iterators and pointers to elements of the container
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
            result = scene.AddEmptyObject(createInfo.Name);
        }
        else {
            result = scene.AddPrefabObject(createInfo.Name, createInfo.PrefabType);
        }

        return result;
    }

    auto SceneManager::DisableTargetEntity() -> void {
        s_CurrentlySelectedEntity.Invalidate();
    }
}