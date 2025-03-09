//
// Created by zanet on 1/26/2025.
//

#include <string>
#include <string_view>
#include <filesystem>

#include <Common/Common.hh>
#include <Core/Logging/Logger.hh>
#include <Core/Logging/Assert.hh>
#include <Library/Utility/Types.hh>
#include <Core/System/AssetsSystem.hh>
#include <Assets/Model.hh>
#include <Assets/Texture.hh>
#include <Material/Texture/TextureCubeMap.hh>


namespace Mikoto {

    auto AssetsSystem::Init( ) -> void {

        // Init Free Ttype Library
        const auto FTInit_Result{ FT_Init_FreeType(std::addressof( m_FreeTypeLibrary )) };

        if (FTInit_Result != 0) {
            MKT_CORE_LOGGER_ERROR( "ERROR::FREETYPE: Could not init FreeType Library");
            return;
        }

    }

    auto AssetsSystem::Update() -> void {
    }

    auto AssetsSystem::CreateTextureFromType( const TextureLoadInfo& info ) -> Texture* {

        switch ( info.Type ) {
            case MapType::TEXTURE_CUBE:
                return TextureCubeMap::Create( { .TexturePath{ info.Path } } ).release();
            default:
                return Texture2D::Create( info.Path, info.Type ).release();
        }

        return nullptr;
    }

    auto AssetsSystem::Shutdown() -> void {
        if (FT_Done_FreeType(m_FreeTypeLibrary) != 0) {
            MKT_CORE_LOGGER_ERROR( "AssetsSystem::Shutdown - Failed to destroy free type library" );
        }

        m_Models.clear();
        m_Textures.clear();
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