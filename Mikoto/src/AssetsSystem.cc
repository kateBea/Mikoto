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

namespace Mikoto {

    auto AssetsSystem::Init( ) -> void {

    }

    auto AssetsSystem::Update( ) -> void {

    }

    auto AssetsSystem::Shutdown() -> void {
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

    auto AssetsSystem::GetTexture( const std::string_view uri) -> Texture* {
        const std::string key{ uri };
        if ( const auto it{ m_Textures.find( key ) }; it != m_Textures.end() ) {
            return it->second.get();
        }

        return nullptr;
    }

    auto AssetsSystem::LoadModel(const ModelLoadInfo& info) -> Model* {
        Model* result{ nullptr };

        // Ask file manager if this is a valid file

        if (!info.Path.is_absolute()) {
            MKT_CORE_LOGGER_ERROR( "AssetsSystem::LoadModel - Model path [{}] is not absolute!", info.Path.string() );
            return result;
        }

        auto itFind{ m_Models.find( info.Path.string() ) };
        if ( itFind == m_Models.end() ) {
            const std::string key{ info.Path.string() };
            auto [insertIt, insertSuccess]{ m_Models.try_emplace( key, CreateScope<Model>( info ) ) };

            if ( insertSuccess ) {
                result = insertIt->second.get();
                MKT_CORE_LOGGER_INFO( "AssetsSystem::LoadModel - Model [{}] loaded successfully!", info.Path.string() );
            } else {
                MKT_CORE_LOGGER_ERROR( "AssetsSystem::LoadModel - Model [{}] failed to load!", info.Path.string() );
            }
        } else {
            result = itFind->second.get();
            MKT_CORE_LOGGER_INFO( "AssetsSystem::LoadModel - Model [{}] already exists!", info.Path.string() );
        }

        return result;
    }

    auto AssetsSystem::LoadTexture(const TextureLoadInfo& info) -> Texture* {
        Texture* result{ nullptr };

        if (!info.Path.is_absolute()) {
            MKT_CORE_LOGGER_ERROR( "AssetsSystem::LoadTexture - Texture path [{}] is not absolute!", info.Path.string() );
            return result;
        }

        // TODO: Assumes texture 2D for now
        auto itFind{ m_Textures.find( info.Path.string() ) };
        if ( itFind == m_Textures.end() ) {
            const std::string key{ info.Path.string() };
            auto [insertIt, insertSuccess]{ m_Textures.try_emplace( key, std::move( Texture2D::Create( info.Path, info.Type ) ) ) };

            if ( insertSuccess ) {
                result = insertIt->second.get();
                MKT_CORE_LOGGER_INFO( "AssetsSystem::LoadTexture - Model [{}] loaded successfully!", info.Path.string() );
            } else {
                MKT_CORE_LOGGER_ERROR( "AssetsSystem::LoadTexture - Model [{}] failed to load!", info.Path.string() );
            }
        } else {
            result = itFind->second.get();
            MKT_CORE_LOGGER_INFO( "AssetsSystem::LoadTexture - Model [{}] already exists!", info.Path.string() );
        }

        return result;
    }

}