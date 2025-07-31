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
#include <FileSystem/FileService.hh>
#include <Library/Utility/Types.hh>
#include <Material/TextureCube.hh>
#include <Renderer/FontService.hh>
#include <Renderer/GpuDevice.hh>

namespace Mikoto {

    AssetsService::AssetsService( const AssetsServiceDescription& options )
        : m_GpuDevice{ options.Device }, m_AudioDevice{ options.AudDevice } {}

    auto AssetsService::Init() -> void {
        // Texture loaded
        m_AssetLoaders[typeid(Texture)] = [](AssetsService* self, void* desc, const Path_T& uri) -> RefAny {
            return self->LoadTextureAsset(*Cast<TextureLoadDescription*>(desc), uri);
        };
        m_AssetLoaders[typeid(Texture2D)] = [](AssetsService* self, void* desc, const Path_T& uri) -> RefAny {
            return self->LoadTextureAsset(*Cast<TextureLoadDescription*>(desc), uri);
        };
        m_AssetLoaders[typeid(TextureCube)] = [](AssetsService* self, void* desc, const Path_T& uri) -> RefAny {
            return self->LoadTextureAsset(*Cast<TextureLoadDescription*>(desc), uri);
        };

        // Model loader
        m_AssetLoaders[typeid(Model)] = [](AssetsService* self, void* desc, const Path_T& uri) -> RefAny {
            return self->LoadModelAsset(*Cast<ModelLoadDescription*>(desc), uri);
        };

        // Font loader
        m_AssetLoaders[typeid(Font)] = [](AssetsService* self, void* desc, const Path_T& uri) -> RefAny {
            return self->LoadFontAsset(*Cast<FontLoadDescription*>(desc), uri);
        };

        // Audio loader
        m_AssetLoaders[typeid(Audio)] = [](AssetsService* self, void* desc, const Path_T& uri) -> RefAny {
            return self->LoadAudioAsset(*Cast<AudioLoadDescription*>(desc), uri);
        };

        m_IsInitialized = true;
    }

    auto AssetsService::Shutdown() -> void {
        if ( !m_IsInitialized ) {
            return;
        }

        m_AssetLoaders.clear();
        m_LoadedAssets.clear();

        m_LoadTasks.clear();

        m_IsInitialized = false;
    }

    auto AssetsService::LoadModelAsset( const ModelLoadDescription& description, const Path_T& uri ) -> RefAny {
        const File* modelFile{ description.ModelFile ? description.ModelFile : FileService::GetInstance()->LoadFile( uri ) };

        if (!modelFile) {
            return {};
        }

        ModelHandle model{ MeshFactory::GetInstance()->CreateModel( ModelLoadDescription {
            .ModelFile{ modelFile },
            .WantTextures{ description.WantTextures }
        } ) };

        auto [it, success]{
            m_LoadedAssets.try_emplace( modelFile->GetPath(), model )
        };

        return m_LoadedAssets[modelFile->GetPath()];
    }

    auto AssetsService::LoadTextureAsset( const TextureLoadDescription& description, const Path_T& uri ) -> RefAny {
        const File* textureFile{ description.TextureFile ? description.TextureFile : FileService::GetInstance()->LoadFile( uri ) };

        if (!textureFile) {
            return {};
        }

        const StbImage image{ textureFile };

        if ( !image.IsValid() ) {
            return {};
        }

        TextureDescription textureDesc{};
        textureDesc
            .WithWidth( image.GetWidth() )
            .WithHeight( image.GetHeight() )
            .WithChannelCount( image.GetChannels() )

            .WithData( image.GetData() )

            .WithType( description.Type )
            .WithFormat( InferFormatFromChannels( image.GetChannels() ) )

            .WithResourceType( ResourceUsageType::RESOURCE_USAGE_STATIC );

        TextureHandle texture{ m_GpuDevice->CreateTexture( textureDesc ) };
        auto [it, success]{
            m_LoadedAssets.try_emplace( textureFile->GetPath(), texture )
        };

        return m_LoadedAssets[textureFile->GetPath()];
    }

    auto AssetsService::LoadAudioAsset( const AudioLoadDescription& description, const Path_T& uri ) -> RefAny {
        const File* audioFile{ description.AudioFile ? description.AudioFile : FileService::GetInstance()->LoadFile( uri ) };

        if (!audioFile) {
            return {};
        }

        AudioLoadDescription audioDesc{};
        audioDesc
                .WithFile( description.AudioFile )
                .SetVolume( 1.0f );

        AudioHandle handle{ m_AudioDevice->LoadAudio( audioDesc ) };

        // Store a reference to the audio to access via uri for asset service clients
        auto [it, success]{
            m_LoadedAssets.try_emplace( audioFile->GetPath(), handle )
        };

        return m_LoadedAssets[audioFile->GetPath()];
    }

    auto AssetsService::LoadFontAsset( const FontLoadDescription& description, const Path_T& uri ) -> RefAny {
        const File* fontFile{ description.FontFile ? description.FontFile : FileService::GetInstance()->LoadFile( uri ) };

        if (!fontFile) {
            return {};
        }

        FontLoadDescription fontDesc{};
        fontDesc
            .WithFile( fontFile )
            .WithPixelSize( 1.0f );

        FontHandle fontHandle{ FontService::GetInstance()->LoadFont( fontDesc ) };
        auto [it, success]{
            m_LoadedAssets.try_emplace( fontFile->GetPath(), fontHandle )
        };

        return m_LoadedAssets[fontFile->GetPath()];
    }
}