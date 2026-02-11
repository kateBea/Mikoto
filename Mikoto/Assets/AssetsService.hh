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

#ifndef MIKOTO_ASSETS_SERVICE_HH
#define MIKOTO_ASSETS_SERVICE_HH

#include <vector>
#include <future>
#include <atomic>
#include <mutex>
#include <ankerl/unordered_dense.h>

#include <Assets/Font.hh>
#include <Assets/Model.hh>
#include <Assets/Texture.hh>
#include <Audio/AudioDevice.hh>
#include <Common/Common.hh>
#include <Common/Service.hh>
#include <Common/Singleton.hh>
#include <Library/Data/ResourcePool.hh>
#include <Library/Utility/Types.hh>
#include <Renderer/Core/FontFactory.hh>
#include <Renderer/Core/RenderUtility.hh>
#include <Threading/TaskService.hh>

#include <Assets/AudioClip.hh>
#include <Assets/MeshFactory.hh>
#include <Material/PBRMaterial.hh>
#include <Material/TextureCube.hh>
#include <Filesystem/FileSystem.hh>

namespace Mikoto {

    struct AssetsServiceDescription {};

    /**
    * @class AssetsService
    * @brief Manages the loading, retrieval, and deletion of assets.
    *
    * The `AssetsService` class provides an interface for handling assets such as models,
    * textures, fonts, and audio files. It supports both synchronous and asynchronous
    * loading of resources, and provides functionality to manage and clean up resources.
    */
    class AssetsService final : public IService, public Singleton<AssetsService> {
    public:
        /**
        * @brief Constructs an AssetsService with the provided configuration.
        * @param options The configuration for initializing the AssetsService.
        */
        explicit AssetsService( const AssetsServiceDescription& options );

        /**
        * @brief Initializes the AssetsService.
        * Initializes this service structures. After this call user can safely load assets.
        */
        auto Init() -> void override;

        /**
        * @brief Shuts down the AssetsService.
        * Destroys this service structures. No other methods should be called after a call to Shutdown()
        */
        auto Shutdown() -> void override;

        /**
        * @brief Retrieves an asset of the specified type by its URI.
        *
        * This function searches for an asset associated with the given URI and attempts
        * to return it as a handle to the specified `AssetType`. If the asset is not found
        * returns an empty handle.
        *
        * @tparam AssetType The type of the asset to retrieve. Must be a type derived from the base asset type.
        * @param uri A string view representing the URI or identifier of the asset.
        * @return Pointer to the asset of type `AssetType` if found and valid; otherwise, `nullptr`.
        */
        template<typename AssetType>
        MKT_NODISCARD auto GetAssetByUri( const std::string_view uri ) -> Ref<AssetType> {
            const auto fullpath{ Filesystem::GetGetAbsolutePathString( uri ) };

            if constexpr (std::is_same_v<AssetType, Model>) {
                if ( const auto it{ m_Models.find(fullpath) }; it != m_Models.end())
                    return it->second;
            }
            else if constexpr (std::is_same_v<AssetType, Texture>) {
                if ( const auto it{ m_Textures2D.find(fullpath) }; it != m_Textures2D.end())
                    return it->second;
            }
            else if constexpr (std::is_same_v<AssetType, TextureCube>) {
                if ( const auto it{ m_Textures2D.find(fullpath) }; it != m_TexturesCubes.end())
                    return it->second;
            }
            else if constexpr (std::is_same_v<AssetType, Audio>) {
                if ( const auto it{ m_Audios.find(fullpath) }; it != m_Audios.end())
                    return it->second;
            }
            else if constexpr (std::is_same_v<AssetType, Font>) {
                if ( const auto it{ m_Fonts.find(fullpath) }; it != m_Fonts.end())
                    return it->second;
            }
            else if constexpr (std::is_same_v<AssetType, Material>) {
                if ( const auto it{ m_Materials.find(std::string(uri)) }; it != m_Materials.end())
                    return it->second;
            }

            return Ref<AssetType>::CreateEmpty();
        }

        template<typename AssetType>
        auto LoadAsset( auto&&... args ) -> Ref<AssetType> {
            if constexpr (std::is_same_v<AssetType, Model>) {
                return LoadModel( std::forward<decltype(args)>(args)... );
            }
            else if constexpr (std::is_same_v<AssetType, Texture>) {
                return LoadTexture( std::forward<decltype(args)>(args)... );
            }
            else if constexpr (std::is_same_v<AssetType, TextureCube>) {
                return LoadCubeMap( std::forward<decltype(args)>(args)... );
            }
            else if constexpr (std::is_same_v<AssetType, Audio>) {
                return LoadAudio( std::forward<decltype(args)>(args)... );
            }
            else if constexpr (std::is_same_v<AssetType, Font>) {
                return LoadFont( std::forward<decltype(args)>(args)... );
            }
            else if constexpr (std::is_same_v<AssetType, Material>) {
                return LoadMaterial( std::forward<decltype(args)>(args)... );
            }

            return Ref<AssetType>::CreateEmpty();
        }

        template<typename AssetType>
        auto LoadAssetAsync( auto&&... args ) -> void {
            auto tuple{ std::make_tuple(std::forward<decltype(args)>(args)...) };

            TaskService::Get()->Submit([this, argsTuple = std::move(tuple)]() mutable -> void {
                std::apply([this]<typename... Args>(Args&&... unpackedArgs) {
                    LoadAsset<AssetType>(std::forward<Args>(unpackedArgs)...);
                }, std::move(argsTuple));
            });
        }

        MKT_NODISCARD auto GetDummyTexture() -> TextureHandle;

        MKT_NODISCARD auto CreateMaterial( const MaterialCreateInfo& spec = {} ) -> MaterialHandle;

        ~AssetsService() override = default;

    private:
        auto LoadModel( std::string_view uri ) -> ModelHandle;
        auto LoadModel( const ModelLoadDescription& description) -> ModelHandle;

        auto LoadTexture( const Path& uri, bool isHDR = false ) -> TextureHandle;
        auto LoadTexture( std::string_view uri, bool isHDR = false  ) -> TextureHandle;
        auto LoadTexture( const TextureDescription& description ) -> TextureHandle;
        auto LoadTexture( const TextureLoadDescription& description) -> TextureHandle;

        auto LoadCubeMap( const Path& uri ) -> TextureHandle;
        auto LoadCubeMap( const TextureCubeLoadDescription& description) -> TextureHandle;

        auto LoadAudio( const AudioLoadDescription& description) -> AudioHandle;

        auto LoadFont( const Path& uri ) -> FontHandle;
        auto LoadFont( std::string_view uri ) -> FontHandle;
        auto LoadFont( const FontLoadDescription& description) -> FontHandle;

        auto LoadMaterial( std::string_view uri) -> MaterialHandle;
        auto LoadMaterial( const Path& uri) -> MaterialHandle;

        auto LoadDummyAssets() -> void;

    private:
        static constexpr std::string_view s_DummyTexturePath{ "./Resources/Textures/texture.png" };

    private:
        Unique<MeshFactory> m_MeshFactory{};
        Unique<FontFactory> m_FontFactory{};

        GpuDevice* m_GpuDevice{ nullptr };
        AudioDevice* m_AudioDevice{ nullptr };

        ResourcePoolTyped<PBRMaterial> m_PBRMaterialsPool{};

        std::mutex m_Texture2DPoolMutex{};
        std::mutex m_TextureCubePoolMutex{};

        ankerl::unordered_dense::map<std::string, MaterialHandle> m_Materials{};
        ankerl::unordered_dense::map<std::string, ModelHandle> m_Models{};
        ankerl::unordered_dense::map<std::string, AudioHandle> m_Audios{};
        ankerl::unordered_dense::map<std::string, FontHandle> m_Fonts{};

        ankerl::unordered_dense::map<std::string, TextureHandle> m_Textures2D{};
        ankerl::unordered_dense::map<std::string, TextureHandle> m_TexturesCubes{};
    };
}// namespace Mikoto

#endif//MIKOTO_ASSETS_SERVICE_HH
