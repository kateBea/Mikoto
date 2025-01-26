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
#include <Models/Enums.hh>

#include "Assets/Model.hh"
#include "Common/Common.hh"
#include "Common/RenderingUtils.hh"

namespace Mikoto {
    struct AssetsManagerSpec {
        Path_T AssetRootDirectory{};
    };

    class AssetsManager final {
    public:
        static auto Init(AssetsManagerSpec&& spec) -> void;
        static auto Shutdown() -> void;

        MKT_NODISCARD static auto GetModel(std::string_view uri) -> Model*;
        MKT_NODISCARD static auto GetTexture(std::string_view uri) -> Texture2D*;

        MKT_NODISCARD static auto LoadModel(const ModelLoadInfo& info) -> Model*;
        MKT_NODISCARD static auto LoadTexture(const TextureLoadInfo& info) -> Texture2D*;

    private:

        static auto LoadPrefabs() -> void;

        static auto AddSpritePrefab() -> void;

        MKT_NODISCARD static auto GetSpritePrefabName() -> const std::string& { static std::string name{ "Sprite" }; return name; }

        MKT_NODISCARD static auto GetCubePrefabName() -> const std::string& { static std::string name{ "Cube" }; return name; }

        MKT_NODISCARD static auto GetSpherePrefabName() -> const std::string& { static std::string name{ "Sphere" }; return name; }

        MKT_NODISCARD static auto GetCylinderPrefabName() -> const std::string& { static std::string name{ "Cylinder" }; return name; }

        MKT_NODISCARD static auto GetConePrefabName() -> const std::string& { static std::string name{ "Cone" }; return name; }

        MKT_NODISCARD static auto GetSponzaPrefabName() -> const std::string& { static std::string name{ "Sponza" }; return name; }

    private:
        static inline AssetsManagerSpec s_Spec{};
        static inline std::unordered_map<std::string, Scope_T<Model>> s_Models{};
        static inline std::unordered_map<std::string, Scope_T<Model>> s_Textures{};

    };
}

#endif // MIKOTO_ASSETS_MANAGER_HH
