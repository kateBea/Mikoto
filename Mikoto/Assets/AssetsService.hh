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

#ifndef MIKOTO_ASSETS_SERVICE_HH
#define MIKOTO_ASSETS_SERVICE_HH

#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/tuple.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/utility.h>
#include <EASTL/vector.h>

#include <Assets/Asset.hh>
#include <Assets/AssetLoadRegistry.hh>
#include <Assets/MeshFactory.hh>
#include <Assets/Model.hh>
#include <Audio/AudioClip.hh>
#include <Audio/AudioDevice.hh>
#include <Core/Core.hh>
#include <Core/ResourcePool.hh>
#include <Core/Service.hh>
#include <Core/Singleton.hh>
#include <Core/String.hh>
#include <Core/Types.hh>
#include <Filesystem/FileSystem.hh>
#include <Material/PhysicalMaterial.hh>
#include <Material/PostProcessMaterial.hh>
#include <Material/SkyboxMaterial.hh>
#include <Renderer/Core/FontFactory.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Rhi/Texture.hh>
#include <Renderer/Rhi/Types.hh>
#include <Renderer/Text/Font.hh>
#include <Threading/TaskService.hh>

namespace mikoto::asset {

    struct TextureLoadDescription {
        Path mPath{};
        renderer::rhi::TextureDimension mDimension{ renderer::rhi::TextureDimension::eTexture2D };

        auto SetPath( const Path& path ) -> TextureLoadDescription&;
        auto SetDimensions( renderer::rhi::TextureDimension dim ) -> TextureLoadDescription&;
    };


    struct AssetsServiceDescription {};

    // This service will ultimately be scoped, we cannot tie this to the application
    // Every project will load its own, when we load a new project we load its asset service
    // and free everything from before
    class AssetsService final : public core::IService, public core::Singleton<AssetsService> {
    public:

        explicit AssetsService( const AssetsServiceDescription& options );

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        MKT_NODISCARD auto GetDummyTexture() -> renderer::rhi::TextureHandle;
        MKT_NODISCARD auto GetAssetCacheBasePath() const  -> const Path&;

        template<typename AssetType>
        MKT_NODISCARD auto GetAssetByUri( const eastl::string_view uri ) -> Ref<AssetType> {
            Path fullPath{ Path{ uri }.GetAbsolute() };

            if constexpr (std::is_same_v<AssetType, Model>) {
                return mModels.GetIfReady( fullPath );
            }
            else if constexpr (std::is_same_v<AssetType, renderer::rhi::ITexture>) {
                return mTextures.GetIfReady( fullPath );
            }
            else if constexpr (std::is_same_v<AssetType, audio::Audio>) {
                return mAudios.GetIfReady( fullPath );
            }
            else if constexpr (std::is_same_v<AssetType, renderer::Font>) {
                return mFonts.GetIfReady( fullPath );
            }
            else if constexpr (std::is_same_v<AssetType, material::Material>) {
                return mMaterials.GetIfReady( fullPath );
            }

            return Ref<AssetType>::CreateEmpty();
        }

        template<typename AssetType>
        MKT_NODISCARD auto LoadAsset( auto&&... args ) -> Ref<AssetType> {
            if constexpr (std::is_same_v<AssetType, Model>) {
                return LoadModel( eastl::forward<decltype(args)>(args)... );
            }
            else if constexpr (std::is_same_v<AssetType, renderer::rhi::ITexture>) {
                return LoadTexture( eastl::forward<decltype(args)>(args)... );
            }
            else if constexpr (std::is_same_v<AssetType, audio::Audio>) {
                return LoadAudio( eastl::forward<decltype(args)>(args)... );
            }
            else if constexpr (std::is_same_v<AssetType, renderer::Font>) {
                return LoadFont( eastl::forward<decltype(args)>(args)... );
            }
            else if constexpr (std::is_same_v<AssetType, material::Material>) {
                return LoadMaterial( eastl::forward<decltype(args)>(args)... );
            }

            return Ref<AssetType>::CreateEmpty();
        }

        template<typename AssetType>
        auto LoadAssetAsync( auto&&... args ) -> void {
            auto tuple{ eastl::make_tuple(eastl::forward<decltype(args)>(args)...) };

            threading::TaskService::Get()->Submit([this, argsTuple = eastl::move(tuple)]() mutable -> void {
                eastl::apply([this]<typename... Args>(Args&&... unpackedArgs) {
                    (void)LoadAsset<AssetType>(eastl::forward<Args>(unpackedArgs)...);
                }, eastl::move(argsTuple));
            });
        }

        // Materials can be serialized or deserialized from a YAML file with mktmtl file extension
        // the type is stored in the YAML file
        // material::MaterialHandle material{ AssetService::Get()->DeserializeMaterial("assets/materials/wireframe.mtl") };
        // material::MaterialHandle material{ AssetService::Get()->DeserializeMaterial("assets/materials/postprocess.mtl") };

        MKT_NODISCARD auto SerializeMaterial( material::MaterialHandle material ) const -> bool;
        MKT_NODISCARD auto SerializeMaterial( material::MaterialHandle material, filesystem::Path& path ) const -> bool;
        MKT_NODISCARD auto SerializeMaterial( material::MaterialHandle material, filesystem::FileHandle file ) const -> bool;

        MKT_NODISCARD auto DeSerializeMaterial( filesystem::FileHandle file ) const -> material::MaterialHandle;
        MKT_NODISCARD auto DeSerializeMaterial( const filesystem::Path& file ) const -> material::MaterialHandle;

        MKT_NODISCARD auto CreateMaterial( const material::PhysicMaterialDescription& spec) -> material::MaterialHandle;
        MKT_NODISCARD auto CreateMaterial( const material::SkyboxMaterialDescription& desc) -> material::MaterialHandle;
        MKT_NODISCARD auto CreateMaterial( const material::PostProcessMaterialDescription& desc) -> material::MaterialHandle;

        ~AssetsService() override = default;

    private:
        auto LoadModel( const Path& uri ) -> ModelHandle;
        auto LoadModel( const ModelLoadDescription& description) -> ModelHandle;

        auto LoadTexture( const Path& uri, renderer::rhi::TextureDimension dimension ) -> renderer::rhi::TextureHandle;
        auto LoadTexture( const TextureLoadDescription& description ) -> renderer::rhi::TextureHandle;

        auto LoadAudio( const audio::AudioLoadDescription& description) -> audio::AudioHandle;

        auto LoadFont( const Path& uri ) -> renderer::FontHandle;
        auto LoadFont( const renderer::FontLoadDescription& description) -> renderer::FontHandle;

        auto LoadMaterial( const Path& uri ) -> material::MaterialHandle;

        auto LoadDummyAssets() -> void;

        auto CreateAssetCacheFolder(const Path& path) -> void;

    private:
        static inline const Path kDummyTexturePath{ "Resources/Textures/texture.png" };

        static inline const Path kDefaultAssetsPath{ "Assets/Materials" };
        static inline const Path kAnimationCachePathBase{ "Assets/.cache" };

        static inline const Path kMaterialsDefaultPath{ "Assets/Materials" };


    private:
        eastl::unique_ptr<MeshFactory> mMeshFactory{};
        eastl::unique_ptr<renderer::FontFactory> mFontFactory{};

        renderer::rhi::IGpuDevice* mGpuDevice{ nullptr };
        audio::AudioDevice* mAudioDevice{ nullptr };

        AssetLoadRegistry<asset::Model> mModels{};
        AssetLoadRegistry<audio::Audio> mAudios{};
        AssetLoadRegistry<material::Material> mMaterials{};

        AssetLoadRegistry<renderer::Font> mFonts{};
        AssetLoadRegistry<renderer::rhi::ITexture> mTextures{};
    };
}

#endif//MIKOTO_ASSETS_SERVICE_HH
