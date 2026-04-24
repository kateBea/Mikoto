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

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Profiler.hh>
#include <Core/Singleton.hh>

#include <Renderer/Core/Rhi.hh>

#include <Assets/AssetsService.hh>
#include <Assets/MeshFactory.hh>

#include <Assets/Model.hh>

#include <Audio/AudioDevice.hh>
#include <Audio/AudioService.hh>

#include <Threading/TaskService.hh>

#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Core/FontFactory.hh>
#include <Renderer/Core/RenderSystem.hh>

#include <Filesystem/FileService.hh>
#include <Filesystem/FileSystem.hh>
#include <Filesystem/FileWatcherService.hh>

namespace mikoto::asset {

     AssetsService::AssetsService( const AssetsServiceDescription& )
         {}

     auto AssetsService::Initialize() -> void {
         MKT_BEGIN_PROFILER_NAMED();

         MKT_CORE_LOGGER_INFO("Initializing AssetsService...");

         mGpuDevice = RenderSystem::Get()->GetGpuDevice();
         mAudioDevice = AudioService::Get()->GetDevice();

         // We need to ensure animation folders exists
         // These are used to store the cached animations
         if (CreateIfNotExistsDirectory( mAnimationCachePathBase ) ) {
             MKT_CORE_LOGGER_DEBUG( "Created directory to cache animation data. '{}'", mAnimationCachePathBase.GetC_Str() );
         }

         // Model importer library
         MeshFactoryCreateInfo meshFactoryCreateInfo{
             .mDevice = mGpuDevice,
         };

         mMeshFactory = eastl::make_unique<MeshFactory>( meshFactoryCreateInfo );
         if (mMeshFactory) {
             mMeshFactory->Initialize();
         }

         // Font factory
         FontFactoryCreateInfo fontFactoryCreateInfo{
             .mDevice = mGpuDevice,
         };

         mFontFactory = eastl::make_unique<FontFactory>( fontFactoryCreateInfo );
         if (mFontFactory) {
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
         return mAnimationCachePathBase;
     }

     auto AssetsService::GetDummyTexture() -> TextureHandle {
         return mTextures2D[ kDummyTexturePath ];
     }

     auto AssetsService::CreateMaterial( const MaterialProperties& props ) -> MaterialHandle {
         MaterialHandle material{ Ref<PhysicalMaterial>::Spawn( props ) };
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

         if (description.mFile.IsEmpty()) {
             return ModelHandle::CreateEmpty();
         }

         const Path& path{ description.mFile->GetPath().GetAbsolute() };

         CreateAssetCacheFolder(path);

         return mModels.LoadOrGet(path, [this, description, path]() -> ModelHandle {
             // This lambda runs ONLY once (per asset)
             ModelHandle model{
                 MeshFactory::Get()->ImportModel(ModelLoadDescription{
                     .mFile = description.mFile,
                     .mApi = mGpuDevice->GetGraphicsApi(),
                     .mExtractTextures = description.mExtractTextures,
                 })
             };

             if (!model.IsEmpty()) {
                 FileWatcherService::Get()->Watch( path,
                     [](const Path& pathCallable, FileWatchEvent event) -> void {
                         if (event == FileWatchEvent::eModified) {
                             MKT_CORE_LOGGER_INFO( "Model file at [{}] has been updated", pathCallable.GetC_Str() );
                         } });
             }

             return model;
         });
     }

     auto AssetsService::LoadTexture( const Path& uri, TextureDimension dimension ) -> TextureHandle {
        return TextureHandle::CreateEmpty();
     }

     auto AssetsService::LoadTexture( const TextureLoadDescription& desc ) -> TextureHandle {
         return TextureHandle::CreateEmpty();
     }

     auto AssetsService::LoadAudio( const AudioLoadDescription& description ) -> AudioHandle {
         return AudioHandle::CreateEmpty();

     }

     auto AssetsService::LoadFont( const Path& uri ) -> FontHandle {
         return FontHandle::CreateEmpty();
     }

     auto AssetsService::LoadFont( const FontLoadDescription& description ) -> FontHandle {
         return FontHandle::CreateEmpty();
     }

     auto AssetsService::LoadMaterial( const Path& uri ) -> MaterialHandle {
         return MaterialHandle::CreateEmpty();
     }

     auto AssetsService::CreateAssetCacheFolder( const Path& path ) -> void {
         const eastl::string cacheFolder{ string::Format(  "{}/{}",
             GetAssetCacheBasePath().GetC_Str(), GetHashedAssetID( path ) ) };

         if (!filesystem::CreateIfNotExistsDirectory( cacheFolder ) ) {
             MKT_CORE_LOGGER_WARN( "Did not creache cache folder '{}'", cacheFolder );
         }
     }
//
//     auto AssetsService::LoadModel( const std::string_view uri ) -> ModelHandle {
//         MKT_BEGIN_PROFILER_NAMED();
//
//         const ModelLoadDescription modelLoadDescription{
//             .ModelFile{ FileService::Get()->LoadFile( uri ) },
//             .WantTextures{ true }
//         };
//
//         return LoadModel( modelLoadDescription );
//     }
//
//     auto AssetsService::LoadModel( const ModelLoadDescription& description ) -> ModelHandle {
//         MKT_BEGIN_PROFILER_NAMED();
//
//         const File* modelFile{ description.ModelFile };
//         if ( !modelFile ) {
//             return ModelHandle::CreateEmpty();
//         }
//
//         CreateAssetCacheFolder( modelFile->GetPath() );
//
//         // if it exists
//         if ( const auto itFind{ m_Models.find( modelFile->GetPath() ) }; itFind != m_Models.end() ) {
//             return itFind->second;
//         }
//
//         ModelHandle model{ MeshFactory::Get()->ImportModel( ModelLoadDescription{
//                 .ModelFile{ modelFile },
//                 .WantTextures{ description.WantTextures },
//                 .TargetAPI{ m_GpuDevice->GetApi() },
//
//         } ) };
//
//         if ( !model.IsEmpty() ) {
//
//             m_ModelLoadMutex.lock();
//             auto [it, success]{
//                 m_Models.try_emplace( modelFile->GetPath(), model )
//             };
//             m_ModelLoadMutex.unlock();
//
//             // TODO: Handle when a model gets updated from disk and reload it at engine side
//             FileWatcherService::Get()->Watch(
//                     modelFile->GetPath(),
//                     []( const Path& pathCallable, FileWatchEvent event ) mutable -> void {
//                         if ( event == FileWatchEvent::MODIFIED ) {
//                             MKT_CORE_LOGGER_INFO( "Model file at [{}] has been updated", pathCallable.string() );
//                         }
//                     } );
//
//             if ( success ) {
//                 return m_Models[modelFile->GetPath()];
//             }
//         }
//
//         return ModelHandle::CreateEmpty();
//     }
//
//     auto AssetsService::LoadTexture( const Path& uri, bool isHDR ) -> TextureHandle {
//         MKT_BEGIN_PROFILER_NAMED();
//
//         const std::string uriString{ uri.string() };
//         return LoadTexture( std::string_view{ uriString }, isHDR );
//     }
//
//     auto AssetsService::LoadTexture( std::string_view uri, bool isHDR ) -> TextureHandle {
//         MKT_BEGIN_PROFILER_NAMED();
//
//         TextureLoadDescription loadDesc{};
//         loadDesc.WithFile( FileService::Get()->LoadFile( uri ) )
//                 .IsHDRMap( isHDR )
//                 // Use proper format for HDR images, currently using the default which is rgb8 unorm
//                 .WithType( TextureType::TEXTURE_2D );
//
//         return LoadTexture( loadDesc );
//     }
//
//     auto AssetsService::LoadTexture( const TextureDescription& description ) -> TextureHandle {
//         std::string path{ description.TextureFile ? description.TextureFile->GetPath() : description.Name };
//
//         CreateAssetCacheFolder( path );
//
//         TextureHandle texture{};
//
//         try {
//            texture = m_GpuDevice->CreateTexture( description );
//         } catch (std::exception& e) {
//             MKT_CORE_LOGGER_ERROR( "Failed to load 2D texture. e.what(): {}", e.what() );
//         }
//
//         if (!texture.IsEmpty()) {
//             std::lock_guard lock{ m_Texture2DPoolMutex };
//
//             auto [it, success]{
//                 m_Textures2D.try_emplace( path, texture )
//             };
//
//             if (success) {
//                 texture = m_Textures2D[path];
//             }
//         }
//
//         return texture;
//     }
//
//     auto AssetsService::LoadTexture( const TextureLoadDescription& description) -> TextureHandle {
//         MKT_BEGIN_PROFILER_NAMED();
//
//         const File* textureFile{ description.TextureFile  };
//         if (!textureFile) {
//             return TextureHandle::CreateEmpty();
//         }
//
//         // if it exists
//         if (const auto itFind{ m_Textures2D.find( textureFile->GetPath() ) }; itFind != m_Textures2D.end() ) {
//             return itFind->second;
//         }
//
//         const ImageLoader2D image{ textureFile };
//
//         if ( !image.IsValid() ) {
//             return TextureHandle::CreateEmpty();
//         }
//
//         TextureDescription textureDesc{};
//         textureDesc.WithWidth( image.GetWidth() )
//             .WithHeight( image.GetHeight() )
//             .WithChannelCount( image.GetChannels() )
//
//             .WithData( image.GetData() )
//             .WithFile( textureFile )
//
//             .IsHDRMap( description.IsHDR )
//
//             .WithMapType( description.Map )
//
//             .WithType( description.Type )
//             .WithFormat( description.Format )
//
//             .WithResourceType( ResourceUsageType::RESOURCE_USAGE_STATIC );
//
//         return LoadTexture( textureDesc );
//     }
//
//     auto AssetsService::LoadCubeMap( const Path &uri ) -> TextureHandle {
//         MKT_BEGIN_PROFILER_NAMED();
//
//         TextureCubeLoadDescription desc{ .BasePath{ uri } };
//
//         return LoadCubeMap(desc);
//     }
//
//     auto AssetsService::LoadCubeMap( const TextureCubeLoadDescription &description ) -> TextureHandle {
//         MKT_BEGIN_PROFILER_NAMED();
//
//         // Return cached texture
//         const std::string baseAbsolute{ filesystem::GetGetAbsolutePathString( description.BasePath ) };
//         if (const auto itFind{ m_TexturesCubes.find( baseAbsolute ) }; itFind != m_TexturesCubes.end() ) {
//             return itFind->second;
//         }
//
//         TextureHandle texture{ TextureHandle::CreateEmpty() };
//
//         TextureCubeCreateDescription textureDesc{};
//         textureDesc
//             .LoadLDR( description.WantLDR )
//             .WithResourceUsage( description.ResourceUsage );
//
//         if (textureDesc.IsFacesSplit) {
//             for (const auto& path : description.FacesRelativePaths ) {
//                 PathBuilder pathBuilder{};
//                 pathBuilder.WithPath( baseAbsolute )
//                     .WithPath( path.string() );
//
//                 if (const File* file{ FileService::Get()->LoadFile( pathBuilder.Build() ) }) {
//                     textureDesc.WithFacePath( file );
//                 }
//             }
//         } else {
//
//             if (StringUtil::Contains( baseAbsolute, ".ktx" )) {
//                 textureDesc.UseCubeImageLoader = true;
//             }
//
//             if (const File* file{ FileService::Get()->LoadFile( baseAbsolute ) }) {
//                 textureDesc.WithFacePath( file );
//             }
//         }
//
//         CreateAssetCacheFolder( description.BasePath );
//
//         try {
//             texture = m_GpuDevice->CreateTexture( textureDesc );
//         } catch (std::exception& e) {
//             MKT_CORE_LOGGER_ERROR( "Failed to load cube texture. e.what(): {}", e.what() );
//         }
//
//         if (!texture.IsEmpty()) {
//             std::lock_guard lock{ m_Texture2DPoolMutex };
//
//             auto [it, success]{
//                 m_TexturesCubes.try_emplace( baseAbsolute, texture )
//             };
//
//             if (success) {
//                 texture = m_TexturesCubes[baseAbsolute];
//             }
//         }
//
//         return texture;
//     }
//
//     auto AssetsService::LoadAudio( const AudioLoadDescription& description) -> AudioHandle {
//         MKT_BEGIN_PROFILER_NAMED();
//
//         const File* audioFile{ description.AudioFile };
//         if (!audioFile) {
//             return AudioHandle::CreateEmpty();
//         }
//
//         if (const auto itFind{ m_Audios.find( audioFile->GetPath() ) }; itFind != m_Audios.end() ) {
//             return itFind->second;
//         }
//
//         AudioLoadDescription audioDesc{};
//         audioDesc.WithFile( description.AudioFile )
//                 .SetVolume( 1.0f );
//
//         AudioHandle handle{ m_AudioDevice->LoadAudio( audioDesc ) };
//         if (!handle.IsEmpty()) {
//             auto [it, success]{
//                 m_Audios.try_emplace( audioFile->GetPath(), handle )
//             };
//
//             if (success) {
//                 return m_Audios[audioFile->GetPath()];
//             }
//         }
//
//         return AudioHandle::CreateEmpty();
//     }
//
//     auto AssetsService::LoadFont(const Path& uri) -> FontHandle {
//         MKT_BEGIN_PROFILER_NAMED();
//
//         const std::string uriString{ uri.string() };
//
//         return LoadFont( std::string_view{ uriString } );
//     }
//
//     auto AssetsService::LoadFont( const std::string_view uri) -> FontHandle {
//         MKT_BEGIN_PROFILER_NAMED();
//
//         const FontLoadDescription fontLoadDescription{
//             .FontFile{ FileService::Get()->LoadFile( uri ) },
//             .FontSize{ 24.0f }
//         };
//
//         return LoadFont( fontLoadDescription );
//     }
//
//     auto AssetsService::LoadFont( const FontLoadDescription& description ) -> FontHandle {
//         MKT_BEGIN_PROFILER_NAMED();
//
//         const File* fontFile{ description.FontFile };
//         if ( !fontFile ) {
//             return FontHandle::CreateEmpty();
//         }
//
//         CreateAssetCacheFolder( fontFile->GetPath() );
//
//         // if it exists
//         if ( const auto itFind{ m_Fonts.find( fontFile->GetPath() ) }; itFind != m_Fonts.end() ) {
//             return itFind->second;
//         }
//
//         FontLoadDescription fontDesc{};
//         fontDesc.WithFile( fontFile )
//                 .WithPixelSize( description.FontSize );
//
//         FontHandle fontHandle{ FontFactory::Get()->LoadFont( fontDesc ) };
//         if ( !fontHandle.IsEmpty() ) {
//             auto [it, success]{
//                 m_Fonts.try_emplace( fontFile->GetPath(), fontHandle )
//             };
//
//             if ( success ) {
//                 return m_Fonts[fontFile->GetPath()];
//             }
//         }
//
//         return FontHandle::CreateEmpty();
//     }
//
//     auto AssetsService::LoadMaterial( std::string_view uri ) -> MaterialHandle {
//         // TODO: implement logic of loading the material in memory
//         return CreateMaterial();
//     }
//
//     auto AssetsService::LoadMaterial( const Path& uri ) -> MaterialHandle {
//         // TODO: implement logic of loading the material in memory
//         return CreateMaterial();
//     }
//
     auto AssetsService::LoadDummyAssets() -> void {
         // Load the checkerboard
     }
}