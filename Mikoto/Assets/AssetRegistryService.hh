//    Copyright 2026 ケイト
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

#ifndef MIKOTO_ASSETS_ASSET_REGISTRY_SERVICE_HH
#define MIKOTO_ASSETS_ASSET_REGISTRY_SERVICE_HH

#include <EASTL/utility.h>
#include <EASTL/tuple.h>
#include <EASTL/atomic.h>
#include <EASTL/vector.h>
#include <EASTL/string.h>
#include <EASTL/optional.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/type_traits.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Service.hh>
#include <Core/Profiler.hh>
#include <Core/Resource.hh>
#include <Core/Exception.hh>
#include <Core/Singleton.hh>

#include <Logging/Assert.hh>

#include <Threading/TaskService.hh>

#include <Filesystem/FileSystem.hh>

#include <Assets/Asset.hh>
#include <Assets/AssetModel.hh>
#include <Assets/MeshFactory.hh>
#include <Assets/FontFactory.hh>
#include <Assets/AssetTexture.hh>
#include <Assets/ShaderLibrary.hh>
#include <Assets/AssetStreamingSystem.hh>

#include <Material/MaterialGraph.hh>
#include <Material/SkyboxMaterial.hh>
#include <Material/SurfaceMaterial.hh>
#include <Material/ParticleMaterial.hh>
#include <Material/PostProcessMaterial.hh>

#include <Audio/AudioClip.hh>
#include <Audio/AudioDevice.hh>

#include <Renderer/Text/Font.hh>
#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Texture.hh>
#include <Renderer/Rhi/GpuDevice.hh>

namespace mikoto::asset {

    struct AssetRegistryServiceDescription {
        filesystem::Path mShadersPath{};
        bool mCompressTextures{ false };
    };

    // This service will ultimately be scoped, we cannot tie this to the application
    // Every project will load its own, when we load a new project we load its asset service
    // and free everything from before
    class AssetRegistryService final : public core::IService, public core::Singleton<AssetRegistryService> {
    public:
        explicit AssetRegistryService( const AssetRegistryServiceDescription& options );

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        // Forwards an asset-specific load description. 
        // The first request defines its cached import settings.
        template<typename T, typename... Args>
        auto Request( AssetRef<T> reference, Args&&... args ) -> AssetHandle<T> {
            MKT_STATIC_ASSERT( eastl::is_same_v<T, typename AssetTraits<T>::Asset>, "Request an asset class, not its backend resource." );
            
            auto description{ [&] {
                if constexpr ( sizeof...( Args ) == 0 ) {
                    return MakeDescription<T>( reference.GetPath() );
                } else {
                    return MakeDescription<T>( eastl::forward<Args>( args )... );
                }
            }() };

            SetDescriptionPath( description, reference.GetPath() );

            return Request<T>( eastl::move( description ) );
        }

        template<typename T>
        auto Request( typename AssetTraits<T>::Description description ) -> AssetHandle<T> {
            const auto path{ DescriptionPath( description ) };
            if ( path.IsEmpty() ) {
                return {};
            }

            const auto id{ RegisterMetadata( path, AssetTraits<T>::kType ) };
            const auto metadata{ FindMetadata( id ) };
            const auto reservation{ metadata ? metadata->GetMemoryUsage() : AssetMemoryUsage{} };
            const auto priority{ description.mPriority };

            auto handle{ mStreamingSystem->GetRegistry<T>().Request( path,
                [this, description = eastl::move( description )]() mutable { 
                    return Import<T>( description ); 
                },
                [this, id, priority, reservation]( auto job ) {
                    mStreamingSystem->Enqueue( eastl::move( job ), priority, id, reservation );
                } ) };

            mStreamingSystem->Promote( id, priority );
            return handle;
        }

        template<typename T>
        MKT_NODISCARD auto TryGet( const AssetHandle<T>& handle ) const -> const T* {
            return handle.GetIfReady().GetPtr();
        }

        template<typename T>
        MKT_NODISCARD auto GetState( const AssetHandle<T>& handle ) const -> AssetState {
            return handle.GetState();
        }

        template<typename T>
        MKT_NODISCARD auto GetProgress( const AssetHandle<T>& handle ) const -> core::f32 {
            return handle.GetProgress();
        }

        template<typename T>
        MKT_NODISCARD auto IsLoaded( AssetRef<T> reference ) const -> bool {
            return mStreamingSystem->GetRegistry<T>().GetLoadState( reference.GetPath() ) == AssetState::eReady;
        }

        auto UpdateStreaming() -> void;
        auto SetCompressTextures( bool enabled ) -> void;

        auto ReloadScriptSource( const filesystem::Path& path ) -> void;
        auto ReloadModel( const AssetRef<Model>& reference ) -> void;
 
        MKT_NODISCARD auto GetMetadata( AssetID id ) const -> const AssetMetadata *;

        MKT_NODISCARD auto GetRegisteredScripts() const -> eastl::vector<AssetMetadata>;

        MKT_NODISCARD auto FindMetadata( AssetID id ) const -> eastl::optional<AssetMetadata>;
        MKT_NODISCARD auto GetStreamingStatistics() const -> AssetStreamingStatistics;

        MKT_NODISCARD auto GetLoadingProgress() const -> AssetLoadingProgress;

        MKT_NODISCARD auto IsTextureCompressionEnabled() const -> bool;

        MKT_NODISCARD auto GetDummyTexture() -> renderer::rhi::TextureHandle;
        MKT_NODISCARD auto GetAssetCacheBasePath() const  -> const filesystem::Path&;

        template<typename T>
        MKT_NODISCARD auto GetAssetByUri( const eastl::string_view uri ) -> core::Ref<T> {
            using Asset = typename AssetTraits<T>::Asset;

            auto resource{ mStreamingSystem->GetRegistry<Asset>().GetIfReady( filesystem::Path{ uri } ) };
            if constexpr ( eastl::is_same_v<T, Asset> ) {
                return resource;
            } else {
                return resource ? resource->GetResource() : core::Ref<T>{};
            }
        }

        template<typename T, typename... Args>
        MKT_NODISCARD auto LoadAsset( Args&&... args ) -> core::Ref<T> {
            using Asset = typename AssetTraits<T>::Asset;
            auto description{ MakeDescription<Asset>( eastl::forward<Args>( args )... ) };
            const auto path{ DescriptionPath( description ) };

            if ( path.IsEmpty() ) {
                return {};
            }

            RegisterMetadata( path, AssetTraits<Asset>::kType );

            auto resource{ mStreamingSystem->GetRegistry<Asset>().GetOrLoad( path, [this, &description] {
                return Import<Asset>( description );
            } ) };

            if constexpr ( eastl::is_same_v<T, Asset> ) {
                return resource;
            } else {
                return resource ? resource->GetResource() : core::Ref<T>{};
            }
        }

        template<typename T, typename... Args>
        MKT_NODISCARD auto LoadAssetHandle( Args&&... args ) -> AssetHandle<T> {
            MKT_STATIC_ASSERT( eastl::is_same_v<T, typename AssetTraits<T>::Asset> );

            auto description{ MakeDescription<T>( eastl::forward<Args>( args )... ) };
            const auto path{ DescriptionPath( description ) };
            if ( path.IsEmpty() ) { 
                return {}; 
            }

            RegisterMetadata( path, AssetTraits<T>::kType );

            return mStreamingSystem->GetRegistry<T>().LoadHandle( path, [this, &description] {
                return Import<T>( description );
            } );
        }

        template<typename T>
        MKT_NODISCARD auto RegisterLoaded( AssetRef<T> reference, core::Ref<T> resource ) -> AssetHandle<T> {
            if ( reference.GetPath().IsEmpty() || !resource ) { 
                return {}; 
            }

            RegisterMetadata( reference.GetPath(), AssetTraits<T>::kType );

            return mStreamingSystem->GetRegistry<T>().LoadHandle( reference.GetPath(), [&resource] {
                return resource;
            } );
        }

        template<typename AssetType>
        auto LoadAssetAsync( auto&&... args ) -> void {
            auto tuple{ eastl::make_tuple( eastl::forward<decltype( args )>( args )... ) };

            mStreamingSystem->Enqueue( [this, argsTuple = eastl::move( tuple )]() mutable -> void {
                eastl::apply( [this]<typename... Args>( Args&&... unpackedArgs ) {
                    ( void )LoadAsset<AssetType>( eastl::forward<Args>( unpackedArgs )... );
                }, eastl::move( argsTuple ) );
            }, LoadPriority::eBackground );
        }

        MKT_NODISCARD auto GetShaderLibrary() const -> ShaderLibrary*;

        MKT_NODISCARD auto CompileMaterialGraph( const material::MaterialGraph& graph ) -> material::CompileResult;

        MKT_NODISCARD auto SerializeMaterial( material::MaterialHandle material ) const -> bool;
        MKT_NODISCARD auto SerializeMaterial( material::MaterialHandle material, filesystem::Path& path ) const -> bool;
        MKT_NODISCARD auto SerializeMaterial( material::MaterialHandle material, filesystem::FileHandle file ) const -> bool;

        MKT_NODISCARD auto DeSerializeMaterial( filesystem::FileHandle file ) const -> material::MaterialHandle;
        MKT_NODISCARD auto DeSerializeMaterial( const filesystem::Path& file ) const -> material::MaterialHandle;

        MKT_NODISCARD auto CreateMaterial( const material::SurfaceMaterialDescription& spec) -> material::MaterialHandle;
        MKT_NODISCARD auto CreateMaterial( const material::SkyboxMaterialDescription& desc) -> material::MaterialHandle;
        MKT_NODISCARD auto CreateMaterial( const material::PostProcessMaterialDescription& desc) -> material::MaterialHandle;
        MKT_NODISCARD auto CreateMaterial( const material::ParticleMaterialDescription& desc ) -> material::MaterialHandle;

        MKT_NODISCARD auto SaveAssetMetadata( const AssetMetadata& metadata, const filesystem::Path& cacheRoot ) -> bool;
        MKT_NODISCARD auto LoadAssetMetadata( const filesystem::Path& path, AssetType type, const filesystem::Path& cacheRoot ) -> AssetMetadata;

    private:
        template<typename Description>
        MKT_NODISCARD static auto DescriptionPath( const Description &description ) -> filesystem::Path {
            if ( !description.mPath.IsEmpty() ) {
                return description.mPath;
            }

            // We check if description supports getting its mFile
            if constexpr ( requires { description.mFile; } ) {
                return description.mFile ? description.mFile->GetPath() : filesystem::Path{};
            }

            return {};
        }

        template<typename Description>
        static auto SetDescriptionPath( Description& description, const filesystem::Path& path ) -> void {
            const auto existing{ DescriptionPath( description ) };
            if ( !existing.IsEmpty() && GetHashedAssetID( existing ) != GetHashedAssetID( path ) ) {
                MKT_THROW_RUNTIME_ERROR( "Load description and asset reference name different sources." );
            }

            description.mPath = path;
        }

        template<typename T>
        MKT_NODISCARD static auto MakeDescription( 
            const typename AssetTraits<T>::Description &description ) -> typename AssetTraits<T>::Description 
        {
            return description;
        }

        template<typename T>
        MKT_NODISCARD auto MakeDescription( const filesystem::Path &path ) -> typename AssetTraits<T>::Description {
            typename AssetTraits<T>::Description description{};

            SetDescriptionPath( description, path );

            if constexpr ( eastl::is_same_v<T, Font> ) {
                description.mSize = 24.0f;
            }

            return description;
        }

        template<typename T>
        MKT_NODISCARD auto MakeDescription( 
            const filesystem::Path &path, 
            renderer::rhi::TextureDimension dimension ) -> typename AssetTraits<T>::Description 
        {
            auto description{ MakeDescription<T>( path ) };
            description.mDimension = dimension;
            return description;
        }

        template<typename T>
        MKT_NODISCARD auto Import( const typename AssetTraits<T>::Description &source ) -> core::Ref<T> {
            auto description{ source };

            if constexpr ( requires { description.mFile; } ) {
                if ( !description.mFile && !description.mPath.IsEmpty() ) {
                    description.mFile = filesystem::FileHandle::New( description.mPath );
                }
            }

            if constexpr ( eastl::is_same_v<T, Model> ) {
                return LoadModel( description );
            } 
            else if constexpr ( eastl::is_same_v<T, ScriptSource> ) {
                return LoadScript( description );
            } 
            else if constexpr ( eastl::is_same_v<T, Texture> ) {
                auto resource{ LoadTexture( description ) };
                return resource ? core::Ref<T>::New( eastl::move( resource ), 
                    AssetRef<T>{ DescriptionPath( description ) } ) : core::Ref<T>{};
            } 
            else if constexpr ( eastl::is_same_v<T, AudioClip> ) {
                auto resource{ LoadAudio( description ) };
                return resource ? core::Ref<T>::New( eastl::move( resource ) ) : core::Ref<T>{};
            } 
            else if constexpr ( eastl::is_same_v<T, VideoClip> ) {
                return core::Ref<T>::New( description );
            } 
            else if constexpr ( eastl::is_same_v<T, Font> ) {
                auto resource{ LoadFont( description ) };
                return resource ? core::Ref<T>::New( eastl::move( resource ) ) : core::Ref<T>{};
            } 
            else if constexpr ( eastl::is_same_v<T, Material> ) {
                auto resource{ LoadMaterial( description.mPath ) };
                return resource ? core::Ref<T>::New( eastl::move( resource ) ) : core::Ref<T>{};
            } 
            else if constexpr ( eastl::is_same_v<T, Animation> || eastl::is_same_v<T, Skeleton> ) {
                auto model{ LoadAsset<Model>( description.mPath ) };
                return core::Ref<T>::New( eastl::move( model ), description );
            } 
            else if constexpr ( eastl::is_same_v<T, ParticleEffect> ) {
                return core::Ref<T>::New( renderer::ParticleEmitter::Load( description.mPath ) );
            } 
            else if constexpr ( eastl::is_same_v<T, Skybox> ) {
                auto material{ LoadMaterial( description.mPath ) };
                if ( !material || !dynamic_cast<const material::SkyboxMaterial*>( material.GetPtr() ) ) {
                    MKT_THROW_RUNTIME_ERROR( "Skybox assets require a skybox material." );
                }

                return core::Ref<T>::New( material.template As<material::SkyboxMaterial>() );
            } 
            else if constexpr ( eastl::is_same_v<T, Sprite> ) {
                return core::Ref<T>::New( LoadAsset<Texture>( description.mPath ) );
            } 
            else {
                MKT_STATIC_ASSERT( sizeof( T ) == 0, "Unsupported asset type" );
            }

            return core::Ref<T>::CreateEmpty();
        }

        MKT_NODISCARD auto LoadModel( const filesystem::Path &uri ) -> ModelHandle;
        MKT_NODISCARD auto LoadModel( const ModelLoadDescription &description ) -> ModelHandle;
        MKT_NODISCARD auto LoadScript( const ScriptLoadDescription &description ) -> core::Ref<ScriptSource>;

        MKT_NODISCARD auto LoadTexture( const TextureLoadDescription &description ) -> renderer::rhi::TextureHandle;
        MKT_NODISCARD auto LoadTexture( const filesystem::Path &uri, renderer::rhi::TextureDimension dimension ) -> renderer::rhi::TextureHandle;

        MKT_NODISCARD auto LoadAudio( const asset::AudioLoadDescription &description ) -> audio::AudioHandle;

        MKT_NODISCARD auto LoadFont( const filesystem::Path &uri ) -> renderer::FontHandle;
        MKT_NODISCARD auto LoadFont( const asset::FontLoadDescription &description ) -> renderer::FontHandle;

        MKT_NODISCARD auto LoadMaterial( const filesystem::Path &uri ) -> material::MaterialHandle;
        MKT_NODISCARD auto RegisterMetadata( const filesystem::Path &path, AssetType type ) -> AssetID;
        MKT_NODISCARD auto CreateTextureResource( const filesystem::Path& path ) -> renderer::rhi::TextureHandle;

        auto LoadDummyAssets() -> void;
        auto RecordModelMetadata( const ModelHandle& model ) -> void;
        auto CreateAssetCacheFolder(const filesystem::Path& path) -> void;

    private:
        static inline const filesystem::Path kAssetCachePathBase{ "Assets/.cache" };
        static inline const filesystem::Path kDefaultAssetsPath{ "Assets/Materials" };
        static inline const filesystem::Path kMaterialsDefaultPath{ "Assets/Materials" };

        static inline const filesystem::Path kDummyTexturePath{ "Resources/Textures/texture.png" };

    private:
        // Devices
        audio::AudioDevice* mAudioDevice{ nullptr };
        renderer::rhi::IGpuDevice* mGpuDevice{ nullptr };

        // Asset factories
        eastl::unique_ptr<MeshFactory> mMeshFactory{};
        eastl::unique_ptr<FontFactory> mFontFactory{};
        eastl::unique_ptr<ShaderLibrary> mShaderLibrary{};
        eastl::unique_ptr<AssetStreamingSystem> mStreamingSystem{};

        // Resource compilers
        eastl::unique_ptr<material::MaterialGraphCompiler> mMaterialGraphCompiler{};

        // Place holder texture
        renderer::rhi::TextureHandle mFallbackTexture{};

        // Asset metadata
        mutable std::mutex mMetadataMutex{};
        eastl::unordered_map<AssetID, AssetMetadata> mMetadata{};
        eastl::unordered_map<eastl::string, AssetID> mMetadataPaths{};

        eastl::atomic<bool> mIsTextureCompressionEnabled{ false };
    };

}// namespace mikoto

#endif// MIKOTO_ASSETS_ASSET_REGISTRY_SERVICE_HH
