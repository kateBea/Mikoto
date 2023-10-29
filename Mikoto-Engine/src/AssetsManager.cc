/**
 * AssetsManager.cc
 * Created by kate on 10/15/23.
 * */

// Project Headers
#include <Threading/TaskManager.hh>

#include <Assets/AssetsManager.hh>

namespace Mikoto {

    auto AssetsManager::Init(AssetsManagerSpec&& spec) -> void {
        s_Spec = std::move(spec);

        LoadPrefabs();
    }

    auto AssetsManager::Shutdown() -> void {

    }

    auto AssetsManager::LoadPrefabs() -> void {
#if 0
        TaskManager::Execute([&]() -> void {
            AddSpritePrefab();
            s_LoadedPrefabModels.emplace(GetSponzaPrefabName(),   std::filesystem::absolute(s_Spec.AssetRootDirectory / "models/Prefabs/sponza/glTF/Sponza.gltf"));
            s_LoadedPrefabModels.emplace(GetCubePrefabName(),     std::filesystem::absolute(s_Spec.AssetRootDirectory / "models/Prefabs/cube/obj/cube.obj"));
            s_LoadedPrefabModels.emplace(GetSpherePrefabName(),   std::filesystem::absolute(s_Spec.AssetRootDirectory / "models/Prefabs/sphere/gltf/scene.gltf"));
            s_LoadedPrefabModels.emplace(GetCylinderPrefabName(), std::filesystem::absolute(s_Spec.AssetRootDirectory / "models/Prefabs/cylinder/gltf/scene.gltf"));
            s_LoadedPrefabModels.emplace(GetConePrefabName(),     std::filesystem::absolute(s_Spec.AssetRootDirectory / "models/Prefabs/cone/gltf/scene.gltf"));
        });
#endif
        AddSpritePrefab();
        s_LoadedPrefabModels.emplace(GetSponzaPrefabName(),   std::filesystem::absolute(s_Spec.AssetRootDirectory / "models/Prefabs/sponza/glTF/Sponza.gltf"));
        s_LoadedPrefabModels.emplace(GetCubePrefabName(),     std::filesystem::absolute(s_Spec.AssetRootDirectory / "models/Prefabs/cube/obj/cube.obj"));
        s_LoadedPrefabModels.emplace(GetSpherePrefabName(),   std::filesystem::absolute(s_Spec.AssetRootDirectory / "models/Prefabs/sphere/gltf/scene.gltf"));
        s_LoadedPrefabModels.emplace(GetCylinderPrefabName(), std::filesystem::absolute(s_Spec.AssetRootDirectory / "models/Prefabs/cylinder/gltf/scene.gltf"));
        s_LoadedPrefabModels.emplace(GetConePrefabName(),     std::filesystem::absolute(s_Spec.AssetRootDirectory / "models/Prefabs/cone/gltf/scene.gltf"));

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

            result.first->second.AddMesh(meshData);
        }

    }

    auto AssetsManager::GetModelPrefabByType(PrefabSceneObject type) -> const Model& {
        switch (type) {
            case PrefabSceneObject::SPRITE_PREFAB_OBJECT: return s_LoadedPrefabModels[GetSpritePrefabName()];
            case PrefabSceneObject::CUBE_PREFAB_OBJECT:   return s_LoadedPrefabModels[GetCubePrefabName()];
            case PrefabSceneObject::SPHERE_PREFAB_OBJECT: return s_LoadedPrefabModels[GetSpherePrefabName()];
            case PrefabSceneObject::CYLINDER_PREFAB_OBJECT: return s_LoadedPrefabModels[GetCylinderPrefabName()];
            case PrefabSceneObject::CONE_PREFAB_OBJECT: return s_LoadedPrefabModels[GetConePrefabName()];
            case PrefabSceneObject::SPONZA_PREFAB_OBJECT: return s_LoadedPrefabModels[GetSponzaPrefabName()];
            case PrefabSceneObject::COUNT_PREFAB_OBJECT:
                [[fallthrough]];
            case PrefabSceneObject::NO_PREFAB_OBJECT:
                MKT_CORE_LOGGER_WARN("Unknown prefab");
                break;
        }
    }
    auto AssetsManager::GetModel(const Path_T& modelPath) -> const Model * {
        std::string modelFullPath{ std::filesystem::absolute(modelPath).string() };

        auto it{ s_LoadedModels.find(modelFullPath) };

        if (it != s_LoadedModels.end()) {
            return std::addressof(it->second);
        }

        return nullptr;
    }

    auto AssetsManager::LoadModel(const Path_T& modelPath) -> void {
        std::string modelFullPath{ std::filesystem::absolute(modelPath).string() };

        if (!s_LoadedPrefabModels.contains(modelFullPath)) {
            s_LoadedModels.emplace(modelFullPath, modelPath);
        }
        else {
            MKT_CORE_LOGGER_INFO("Model [{}] already exists!", modelFullPath);
        }
    }
}
