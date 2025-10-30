//
// Created by zanet on 1/26/2025.
//
#include <ankerl/unordered_dense.h>

#include <Assets/AssetsService.hh>
#include <Assets/MeshFactory.hh>
#include <Assets/Model.hh>
#include <Assets/Texture.hh>
#include <Audio/AudioDevice.hh>
#include <Common/Common.hh>
#include <Filesystem/FileService.hh>
#include <Library/Utility/Types.hh>
#include <Material/TextureCube.hh>
#include <Renderer/FontFactory.hh>
#include <Renderer/GpuDevice.hh>
#include <Renderer/RenderService.hh>
#include <Threading/TaskService.hh>

namespace Mikoto {

    AssetsService::AssetsService( const AssetsServiceDescription& options )
        : m_GpuDevice{ options.Device }, m_AudioDevice{ options.AudDevice } {}

    auto AssetsService::Init() -> void {
        MKT_CORE_LOGGER_INFO("Initializing AssetsService...");

        // Model importer library
        MeshFactoryCreateInfo meshFactoryCreateInfo{
            .ImportersCount{ 5 },
            .UseCustomLogger{ true },
            .Device{ m_GpuDevice },
        };

        m_MeshFactory = CreateScope<MeshFactory>( meshFactoryCreateInfo );
        if (m_MeshFactory) {
            m_MeshFactory->Init();
        }

        // Font factory
        FontFactoryCreateInfo fontFactoryCreateInfo{};

        m_FontFactory = CreateScope<FontFactory>( fontFactoryCreateInfo );
        if (m_FontFactory) {
            m_FontFactory->Init();
        }

        LoadDummyAssets();

        m_IsInitialized = true;
    }

    auto AssetsService::Shutdown() -> void {
        if ( !m_IsInitialized ) {
            return;
        }

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down AssetsService..." );

        m_Textures.clear();
        m_Audios.clear();
        m_Fonts.clear();
        m_Models.clear();

        m_MeshFactory->Shutdown();
        m_MeshFactory.reset();

        m_FontFactory->Shutdown();
        m_FontFactory.reset();

        m_AudioDevice = nullptr;
        m_GpuDevice = nullptr;

        m_IsInitialized = false;
    }

    auto AssetsService::GetDummyTexture() -> TextureHandle {
        return m_Textures["./texture"];
    }

    auto AssetsService::LoadAssetTyped( const ModelLoadDescription& description) -> ModelHandle {
        const File* modelFile{ description.ModelFile };
        if (!modelFile) {
            return ModelHandle::CreateEmpty();
        }

        // if it exists
        if (const auto itFind{ m_Models.find( modelFile->GetPath() ) }; itFind != m_Models.end() ) {
            return itFind->second;
        }

        ModelHandle model{ MeshFactory::Get()->ImportModel( ModelLoadDescription {
            .ModelFile{ modelFile },
            .WantTextures{ description.WantTextures }
        } ) };

        if (!model.IsEmpty()) {
            auto [it, success]{
                m_Models.try_emplace( modelFile->GetPath(), model )
            };

            if (success) {
                return m_Models[modelFile->GetPath()];
            }
        }

        return ModelHandle::CreateEmpty();
    }

    auto AssetsService::LoadAssetTyped( const TextureLoadDescription& description) -> TextureHandle {
        const File* textureFile{ description.TextureFile  };
        if (!textureFile) {
            return TextureHandle::CreateEmpty();
        }

        // if it exists
        if (const auto itFind{ m_Textures.find( textureFile->GetPath() ) }; itFind != m_Textures.end() ) {
            return itFind->second;
        }

        const StbImage image{ textureFile };

        if ( !image.IsValid() ) {
            return TextureHandle::CreateEmpty();
        }

        TextureDescription textureDesc{};
        textureDesc.WithWidth( image.GetWidth() )
            .WithHeight( image.GetHeight() )
            .WithChannelCount( image.GetChannels() )

            .WithData( image.GetData() )
            .WithFile( textureFile )

            .WithType( description.Type )
            .WithFormat( TextureFormat::TEXTURE_FORMAT_SRGB8_ALPHA8 )

            .WithResourceType( ResourceUsageType::RESOURCE_USAGE_STATIC );

        TextureHandle texture{ m_GpuDevice->CreateTexture( textureDesc ) };
        if (!texture.IsEmpty()) {
            auto [it, success]{
                m_Textures.try_emplace( textureFile->GetPath(), texture )
            };

            if (success) {
                return m_Textures[textureFile->GetPath()];
            }
        }

        return TextureHandle::CreateEmpty();
    }

    auto AssetsService::LoadAssetTyped( const AudioLoadDescription& description) -> AudioHandle {
        const File* audioFile{ description.AudioFile };
        if (!audioFile) {
            return AudioHandle::CreateEmpty();
        }

        // if it exists
        if (const auto itFind{ m_Audios.find( audioFile->GetPath() ) }; itFind != m_Audios.end() ) {
            return itFind->second;
        }

        AudioLoadDescription audioDesc{};
        audioDesc.WithFile( description.AudioFile )
                .SetVolume( 1.0f );

        AudioHandle handle{ m_AudioDevice->LoadAudio( audioDesc ) };
        if (!handle.IsEmpty()) {
            auto [it, success]{
                m_Audios.try_emplace( audioFile->GetPath(), handle )
            };

            if (success) {
                return m_Audios[audioFile->GetPath()];
            }
        }

        return AudioHandle::CreateEmpty();
    }

    auto AssetsService::LoadAssetTyped( const FontLoadDescription& description ) -> FontHandle {
        const File* fontFile{ description.FontFile };
        if ( !fontFile ) {
            return FontHandle::CreateEmpty();
        }

        // if it exists
        if ( const auto itFind{ m_Fonts.find( fontFile->GetPath() ) }; itFind != m_Fonts.end() ) {
            return itFind->second;
        }

        FontLoadDescription fontDesc{};
        fontDesc.WithFile( fontFile )
                .WithPixelSize( 1.0f );

        FontHandle fontHandle{ FontFactory::Get()->LoadFont( fontDesc ) };
        if ( !fontHandle.IsEmpty() ) {
            auto [it, success]{
                m_Fonts.try_emplace( fontFile->GetPath(), fontHandle )
            };

            if ( success ) {
                return m_Fonts[fontFile->GetPath()];
            }
        }

        return FontHandle::CreateEmpty();
    }

    auto AssetsService::LoadDummyAssets() -> void {
        TextureLoadDescription loadDesc{};
        loadDesc
                .WithFile( FileService::Get()->LoadFile( "./texture.png" ) )
                .WithType( TextureType::TEXTURE_2D );

        LoadAsset<Texture>( loadDesc );
    }
}// namespace Mikoto