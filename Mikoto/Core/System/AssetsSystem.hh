//
// Created by zanet on 1/26/2025.
//

#ifndef ASSETSSYSTEM_HH
#define ASSETSSYSTEM_HH

#include <ft2build.h>
#include FT_FREETYPE_H

#include <Core/Engine.hh>
#include <Common/Common.hh>
#include <Library/Utility/Types.hh>
#include <Assets/Model.hh>
#include <Assets/Texture.hh>
#include <Assets/Font.hh>

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
        MKT_NODISCARD auto GetFont(std::string_view uri) -> Texture*;

        MKT_NODISCARD auto LoadModel(const ModelLoadInfo& info) -> Model*;
        MKT_NODISCARD auto LoadTexture(const TextureLoadInfo& info) -> Texture*;
        MKT_NODISCARD auto LoadFont(const FontLoadInfo& info) -> Font*;

        auto Shutdown() -> void override;
        auto Update() -> void override;

    private:

        static auto CreateTextureFromType( const TextureLoadInfo& info ) -> Texture*;

    private:
        std::unordered_map<std::string, Scope_T<Model>> m_Models{};
        std::unordered_map<std::string, Scope_T<Texture>> m_Textures{};
        std::unordered_map<std::string, Scope_T<Font>> m_Fonts{};

        FT_Library m_FreeTypeLibrary{};
    };

}



#endif //ASSETSSYSTEM_HH
