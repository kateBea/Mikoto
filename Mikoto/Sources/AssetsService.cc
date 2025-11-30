//
// Created by zanet on 1/26/2025.
//
#include <ankerl/unordered_dense.h>

#include <Renderer/Core/FontFactory.hh>
#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Core/RenderService.hh>
#include <Assets/AssetsService.hh>
#include <Assets/MeshFactory.hh>
#include <Assets/Model.hh>
#include <Assets/Texture.hh>
#include <Audio/AudioDevice.hh>
#include <Common/Common.hh>
#include <Core/Profiler.hh>
#include <Filesystem/FileService.hh>
#include <Library/Utility/Types.hh>
#include <Threading/TaskService.hh>

namespace Mikoto {

    AssetsService::AssetsService( const AssetsServiceDescription& options )
        : m_GpuDevice{ options.Device }, m_AudioDevice{ options.AudDevice } {}

    auto AssetsService::Init() -> void {
        MKT_BEGIN_PROFILER_NAMED();

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
        FontFactoryCreateInfo fontFactoryCreateInfo{
            .Device{ m_GpuDevice },
        };

        m_FontFactory = CreateScope<FontFactory>( fontFactoryCreateInfo );
        if (m_FontFactory) {
            m_FontFactory->Init();
        }

        LoadDummyAssets();

        m_PBRMaterialsPool.Init( 10 );

        m_IsInitialized = true;
    }

    auto AssetsService::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !m_IsInitialized ) {
            return;
        }

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down AssetsService..." );

        m_PBRMaterialsPool.Shutdown();

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
        return m_Textures[s_DummyTexturePath.data() ];
    }

    auto AssetsService::CreateMaterial() -> MaterialHandle {
        MaterialHandle material{ m_PBRMaterialsPool.Allocate() };
        if ( material.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "AssetsService::CreateMaterial - Failed to create material" );
        }

        return material;
    }

    auto AssetsService::LoadModel( const std::string_view uri ) -> ModelHandle {
        MKT_BEGIN_PROFILER_NAMED();

        const ModelLoadDescription modelLoadDescription{
            .ModelFile{ FileService::Get()->LoadFile( uri ) },
            .WantTextures{ true }
        };

        return LoadModel( modelLoadDescription );
    }

    auto AssetsService::LoadModel( const ModelLoadDescription& description ) -> ModelHandle {
        MKT_BEGIN_PROFILER_NAMED();

        const File* modelFile{ description.ModelFile };
        if ( !modelFile ) {
            return ModelHandle::CreateEmpty();
        }

        // if it exists
        if ( const auto itFind{ m_Models.find( modelFile->GetPath() ) }; itFind != m_Models.end() ) {
            return itFind->second;
        }

        ModelHandle model{ MeshFactory::Get()->ImportModel( ModelLoadDescription{
                .ModelFile{ modelFile },
                .WantTextures{ description.WantTextures } } ) };

        if ( !model.IsEmpty() ) {
            auto [it, success]{
                m_Models.try_emplace( modelFile->GetPath(), model )
            };

            if ( success ) {
                return m_Models[modelFile->GetPath()];
            }
        }

        return ModelHandle::CreateEmpty();
    }

    auto AssetsService::LoadTexture( const Path& uri ) -> TextureHandle {
        MKT_BEGIN_PROFILER_NAMED();

        const std::string uriString{ uri.string() };
        return LoadTexture( std::string_view{ uriString } );
    }

    auto AssetsService::LoadTexture( std::string_view uri ) -> TextureHandle {
        MKT_BEGIN_PROFILER_NAMED();

        TextureLoadDescription loadDesc{};
        loadDesc.WithFile( FileService::Get()->LoadFile( uri ) )
                .WithType( TextureType::TEXTURE_2D );

        return LoadTexture( loadDesc );
    }

    auto AssetsService::LoadTexture( const TextureLoadDescription& description) -> TextureHandle {
        MKT_BEGIN_PROFILER_NAMED();

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

    auto AssetsService::LoadAudio( const AudioLoadDescription& description) -> AudioHandle {
        MKT_BEGIN_PROFILER_NAMED();

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

    auto AssetsService::LoadFont(const Path& uri) -> FontHandle {
        MKT_BEGIN_PROFILER_NAMED();

        const std::string uriString{ uri.string() };

        return LoadFont( std::string_view{ uriString } );
    }

    auto AssetsService::LoadFont( const std::string_view uri) -> FontHandle {
        MKT_BEGIN_PROFILER_NAMED();

        const FontLoadDescription fontLoadDescription{
            .FontFile{ FileService::Get()->LoadFile( uri ) },
            .FontSize{ 24.0f }
        };

        return LoadFont( fontLoadDescription );
    }

    auto AssetsService::LoadFont( const FontLoadDescription& description ) -> FontHandle {
        MKT_BEGIN_PROFILER_NAMED();

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
                .WithPixelSize( description.FontSize );

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

    auto AssetsService::LoadMaterial( std::string_view uri ) -> MaterialHandle {
        // TODO: implement logic of loading the material in memory
        return CreateMaterial();
    }

    auto AssetsService::LoadMaterial( const Path& uri ) -> MaterialHandle {
        // TODO: implement logic of loading the material in memory
        return CreateMaterial();
    }

    auto AssetsService::LoadDummyAssets() -> void {

        TextureLoadDescription loadDesc{};
        loadDesc
                .WithFile( FileService::Get()->LoadFile( s_DummyTexturePath ) )
                .WithType( TextureType::TEXTURE_2D );

        LoadAsset<Texture>( loadDesc );
    }
}