/**
 * AssetsManager.cc
 * Created by kate on 10/15/23.
 * */

#ifndef MIKOTO_ASSETS_MANAGER_HH
#define MIKOTO_ASSETS_MANAGER_HH

// C++ Standard Library
#include <string>
#include <unordered_map>

// Project Headers
#include "Common/Common.hh"
#include "Common/RenderingUtils.hh"
#include "Common/Types.hh"
#include "Renderer/Model.hh"

namespace Mikoto {
    struct AssetsManagerSpec {
        Path_T AssetRootDirectory{};
    };

    class AssetsManager {
    public:
        static auto Init(AssetsManagerSpec&& spec) -> void;
        static auto Shutdown() -> void;

        MKT_NODISCARD static auto GetAssetRootDirectory() -> const Path_T& { return s_Spec.AssetRootDirectory; }

        /**
         * @brief Retrieves the model prefab corresponding to a specific type.
         * @param type The type of prefab scene object.
         * @returns Const reference to the corresponding ModelPrefab.
         * */
        MKT_NODISCARD static auto GetModelPrefabByType(PrefabSceneObject type) -> const Model&;

        MKT_NODISCARD static auto GetModifiableModelPrefabByType(PrefabSceneObject type) -> Model*;

        /**
         * @brief Retrieves the model by the given path and name.
         *
         * This method returns a constant reference to a Model object based on the provided path and name. If the model does not exist
         * with the given path and name, this method returns a null pointer.
         *
         * @param modelPath The path to the model.
         * @return A pointer to the Model object found.
         * */
        MKT_NODISCARD static auto GetModel(const Path_T &modelPath) -> const Model *;

        auto static GetModifiableModel(const Path_T& modelPath) -> Model *;

        /**
         * @brief Loads a model from the provided path.
         *
         * This method loads and processes a model from the specified path. It handles loading the model's data and storing it within
         * the assets manager, enabling access to the model throughout the application.
         *
         * @param modelPath The path to the model to load.
         * */
        static auto LoadModel(const ModelLoadInfo& modelPath) -> void;


    private:
        /**
         * @brief Loads prefab models for rendering.
         * */
        static auto LoadPrefabs() -> void;

        /**
         * @brief Adds a sprite prefab to the Renderer's list of prefabs.
         * */
        static auto AddSpritePrefab() -> void;

        /**
         * @brief Retrieves the name of the sprite prefab.
         * @returns Reference to the constant string containing the sprite prefab name.
         * */
        MKT_NODISCARD static auto GetSpritePrefabName() -> const std::string& { static std::string name{ "Sprite" }; return name; }

        /**
         * @brief Retrieves the name of the cube prefab.
         * @returns Reference to the constant string containing the cube prefab name.
         * */
        MKT_NODISCARD static auto GetCubePrefabName() -> const std::string& { static std::string name{ "Cube" }; return name; }

        /**
         * @brief Retrieves the name of the sphere prefab.
         * @returns Reference to the constant string containing the sphere prefab name.
         * */
        MKT_NODISCARD static auto GetSpherePrefabName() -> const std::string& { static std::string name{ "Sphere" }; return name; }

        /**
         * @brief Retrieves the name of the cylinder prefab.
         * @returns Reference to the constant string containing the cylinder prefab name.
         * */
        MKT_NODISCARD static auto GetCylinderPrefabName() -> const std::string& { static std::string name{ "Cylinder" }; return name; }

        /**
         * @brief Retrieves the name of the cone prefab.
         * @returns Reference to the constant string containing the cone prefab name.
         * */
        MKT_NODISCARD static auto GetConePrefabName() -> const std::string& { static std::string name{ "Cone" }; return name; }

        /**
         * @brief Retrieves the name of the Sponza prefab.
         * @returns Reference to the constant string containing the Sponza prefab name.
         * */
        MKT_NODISCARD static auto GetSponzaPrefabName() -> const std::string& { static std::string name{ "Sponza" }; return name; }

    private:
        static inline AssetsManagerSpec                      s_Spec{};               /**< Assets manager specification. */
        static inline std::unordered_map<std::string, Model> s_LoadedPrefabModels{}; /**< List of loaded models. Includes prefabs which are loaded at initialization */
        static inline std::unordered_map<std::string, Model> s_LoadedModels{};       /**< List of arbitrary models loaded by the user */

    };
}

#endif // MIKOTO_ASSETS_MANAGER_HH
