//
// Created by zanet on 1/26/2025.
//

#ifndef ASSETSSYSTEM_HH
#define ASSETSSYSTEM_HH

#include <Core/Engine.hh>
#include <Common/Common.hh>
#include <Library/Utility/Types.hh>
#include <Assets/Model.hh>
#include <Assets/Texture.hh>

namespace Mikoto {
    class AssetsSystem final : public IEngineSystem {
    public:
        explicit AssetsSystem() = default;
        explicit AssetsSystem(const EngineConfig& options) {

        }

        ~AssetsSystem() override = default;

        auto Init() -> void override;

        MKT_NODISCARD auto GetModel(std::string_view uri) -> Model*;
        MKT_NODISCARD auto GetTexture(std::string_view uri) -> Texture*;

        MKT_NODISCARD auto LoadModel(const ModelLoadInfo& info) -> Model*;
        MKT_NODISCARD auto LoadTexture(const TextureLoadInfo& info) -> Texture*;

        auto Shutdown() -> void override;
        auto Update() -> void override;

    private:
        std::unordered_map<std::string, Scope_T<Model>> m_Models{};
        std::unordered_map<std::string, Scope_T<Texture>> m_Textures{};
    };

}



#endif //ASSETSSYSTEM_HH
