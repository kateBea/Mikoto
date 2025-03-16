//
// Created by zanet on 1/26/2025.
//

#include <Assets/Model.hh>
#include <Assets/Texture.hh>
#include <Common/Common.hh>
#include <Core/Logging/Assert.hh>
#include <Core/Logging/Logger.hh>
#include <Core/System/AssetsSystem.hh>
#include <Library/Utility/Types.hh>
#include <Material/Texture/TextureCubeMap.hh>
#include <filesystem>
#include <string>
#include <string_view>

#include "Renderer/Text/FreeTypeManager.hh"


namespace Mikoto {

    auto AssetsSystem::Init( ) -> void {
        FreeTypeManager::Init();

    }

    auto AssetsSystem::Update() -> void {
    }

    auto AssetsSystem::CreateTextureFromType( const TextureLoadInfo& info ) -> Texture* {

        switch ( info.Type ) {
            case MapType::TEXTURE_CUBE:
                return TextureCubeMap::Create( { .TexturePath{ info.Path } } ).release();
            case MapType::TEXTURE_2D_INVALID:
            case MapType::TEXTURE_2D_TEXT:
            case MapType::TEXTURE_2D_DIFFUSE:
            case MapType::TEXTURE_2D_SPECULAR:
            case MapType::TEXTURE_2D_EMISSIVE:
            case MapType::TEXTURE_2D_NORMAL:
            case MapType::TEXTURE_2D_ROUGHNESS:
            case MapType::TEXTURE_2D_METALLIC:
            case MapType::TEXTURE_2D_AMBIENT_OCCLUSION:
            case MapType::TEXTURE_2D_COUNT:
                return Texture2D::Create( info.Path, info.Type ).release();
        }

        return nullptr;
    }

    auto AssetsSystem::Shutdown() -> void {

        m_Fonts.clear();
        m_Models.clear();
        m_Textures.clear();

        // Shutdown library after all fonts are disposed of
        FreeTypeManager::Shutdown();
    }

    auto AssetsSystem::GetModel( const std::string_view uri) -> Model* {
        const std::string key{ uri };
        if ( const auto it{ m_Models.find( key ) }; it != m_Models.end() ) {
            return it->second.get();
        }

        return nullptr;

    }

    auto AssetsSystem::GetTexture( const std::string_view uri ) -> Texture* {
        const std::string key{ uri };
        if ( const auto it{ m_Textures.find( key ) }; it != m_Textures.end() ) {
            return it->second.get();
        }

        return nullptr;
    }

    auto AssetsSystem::GetFont( const std::string_view uri ) -> Font* {
        const std::string key{ uri };
        if ( const auto it{ m_Fonts.find( key ) }; it != m_Fonts.end() ) {
            return it->second.get();
        }

        return nullptr;
    }

    auto AssetsSystem::LoadFont( const FontLoadInfo& info ) -> Font* {
        Font* result{ nullptr };

        if (!info.Path.is_absolute()) {
            return result;
        }

        auto itFind{ m_Fonts.find( info.Path.string() ) };
        if ( itFind == m_Fonts.end() ) {
            const std::string key{ info.Path.string() };
            auto [insertIt, insertSuccess]{ m_Fonts.try_emplace( key, Font::Create( info ) ) };

            if ( insertSuccess ) {
                result = insertIt->second.get();
            }
        } else {
            result = itFind->second.get();
        }

        return result;
    }

    auto AssetsSystem::LoadModel(const ModelLoadInfo& info) -> Model* {
        Model* result{ nullptr };

        if (!info.Path.is_absolute()) {
            return result;
        }

        auto itFind{ m_Models.find( info.Path.string() ) };
        if ( itFind == m_Models.end() ) {
            const std::string key{ info.Path.string() };
            auto [insertIt, insertSuccess]{ m_Models.try_emplace( key, CreateScope<Model>( info ) ) };

            if ( insertSuccess ) {
                result = insertIt->second.get();
            }
        } else {
            result = itFind->second.get();
        }

        return result;
    }

    auto AssetsSystem::LoadTexture(const TextureLoadInfo& info) -> Texture* {
        Texture* result{ nullptr };

        if (!info.Path.is_absolute()) {
            return result;
        }

        auto itFind{ m_Textures.find( info.Path.string() ) };
        if ( itFind == m_Textures.end() ) {
            const std::string key{ info.Path.string() };

            Texture* createTextureResult{ CreateTextureFromType(info) };

            if (createTextureResult) {
                auto [insertIt, insertSuccess]{ m_Textures.try_emplace( key, createTextureResult ) };

                if ( insertSuccess ) {
                    result = insertIt->second.get();
                }
            }
        } else {
            result = itFind->second.get();
        }

        return result;
    }

}