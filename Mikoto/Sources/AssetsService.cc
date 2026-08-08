//    Copyright 2025 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <EASTL/unique_ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Profiler.hh>
#include <Core/Singleton.hh>

#include <Threading/TaskService.hh>

#include <Assets/Model.hh>
#include <Assets/MeshFactory.hh>
#include <Assets/AssetsService.hh>
#include <Assets/ImageProcessor.hh>

#include <Audio/AudioDevice.hh>
#include <Audio/AudioService.hh>

#include <Filesystem/FileService.hh>
#include <Filesystem/FileSystem.hh>
#include <Filesystem/FileWatcherService.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>

#include <Renderer/Core/FontFactory.hh>
#include <Renderer/Core/RenderSystem.hh>

namespace mikoto::asset {

    using namespace mikoto::core;
    using namespace mikoto::audio;
    using namespace mikoto::material;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;

    auto TextureLoadDescription::SetPath( const Path& path ) -> TextureLoadDescription& {
        mPath = path;
        return *this;
    }

    auto TextureLoadDescription::SetDimensions( TextureDimension dim ) -> TextureLoadDescription& {
        mDimension = dim;
        return *this;
    }

    AssetsService::AssetsService( const AssetsServiceDescription& ) {}

    auto AssetsService::Initialize() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_INFO( "Initializing AssetsService..." );

        mGpuDevice = RenderSystem::Get()->GetGpuDevice();
        mAudioDevice = AudioService::Get()->GetDevice();

        // We need to ensure animation folders exists
        // These are used to store the cached animations
        if ( CreateIfNotExistsDirectory( kAnimationCachePathBase ) ) {
            MKT_CORE_LOGGER_DEBUG( "Created directory to cache animation data. '{}'", kAnimationCachePathBase.GetC_Str() );
        }

        // Model importer library
        MeshFactoryCreateInfo meshFactoryCreateInfo{
            .mDevice = mGpuDevice,
        };

        mMeshFactory = eastl::make_unique<MeshFactory>( meshFactoryCreateInfo );
        if ( mMeshFactory ) {
            mMeshFactory->Initialize();
        }

        // Font factory
        FontFactoryCreateInfo fontFactoryCreateInfo{
            .mDevice = mGpuDevice,
        };

        mFontFactory = eastl::make_unique<FontFactory>( fontFactoryCreateInfo );
        if ( mFontFactory ) {
            mFontFactory->Initialize();
        }

        LoadDummyAssets();

        mIsInitialized = true;
    }

    auto AssetsService::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !mIsInitialized ) {
            return;
        }

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down AssetsService..." );

        mTextures2D.Clear();
        mTexturesCubes.Clear();
        mAudios.Clear();
        mFonts.Clear();
        mModels.Clear();

        mMeshFactory->Shutdown();
        mMeshFactory.reset();

        mFontFactory->Shutdown();
        mFontFactory.reset();

        mAudioDevice = nullptr;
        mGpuDevice = nullptr;

        mIsInitialized = false;
    }

    auto AssetsService::GetAssetCacheBasePath() const -> const Path& {
        return kAnimationCachePathBase;
    }

    auto AssetsService::GetDummyTexture() -> TextureHandle {
        return mTextures2D[kDummyTexturePath];
    }

    auto AssetsService::CreateMaterial( const PhysicMaterialDescription& desc ) -> MaterialHandle {
        // TODO: store materials in the materials default path or user specified one
        MaterialHandle material{ Ref<PhysicalMaterial>::Spawn( desc ) };
        if ( material.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "AssetsService::CreateMaterial - Failed to create material" );
        }

        return material;
    }
    auto AssetsService::CreateMaterial( const SkyboxMaterialDescription& desc ) -> MaterialHandle {
        MaterialHandle material{ Ref<SkyboxMaterial>::Spawn( desc ) };
        if ( material.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "AssetsService::CreateMaterial - Failed to create material" );
        }

        return material;
    }

    auto AssetsService::CreateMaterial( const PostProcessMaterialDescription& desc ) -> MaterialHandle {
        MaterialHandle material{ Ref<PostProcessMaterial>::Spawn( desc ) };
        if ( material.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "AssetsService::CreateMaterial - Failed to create material" );
        }

        return material;
    }

    auto AssetsService::LoadModel( const Path& uri ) -> ModelHandle {
        MKT_BEGIN_PROFILER_NAMED();

        const ModelLoadDescription modelLoadDescription{
            .mFile = FileService::Get()->LoadFile( uri ),
            .mExtractTextures = true,
        };

        return LoadModel( modelLoadDescription );
    }

    auto AssetsService::LoadModel( const ModelLoadDescription& description ) -> ModelHandle {
        MKT_BEGIN_PROFILER_NAMED();

        if ( description.mFile.IsEmpty() ) {
            return ModelHandle::CreateEmpty();
        }

        const Path& path{ description.mFile->GetPath().GetAbsolute() };

        CreateAssetCacheFolder( path );

        return mModels.LoadOrGet( path, [this, description, path]() -> ModelHandle {
            // This lambda runs ONLY once (per asset)
            ModelHandle model{
                MeshFactory::Get()->ImportModel( ModelLoadDescription{
                    .mFile = description.mFile,
                    .mApi = mGpuDevice->GetGraphicsApi(),
                    .mExtractTextures = description.mExtractTextures,
                } )
            };

            if ( !model.IsEmpty() ) {
                FileWatcherService::Get()->Watch( path,
                    []( const Path& pathCallable, FileWatchEvent event ) -> void {
                     if (event == FileWatchEvent::eModified) {
                         MKT_CORE_LOGGER_INFO( "Model file at [{}] has been updated", pathCallable.GetC_Str() );
                     } } );
            }

            return model;
        } );
    }

    auto AssetsService::LoadTexture( const Path& uri, TextureDimension dimension ) -> TextureHandle {
        auto desc{ TextureLoadDescription{}
           .SetPath( uri )
           .SetDimensions( dimension ) };

        return LoadTexture( desc );
    }

    auto AssetsService::LoadTexture( const TextureLoadDescription& description ) -> TextureHandle {
        MKT_BEGIN_PROFILER_NAMED();

        if ( description.mPath.IsEmpty() ) {
            return TextureHandle::CreateEmpty();
        }

        const Path& path{ description.mPath.GetAbsolute() };

        CreateAssetCacheFolder( path );

        if ( description.mDimension == TextureDimension::eTexture2D ) {
            return mTextures2D.LoadOrGet( path, [this, path]() -> TextureHandle {
                asset::ImageHandle image{ asset::ProcessImage2D( path ) };
                auto textureDescription{ TextureCreateDescription{}
                     .SetImageData( image )
                     .SetWidth( as<i32>( image->mWidth ) )
                     .SetHeight( as<i32>( image->mHeight ) )
                     .SetDimensions( TextureDimension::eTexture2D )
                     .SetMultisampling( Multisampling::eMsaaX1 )
                     .SetUsage( TextureUsageFlagsBits::kShaderResource | TextureUsageFlagsBits::kCopyDst )
                     .SetFormat( image->mFormat == ImageFormat::eRGBA8_UINT ? Format::eRGBA8_UNORM : Format::eRGBA32_FLOAT ) };

                TextureHandle texture{ mGpuDevice->CreateTexture( textureDescription ) };

                if ( !texture.IsEmpty() ) {
                    texture->SetDebugName( string::Format( "Texture: {}", path.GetC_Str() ) );

                    FileWatcherService::Get()->Watch( path,
                    []( const Path& pathCallable, FileWatchEvent event ) -> void {
                      if (event == FileWatchEvent::eModified) {
                          MKT_CORE_LOGGER_INFO( "Texture file at [{}] has been updated", pathCallable.GetC_Str() );
                      } } );
                }

                return texture;
            } );
        }

        return TextureHandle::CreateEmpty();
    }

    auto AssetsService::LoadAudio( const AudioLoadDescription& description ) -> AudioHandle {
        MKT_BEGIN_PROFILER_NAMED();

        if ( description.mFile.IsEmpty() ) {
            return AudioHandle::CreateEmpty();
        }

        const Path& path{ description.mFile->GetPath().GetAbsolute() };

        CreateAssetCacheFolder( path );

        return mAudios.LoadOrGet( path, [this, path, description]() -> AudioHandle {
            AudioHandle audio{ mAudioDevice->LoadAudio( description ) };

            if ( !audio.IsEmpty() ) {
                FileWatcherService::Get()->Watch( path,
                []( const Path& pathCallable, FileWatchEvent event ) -> void {
                  if (event == FileWatchEvent::eModified) {
                      MKT_CORE_LOGGER_INFO( "Audio file at [{}] has been updated", pathCallable.GetC_Str() );
                  } } );
            }

            return audio;
        } );
    }

    auto AssetsService::LoadFont( const Path& uri ) -> FontHandle {
        MKT_BEGIN_PROFILER_NAMED();

        const FontLoadDescription fontLoadDescription{
            .mFile = FileService::Get()->LoadFile( uri ),
            .mSize = 24.0f
        };

        return LoadFont( fontLoadDescription );
    }

    auto AssetsService::LoadFont( const FontLoadDescription& description ) -> FontHandle {
        MKT_BEGIN_PROFILER_NAMED();

        if ( description.mFile.IsEmpty() ) {
            return FontHandle::CreateEmpty();
        }

        const Path& path{ Path{ description.mFile->GetDirectory() }.GetAbsolute() };

        CreateAssetCacheFolder( path );

        return mFonts.LoadOrGet( path, [this, description, path, fontFile = description.mFile] {
            // This lambda runs ONLY once (per asset)
            auto fontDesc{ FontLoadDescription{}
                .SetFile( fontFile )
                .SetSize( description.mFile ) };

            FontHandle fontHandle{ mFontFactory->LoadFont( fontDesc ) };

            if ( !fontHandle.IsEmpty() ) {
                FileWatcherService::Get()->Watch( path,
                    []( const Path& pathCallable, FileWatchEvent event ) -> void {
                     if (event == FileWatchEvent::eModified) {
                         MKT_CORE_LOGGER_INFO( "Font file at [{}] has been updated", pathCallable.GetC_Str() );
                     } } );
            }

            return fontHandle;
        } );
    }

    auto AssetsService::LoadMaterial( const Path& uri ) -> MaterialHandle {
        return MaterialHandle::CreateEmpty();
    }

    auto AssetsService::CreateAssetCacheFolder( const Path& path ) -> void {
        const eastl::string cacheFolder{ string::Format( "{}/{}",
                                                         GetAssetCacheBasePath().GetC_Str(), GetHashedAssetID( path ) ) };

        if ( !filesystem::CreateIfNotExistsDirectory( cacheFolder ) ) {
            MKT_CORE_LOGGER_WARN( "Did not creache cache folder '{}'", cacheFolder );
        }
    }

    auto AssetsService::LoadDummyAssets() -> void {
        // Load the checkerboard
        ( void )LoadAsset<ITexture>( kDummyTexturePath, TextureDimension::eTexture2D );
    }
}// namespace mikoto::asset