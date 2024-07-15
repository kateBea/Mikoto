/**
 * AssetsManager.cc
 * Created by kate on 10/15/23.
 * */

// Project Headers
#include "../Assets/AssetsManager.hh"
#include "Renderer/Renderer.hh"

namespace Mikoto {

    auto AssetsManager::Init(AssetsManagerSpec&& spec) -> void {
        s_Spec = std::move(spec);

        LoadPrefabs();
    }

    auto AssetsManager::Shutdown() -> void {

    }

    auto AssetsManager::LoadPrefabs() -> void {
        AddSpritePrefab();

        ModelLoadInfo modelLoadInfo{};
        const bool invertedY{ Renderer::GetActiveGraphicsAPI() == GraphicsAPI::VULKAN_API };

        modelLoadInfo.InvertedY = invertedY;
        modelLoadInfo.WantTextures = true;

        modelLoadInfo.ModelPath = s_Spec.AssetRootDirectory / "models/Prefabs/sponza/glTF/Sponza.gltf";
        s_LoadedPrefabModels.emplace(GetSponzaPrefabName(), modelLoadInfo);

        modelLoadInfo.ModelPath = s_Spec.AssetRootDirectory / "models/Prefabs/cube/obj/cube.obj";
        s_LoadedPrefabModels.emplace(GetCubePrefabName(), modelLoadInfo);

        modelLoadInfo.ModelPath = s_Spec.AssetRootDirectory / "models/Prefabs/sphere/gltf/scene.gltf";
        s_LoadedPrefabModels.emplace(GetSpherePrefabName(), modelLoadInfo);

        modelLoadInfo.ModelPath = s_Spec.AssetRootDirectory / "models/Prefabs/cylinder/gltf/scene.gltf";
        s_LoadedPrefabModels.emplace(GetCylinderPrefabName(), modelLoadInfo);

        modelLoadInfo.ModelPath = s_Spec.AssetRootDirectory / "models/Prefabs/cone/gltf/scene.gltf";
        s_LoadedPrefabModels.emplace(GetConePrefabName(), modelLoadInfo);

    }

    auto AssetsManager::AddSpritePrefab() -> void {
        const std::vector<float> squareData {
            /* Positions    -       // Normals   -       // Colors    -       // Texture coordinates */
            -0.5f,  -0.5f, 0.0f,    0.0f, 0.0f, 0.0f,    1.0f, 0.0f, 0.0f,    0.0f, 0.0f,   // bottom left
            0.5f,  -0.5f, 0.0f,    0.0f, 0.0f, 0.0f,    0.0f, 1.0f, 0.0f,    1.0f, 0.0f,   // bottom right
            0.5f,   0.5f, 0.0f,    0.0f, 0.0f, 0.0f,    0.0f, 0.0f, 1.0f,    1.0f, 1.0f,   // top right
            -0.5f,   0.5f, 0.0f,    0.0f, 0.0f, 0.0f,    0.8f, 0.3f, 0.4f,    0.0f, 1.0f,   // top left
        };

        // Set index and vertex buffers
        auto vertexBuffer{ VertexBuffer::Create(squareData, VertexBuffer::GetDefaultBufferLayout()) };
        auto indexBuffer{ IndexBuffer::Create({0, 1, 2, 2, 3, 0}) };

        // Add model to the list of prefabs
        auto result{ s_LoadedPrefabModels.emplace(GetSpritePrefabName(), Model{}) };

        if (result.second) {
            MeshData meshData{};
            meshData.SetVertices(vertexBuffer);
            meshData.SetIndices(indexBuffer);

            result.first->second.AddMesh(std::move(meshData));
        }

    }

    auto AssetsManager::GetPrefabModel(PrefabSceneObject type) -> Model* {
        Model* result{ nullptr };
        auto it{ s_LoadedPrefabModels.end() };

        switch (type) {
            case PrefabSceneObject::SPRITE_PREFAB_OBJECT: it = s_LoadedPrefabModels.find(GetSpritePrefabName()); break;
            case PrefabSceneObject::CUBE_PREFAB_OBJECT:   it = s_LoadedPrefabModels.find(GetCubePrefabName()); break;
            case PrefabSceneObject::SPHERE_PREFAB_OBJECT: it = s_LoadedPrefabModels.find(GetSpherePrefabName()); break;
            case PrefabSceneObject::CYLINDER_PREFAB_OBJECT: it = s_LoadedPrefabModels.find(GetCylinderPrefabName()); break;
            case PrefabSceneObject::CONE_PREFAB_OBJECT: it = s_LoadedPrefabModels.find(GetConePrefabName()); break;
            case PrefabSceneObject::SPONZA_PREFAB_OBJECT: it = s_LoadedPrefabModels.find(GetSponzaPrefabName()); break;

            case PrefabSceneObject::COUNT_PREFAB_OBJECT:
            case PrefabSceneObject::NO_PREFAB_OBJECT:
                it = s_LoadedPrefabModels.end();
                break;
        }

        if (it != s_LoadedPrefabModels.end()) {
            result = std::addressof(it->second);
        }

        return result;
    }


    auto AssetsManager::GetModel(const Path_T& modelPath) -> const Model * {
        auto modelFullPath{ std::filesystem::absolute(modelPath) };

        // The key to the model will be its full path converted to a string
        auto key{ modelFullPath.string() };

        auto it{ s_LoadedModels.find(key) };

        if ( it != s_LoadedModels.end() ) {
            return std::addressof(it->second);
        }

        return nullptr;
    }

    auto AssetsManager::GetModifiableModel(const Path_T& modelPath) -> Model * {
        auto modelFullPath{ std::filesystem::absolute(modelPath) };

        // The key to the model will be its full path converted to a string
        auto key{ modelFullPath.string() };

        auto it{ s_LoadedModels.find(key) };

        if ( it != s_LoadedModels.end() ) {
            return std::addressof(it->second);
        }

        return nullptr;
    }

    auto AssetsManager::LoadModel(const ModelLoadInfo& info) -> Model * {
        Model* result{ nullptr };
        ModelLoadInfo modelLoadInfo{ info };
        modelLoadInfo.ModelPath = std::filesystem::absolute(info.ModelPath);

        if (!s_LoadedPrefabModels.contains(modelLoadInfo.ModelPath.string())) {
            auto key{ modelLoadInfo.ModelPath.string() };
            result = std::addressof(s_LoadedModels.emplace(key, modelLoadInfo).first->second);
        }
        else {
            MKT_CORE_LOGGER_INFO("Model [{}] already exists!", modelLoadInfo.ModelPath.string());
        }

        return result;
    }
}
