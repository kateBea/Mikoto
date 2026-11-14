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

#include <filesystem>
#include <system_error>

#include <EASTL/sort.h>
#include <EASTL/unique_ptr.h>

#include <yaml-cpp/yaml.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>
#include <Core/Singleton.hh>

#include <Logging/Logger.hh>

#include <Threading/TaskService.hh>

#include <Filesystem/FileSystem.hh>
#include <Filesystem/FileService.hh>
#include <Filesystem/FileWatcherService.hh>

#include <Assets/Utility.hh>
#include <Assets/AssetModel.hh>
#include <Assets/MeshFactory.hh>
#include <Assets/FontFactory.hh>
#include <Assets/ImageProcessing.hh>
#include <Assets/AssetRegistryService.hh>

#include <Material/Utility.hh>

#include <Audio/AudioDevice.hh>
#include <Audio/AudioService.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Core/RenderSystem.hh>

namespace mikoto::asset {

    using namespace mikoto::core;
    using namespace mikoto::audio;
    using namespace mikoto::material;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;

    MKT_NODISCARD static auto MetadataPath( 
        const filesystem::Path& source, 
        const filesystem::Path& cacheRoot ) -> filesystem::Path 
    {
        if ( cacheRoot.IsEmpty() ) {
            MKT_THROW_RUNTIME_ERROR( "Asset metadata requires a cache directory." );
        }

        // Key by the full source path so equally named files in different
        // directories cannot overwrite one another's import information.
        return filesystem::Path{ cacheRoot.GetPathTyped<std::filesystem::path>() / "Metadata" /
                                 string::Format( "{}.mktmeta", GetHashedAssetID( source ) ).c_str() };
    }


    auto AssetRegistryService::SaveAssetMetadata( 
        const AssetMetadata& metadata, 
        const filesystem::Path& cacheRoot ) -> bool 
    {
        try {
            YAML::Node node;
            node["Version"] = 1;
            node["ID"] = metadata.GetID();
            node["Source"] = metadata.GetPath().GetAbsolute().c_str();
            node["Type"] = static_cast<core::u32>( metadata.GetType() );
            node["Revision"] = metadata.GetSourceRevision();
            node["CpuBytes"] = metadata.GetMemoryUsage().mCpuBytes;
            node["GpuBytes"] = metadata.GetMemoryUsage().mGpuBytes;

            for ( const auto dependency: metadata.GetDependencies() ) {
                node["Dependencies"].push_back( dependency );
            }

            const auto text{ YAML::Dump( node ) };
            filesystem::FileHandle file{ filesystem::FileService::Get()->CreateNewFile( MetadataPath( metadata.GetPath(), cacheRoot ) ) };

            if ( file.IsEmpty() ) {
                MKT_THROW_RUNTIME_ERROR( "Cannot open asset metadata file for writing." );
            }

            file->Write( text.data(), text.size() );
            return true;
        } catch ( const std::exception& error ) {
            MKT_CORE_LOGGER_WARN( "Asset metadata for '{}': {}", metadata.GetPath().GetC_Str(), error.what() );
            return false;
        }
    }

    auto AssetRegistryService::LoadAssetMetadata( 
        const filesystem::Path& source, 
        AssetType type, const filesystem::Path& cacheRoot ) -> AssetMetadata 
    {
        AssetMetadata result{};
        result.SetType( type );
        result.SetPath( filesystem::Path{ source.GetAbsolute() } );

        const auto cached{ MetadataPath( source, cacheRoot ) };
        const bool hasCachedMetadata{ cached.Exists() };

        // Read older metadata without modifying the source directory. Identity is
        // normalized to AssetRef's path hash below; old dependency IDs are discarded.
        const filesystem::Path input{ hasCachedMetadata ? cached : filesystem::Path{ source.GetAbsolute() + ".mktmeta" } };

        if ( input.Exists() ) {
            const auto node{ YAML::Load( filesystem::File{ input }.GetContentsString().c_str() ) };

            if ( hasCachedMetadata && node["Source"] &&
                 node["Source"].as<std::string>() != source.GetAbsolute().c_str() ) {
                MKT_THROW_RUNTIME_ERROR( "Asset metadata cache key collision." );
            }

            if ( node["Version"].as<core::u32>( 0 ) != 1 || !node["ID"] || !node["ID"].as<AssetID>() ) {
                MKT_THROW_RUNTIME_ERROR( "Invalid asset metadata identity." );
            }

            result.SetID( node["ID"].as<AssetID>() );
            result.SetSourceRevision( node["Revision"].as<core::u64>( 0 ) );
            result.SetMemoryUsage( { node["CpuBytes"].as<core::usize>( 0 ), node["GpuBytes"].as<core::usize>( 0 ) } );

            if ( const auto dependencies{ node["Dependencies"] }; dependencies && dependencies.IsSequence() ) {
                for ( const auto& entry: dependencies ) {
                    result.AddDependency( entry.as<AssetID>() );
                }
            }

            result.SetPersistent( hasCachedMetadata );
        }

        const auto pathID{ GetHashedAssetID( source ) };

        if ( result.GetID() != pathID ) {
            result.SetID( pathID );
            result.SetDependencies( {} );
            result.SetPersistent( false );
        }

        const auto native{ source.GetPathTyped<std::filesystem::path>() };
        const auto revision{ static_cast<core::u64>( std::filesystem::last_write_time( native ).time_since_epoch().count() ) ^
                             static_cast<core::u64>( std::filesystem::file_size( native ) ) };
        // Reusing an old payload estimate after an edit can under-admit work.
        // Keep identity, but discard stale import-derived data until reimport.
        if ( result.GetSourceRevision() != revision ) {
            result.SetMemoryUsage( {} );
            result.SetDependencies( {} );
            result.SetSourceRevision( revision );
            result.SetPersistent( SaveAssetMetadata( result, cacheRoot ) );
        } else if ( !result.IsPersistent() ) {
            result.SetPersistent( SaveAssetMetadata( result, cacheRoot ) );
        }

        return result;
    }


    AssetRegistryService::AssetRegistryService( const AssetRegistryServiceDescription& options )
        : mIsTextureCompressionEnabled{ options.mCompressTextures } {
    }

    auto AssetRegistryService::Initialize() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( mIsInitialized ) {
            return;
        }

        MKT_CORE_LOGGER_INFO( "Initializing AssetRegistryService..." );

        mGpuDevice = renderer::RenderSystem::Get()->GetGpuDevice();
        mAudioDevice = audio::AudioService::Get()->GetDevice();

        // We need to ensure animation folders exists
        // These are used to store the cached animations
        if ( filesystem::CreateIfNotExistsDirectory( kAssetCachePathBase ) ) {
            MKT_CORE_LOGGER_DEBUG( "Created directory to cache animation data. '{}'", kAssetCachePathBase.GetC_Str() );
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

        // Asset streamer setup
        AssetStreamingSettings streamingSettings{
            // ...
        };
        mStreamingSystem = eastl::make_unique<AssetStreamingSystem>();
        if ( mStreamingSystem ) {
            mStreamingSystem->Initialize();
        }

        // Material graph compiler
        mMaterialGraphCompiler = eastl::make_unique<MaterialGraphCompiler>();

        // Shader library
        const ShaderLibraryDescription description{
            .mDevice = mGpuDevice,
            .mRootPath{ "Resources/Shaders/slang" },
            .mCacheShaders = true,
            .mCachePath{ GetAssetCacheBasePath().GetPathTyped<std::filesystem::path>() / "Shaders" }
        };
        if ( !mShaderLibrary ) {
            mShaderLibrary = eastl::make_unique<ShaderLibrary>( description );
            mShaderLibrary->Initialize();
        }

        LoadDummyAssets();

        mIsInitialized = true;
    }

    auto AssetRegistryService::RegisterMetadata( const filesystem::Path& path, AssetType type ) -> AssetID {
        const filesystem::Path absolute{ path.GetAbsolute() };
        const std::lock_guard lock{ mMetadataMutex };

        if ( const auto found{ mMetadataPaths.find( absolute.GetAbsolute() ) }; found != mMetadataPaths.end() ) {
            return found->second;
        }

        auto metadata{ LoadAssetMetadata( absolute, type, GetAssetCacheBasePath() ) };

        if ( const auto found{ mMetadata.find( metadata.GetID() ) }; found != mMetadata.end() && found->second.GetPath() != absolute ) {
            // A moved sidecar preserves identity. A copied sidecar beside another
            // existing source is ambiguous and must not alias unrelated assets.
            if ( found->second.GetPath().Exists() ) {
                MKT_THROW_RUNTIME_ERROR( "Duplicate asset metadata ID: two source assets share the same identity." );
            }

            mMetadataPaths.erase( found->second.GetPath().GetAbsolute() );
        }

        const auto id{ metadata.GetID() };

        mMetadata[id] = eastl::move( metadata );
        mMetadataPaths[absolute.GetAbsolute()] = id;

        return id;
    }

    auto AssetRegistryService::FindMetadata( AssetID id ) const -> eastl::optional<AssetMetadata> {
        const std::lock_guard lock{ mMetadataMutex };
        const auto entry{ mMetadata.find( id ) };
        return entry == mMetadata.end() ? eastl::optional<AssetMetadata>{} : entry->second;
    }

    auto AssetRegistryService::RecordModelMetadata( const ModelHandle& model ) -> void {
        if ( !model ) {
            return;
        }

        const auto id{ RegisterMetadata( model->GetPath(), AssetType::eModel ) };
        eastl::vector<AssetID> dependencies{};

        for ( core::usize index{}; index < model->GetMeshNodeCount(); ++index ) {
            for ( const auto& [path, binding]: model->GetMeshNode( index ).GetProperties().mTextureBindings ) {
                if ( !path.IsEmpty() ) {
                    dependencies.push_back( RegisterMetadata( path, AssetType::eTexture ) );
                }
            }
        }

        eastl::sort( dependencies.begin(), dependencies.end() );
        dependencies.erase( eastl::unique( dependencies.begin(), dependencies.end() ), dependencies.end() );

        const std::lock_guard lock{ mMetadataMutex };

        auto& metadata{ mMetadata.at( id ) };

        metadata.SetMemoryUsage( model->GetResidentMemoryUsage() );
        metadata.SetDependencies( eastl::move( dependencies ) );
        metadata.SetPersistent( SaveAssetMetadata( metadata, GetAssetCacheBasePath() ) );
    }

    auto AssetRegistryService::ReloadModel( const AssetRef<Model>& reference ) -> void {
        mStreamingSystem->GetRegistry<Model>().Invalidate( reference.GetPath() );
    }

    auto AssetRegistryService::UpdateStreaming() -> void {
        if ( mIsInitialized ) {
            // OpenGL resource creation requires the context owned by the engine thread.
            // https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-wglmakecurrent
            mStreamingSystem->Update( mGpuDevice->IsGraphicsApi( renderer::rhi::GraphicsAPI::eOpenGL ) );
        }
    }

    auto AssetRegistryService::GetStreamingStatistics() const -> AssetStreamingStatistics {
        return mStreamingSystem->GetStatistics();
    }

    auto AssetRegistryService::GetLoadingProgress() const -> AssetLoadingProgress {
        return mStreamingSystem->GetLoadingProgress();
    }

    auto AssetRegistryService::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !mIsInitialized ) {
            return;
        }

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down AssetRegistryService..." );


        if ( mStreamingSystem ) {
            mStreamingSystem->Shutdown();
            mStreamingSystem.reset();
        }

        mFallbackTexture.Reset();
        {
            const std::lock_guard lock{ mMetadataMutex };
            mMetadata.clear();
            mMetadataPaths.clear();
        }

        if ( mMeshFactory ) {
            mMeshFactory->Shutdown();
            mMeshFactory.reset();
        }

        if ( mFontFactory ) {
            mFontFactory->Shutdown();
            mFontFactory.reset();
        }

        mMaterialGraphCompiler.reset();

        if ( mShaderLibrary ) {
            mShaderLibrary->Shutdown();
            mShaderLibrary.reset();
        }

        mAudioDevice = nullptr;
        mGpuDevice = nullptr;

        mIsInitialized = false;
    }

    auto AssetRegistryService::GetAssetCacheBasePath() const -> const filesystem::Path& {
        MKT_BEGIN_PROFILER();
        return kAssetCachePathBase;
    }

    auto AssetRegistryService::SetCompressTextures( bool enabled ) -> void {
        if ( mIsTextureCompressionEnabled.exchange( enabled ) == enabled ) {
            return;
        }

        mStreamingSystem->GetRegistry<Sprite>().Clear();
        mStreamingSystem->GetRegistry<Skybox>().Clear();
        mStreamingSystem->GetRegistry<Animation>().Clear();
        mStreamingSystem->GetRegistry<Skeleton>().Clear();
        mStreamingSystem->GetRegistry<Material>().Clear();
        mStreamingSystem->GetRegistry<asset::Model>().Clear();
        mStreamingSystem->GetRegistry<Texture>().Clear();
        mStreamingSystem->GetRegistry<VideoClip>().Clear();

        if ( mIsInitialized ) {
            LoadDummyAssets();
        }
    }

    auto AssetRegistryService::IsTextureCompressionEnabled() const -> bool {
        return mIsTextureCompressionEnabled.load();
    }

    auto AssetRegistryService::GetDummyTexture() -> renderer::rhi::TextureHandle {
        MKT_BEGIN_PROFILER();
        return mFallbackTexture;
    }

    auto AssetRegistryService::GetShaderLibrary() const -> ShaderLibrary* {
        return mShaderLibrary.get();
    }

    auto AssetRegistryService::CompileMaterialGraph( const material::MaterialGraph& graph ) -> material::CompileResult {
        if ( !mMaterialGraphCompiler ) {
            return { .mDiagnostics = { { "Material compiler is unavailable." } } };
        }

        auto compiled{ mMaterialGraphCompiler->Compile( graph ) };

        if ( compiled.mSuccess ) {
            MKT_CORE_LOGGER_INFO( "Material graph YAML:\n{}", compiled.mSource );
        }
        else {
            for ( const auto& diagnostic: compiled.mDiagnostics ) {
                MKT_CORE_LOGGER_ERROR( "Material graph: {}", diagnostic.mMessage );
            }
        }

        return compiled;
    }

    auto AssetRegistryService::SerializeMaterial( material::MaterialHandle material ) const -> bool {
        MKT_BEGIN_PROFILER();
        return false;
    }

    auto AssetRegistryService::SerializeMaterial( material::MaterialHandle material, filesystem::Path& path ) const -> bool {
        MKT_BEGIN_PROFILER();

        if ( !material || path.IsEmpty() ) {
            return false;
        }

        try {
            material->Serialize( path );
            return true;
        } catch ( const std::exception& error ) {
            MKT_CORE_LOGGER_ERROR( "Cannot save material '{}': {}", path.GetC_Str(), error.what() );
            return false;
        }
    }

    auto AssetRegistryService::SerializeMaterial( material::MaterialHandle material, filesystem::FileHandle file ) const -> bool {
        MKT_BEGIN_PROFILER();

        if ( !file ) {
            return false;
        }

        auto path{ file->GetPath() };
        const bool saved{ SerializeMaterial( material, path ) };

        if ( saved ) {
            file->UpdateContentsFromDisk();
        }

        return saved;
    }

    auto AssetRegistryService::DeSerializeMaterial( filesystem::FileHandle file ) const -> material::MaterialHandle {
        MKT_BEGIN_PROFILER();

        if ( !file ) {
            return material::MaterialHandle::CreateEmpty();
        }

        try {
            return material::LoadMaterial( file );
        } catch ( const std::exception& error ) {
            MKT_CORE_LOGGER_ERROR( "Cannot load material '{}': {}", file->GetPath().GetC_Str(), error.what() );
            return material::MaterialHandle::CreateEmpty();
        }
    }

    auto AssetRegistryService::DeSerializeMaterial( const filesystem::Path& file ) const -> material::MaterialHandle {
        MKT_BEGIN_PROFILER();

        if ( file.IsEmpty() ) {
            return material::MaterialHandle::CreateEmpty();
        }

        try {
            return material::LoadMaterial( filesystem::FileHandle::New( file ) );
        } catch ( const std::exception& error ) {
            MKT_CORE_LOGGER_ERROR( "Cannot load material '{}': {}", file.GetC_Str(), error.what() );
            return material::MaterialHandle::CreateEmpty();
        }
    }

    auto AssetRegistryService::CreateMaterial( const material::ParticleMaterialDescription& desc ) -> material::MaterialHandle {
        MKT_BEGIN_PROFILER();
        return core::Ref<ParticleMaterial>::New( desc ).As<material::IMaterial>();
    }

    auto AssetRegistryService::CreateMaterial( const material::SurfaceMaterialDescription& desc ) -> material::MaterialHandle {
        MKT_BEGIN_PROFILER();
        material::MaterialHandle material{ core::Ref<material::SurfaceMaterial>::New( desc ) };

        if ( material.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "AssetRegistryService::CreateMaterial - Failed to create material" );
        }

        return material;
    }

    auto AssetRegistryService::CreateMaterial( const material::SkyboxMaterialDescription& desc ) -> material::MaterialHandle {
        MKT_BEGIN_PROFILER();
        material::MaterialHandle material{ core::Ref<material::SkyboxMaterial>::New( desc ) };

        if ( material.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "AssetRegistryService::CreateMaterial - Failed to create material" );
        }

        return material;
    }

    auto AssetRegistryService::CreateMaterial( const material::PostProcessMaterialDescription& desc ) -> material::MaterialHandle {
        MKT_BEGIN_PROFILER();
        material::MaterialHandle material{ core::Ref<material::PostProcessMaterial>::New( desc ) };

        if ( material.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "AssetRegistryService::CreateMaterial - Failed to create material" );
        }

        return material;
    }

    auto AssetRegistryService::LoadModel( const filesystem::Path& uri ) -> ModelHandle {
        MKT_BEGIN_PROFILER_NAMED();

        const ModelLoadDescription modelLoadDescription{
            .mFile = filesystem::FileService::Get()->LoadFile( uri ),
            .mExtractTextures = true,
        };

        return LoadModel( modelLoadDescription );
    }

    auto AssetRegistryService::LoadModel( const ModelLoadDescription& description ) -> ModelHandle {
        MKT_BEGIN_PROFILER_NAMED();

        if ( description.mFile.IsEmpty() ) {
            return ModelHandle::CreateEmpty();
        }

        const filesystem::Path& path{ description.mFile->GetPath().GetAbsolute() };

        CreateAssetCacheFolder( path );

        ModelHandle model{
            MeshFactory::Get()->ImportModel( ModelLoadDescription{
                .mFile = description.mFile,
                .mApi = mGpuDevice->GetGraphicsApi(),
                .mExtractTextures = description.mExtractTextures,
                .mDeferTextures = description.mDeferTextures,
                .mImportConfig = description.mImportConfig,
            } )
        };

        if ( !model.IsEmpty() ) {
            filesystem::FileWatcherService::Get()->Watch( path,
                []( const filesystem::Path& pathCallable, filesystem::FileWatchEvent event ) -> void {
                if ( event == filesystem::FileWatchEvent::eModified ) {
                    MKT_CORE_LOGGER_INFO( "Model file at [{}] has been updated", pathCallable.GetC_Str() );
                }
            } );
        }

        RecordModelMetadata( model );

        return model;
    }

    auto AssetRegistryService::GetRegisteredScripts() const -> eastl::vector<AssetMetadata> {
        const std::lock_guard lock{ mMetadataMutex };
        eastl::vector<AssetMetadata> scripts;

        for ( const auto& [id, metadata]: mMetadata ) {
            if ( metadata.GetType() == AssetType::eScript ) {
                scripts.push_back( metadata );
            }
        }

        return scripts;
    }

    auto AssetRegistryService::ReloadScriptSource( const filesystem::Path& path ) -> void {
        mStreamingSystem->GetRegistry<ScriptSource>().Invalidate( filesystem::Path{ path.GetAbsolute() } );
    }

    auto AssetRegistryService::CreateTextureResource( const filesystem::Path& path ) -> renderer::rhi::TextureHandle {
        asset::ImageHandle image{ PrepareImageForUpload( LoadTextureImage( path, GetAssetCacheBasePath(), IsTextureCompressionEnabled() ), *mGpuDevice ) };

        if ( image.IsEmpty() ) {
            return renderer::rhi::TextureHandle::CreateEmpty();
        }

        auto textureDescription{ renderer::rhi::TextureCreateDescription{}
            .SetImageData( image )
            .SetWidth( core::as<core::i32>( image->mWidth ) )
            .SetHeight( core::as<core::i32>( image->mHeight ) )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetMultisampling( renderer::rhi::Multisampling::eMsaaX1 )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::ShaderResource | renderer::rhi::TextureUsageFlagsBits::CopyDest )
            .SetFormat( GetTextureFormat( image->mFormat ) ) };

        renderer::rhi::TextureHandle texture{ mGpuDevice->CreateTexture( textureDescription ) };

        if ( !texture.IsEmpty() ) {
            texture->SetDebugName( string::Format( "Texture: {}", path.GetC_Str() ) );

            filesystem::FileWatcherService::Get()->Watch( path,
                []( const filesystem::Path& pathCallable, filesystem::FileWatchEvent event ) -> void {
                if ( event == filesystem::FileWatchEvent::eModified ) {
                    MKT_CORE_LOGGER_INFO( "Texture file at [{}] has been updated", pathCallable.GetC_Str() );
                }
            } );
        }

        if ( texture ) {
            const auto id{ RegisterMetadata( path, AssetType::eTexture ) };
            const std::lock_guard lock{ mMetadataMutex };
            auto& metadata{ mMetadata.at( id ) };

            // Decode keeps a working copy plus the upload source. This is
            // an admission estimate; driver allocations are reported separately.
            metadata.SetMemoryUsage( { image->mBufferSpan->GetSize() * 2, texture->GetStorageSizeBytes() } );
            metadata.SetPersistent( SaveAssetMetadata( metadata, GetAssetCacheBasePath() ) );
        }

        return texture;
    }

    auto AssetRegistryService::LoadTexture( const filesystem::Path& uri, renderer::rhi::TextureDimension dimension ) -> renderer::rhi::TextureHandle {
        MKT_BEGIN_PROFILER();
        auto desc{ TextureLoadDescription{}
            .SetPath( uri )
            .SetDimensions( dimension ) };

        return LoadTexture( desc );
    }

    auto AssetRegistryService::LoadTexture( const TextureLoadDescription& description ) -> renderer::rhi::TextureHandle {
        MKT_BEGIN_PROFILER_NAMED();

        if ( description.mPath.IsEmpty() ) {
            return renderer::rhi::TextureHandle::CreateEmpty();
        }

        const filesystem::Path& path{ description.mPath.GetAbsolute() };

        if ( IsTextureCompressionEnabled() ) {
            CreateAssetCacheFolder( path );
        }

        if ( description.mDimension == renderer::rhi::TextureDimension::eTexture2D ) {
            RegisterMetadata( path, AssetType::eTexture );
            return CreateTextureResource( path );
        }

        return renderer::rhi::TextureHandle::CreateEmpty();
    }

    auto AssetRegistryService::LoadAudio( const asset::AudioLoadDescription& description ) -> audio::AudioHandle {
        MKT_BEGIN_PROFILER_NAMED();

        if ( description.mFile.IsEmpty() ) {
            return audio::AudioHandle::CreateEmpty();
        }

        const filesystem::Path& path{ description.mFile->GetPath().GetAbsolute() };

        CreateAssetCacheFolder( path );

        RegisterMetadata( path, AssetType::eAudio );

        audio::AudioHandle audio{ mAudioDevice->LoadAudio( description.mFile, description.mVolume ) };

        if ( !audio.IsEmpty() ) {
            filesystem::FileWatcherService::Get()->Watch( path,
                []( const filesystem::Path& pathCallable, filesystem::FileWatchEvent event ) -> void {
                if ( event == filesystem::FileWatchEvent::eModified ) {
                    MKT_CORE_LOGGER_INFO( "Audio file at [{}] has been updated", pathCallable.GetC_Str() );
                }
            } );
        }

        return audio;
    }

    auto AssetRegistryService::LoadFont( const filesystem::Path& uri ) -> renderer::FontHandle {
        MKT_BEGIN_PROFILER_NAMED();

        const asset::FontLoadDescription fontLoadDescription{
            .mFile = filesystem::FileService::Get()->LoadFile( uri ),
            .mSize = 24.0f
        };

        return LoadFont( fontLoadDescription );
    }

    auto AssetRegistryService::LoadFont( const asset::FontLoadDescription& description ) -> renderer::FontHandle {
        MKT_BEGIN_PROFILER_NAMED();

        if ( description.mFile.IsEmpty() ) {
            return renderer::FontHandle::CreateEmpty();
        }

        const filesystem::Path path{ description.mFile->GetPath().GetAbsolute() };

        CreateAssetCacheFolder( path );

        RegisterMetadata( path, AssetType::eFont );

        const auto fontFile{ description.mFile };
        auto fontDesc{ asset::FontLoadDescription{}
            .SetFile( fontFile )
            .SetSize( description.mSize ) };
        fontDesc.mColorFont = description.mColorFont;

        renderer::FontHandle fontHandle{ mFontFactory->LoadFont( fontDesc ) };

        if ( !fontHandle.IsEmpty() ) {
            filesystem::FileWatcherService::Get()->Watch( path,
                []( const filesystem::Path& pathCallable, filesystem::FileWatchEvent event ) -> void {
                if ( event == filesystem::FileWatchEvent::eModified ) {
                    MKT_CORE_LOGGER_INFO( "Font file at [{}] has been updated", pathCallable.GetC_Str() );
                }
            } );
        }

        return fontHandle;
    }

    auto AssetRegistryService::LoadMaterial( const filesystem::Path& uri ) -> material::MaterialHandle {
        MKT_BEGIN_PROFILER();
        const auto path{ uri.GetAbsolute() };
        RegisterMetadata( path, AssetType::eMaterial );
        return DeSerializeMaterial( path );
    }

    auto AssetRegistryService::CreateAssetCacheFolder( const filesystem::Path& path ) -> void {
        MKT_BEGIN_PROFILER();
        const eastl::string cacheFolder{ string::Format( "{}/{}", GetAssetCacheBasePath().GetC_Str(), GetHashedAssetID( path ) ) };

        // create_directories returns false for an existing directory too.
        // Only its error code indicates a failure; cache hits are normal.
        std::error_code error{};
        std::filesystem::create_directories( filesystem::Path{ cacheFolder }.GetPathTyped<std::filesystem::path>(), error );

        if ( error ) {
            MKT_CORE_LOGGER_WARN( "Could not create asset cache folder '{}': {}", cacheFolder, error.message() );
        }
    }

    auto AssetRegistryService::LoadDummyAssets() -> void {
        MKT_BEGIN_PROFILER();

        // Load the checkerboard
        mFallbackTexture = LoadAsset<renderer::rhi::ITexture>( kDummyTexturePath, renderer::rhi::TextureDimension::eTexture2D );
    }

    auto AssetRegistryService::LoadScript( const ScriptLoadDescription& description ) -> core::Ref<ScriptSource> {
        const auto& path{ description.mPath };
        const auto extension{ path.GetNormalizedExtension() };

        if ( extension != "lua" && extension != "cs" && extension != "cpp" ) {
            MKT_THROW_RUNTIME_ERROR( "Script source assets must be .lua, .cs or .cpp files." );
        }

        auto source{ core::Ref<ScriptSource>::New( path ) };
        const auto id{ RegisterMetadata( path, AssetType::eScript ) };
        const std::lock_guard lock{ mMetadataMutex };
        mMetadata.at( id ).SetMemoryUsage( source->GetResidentMemoryUsage() );
        return source;
    }

    auto AssetRegistryService::GetMetadata( AssetID id ) const -> const AssetMetadata* {
        thread_local eastl::optional<AssetMetadata> snapshot{};
        snapshot = FindMetadata( id );

        return snapshot ? MKT_ADDRESSOF( *snapshot ) : nullptr;
    }
}// namespace mikoto::asset
