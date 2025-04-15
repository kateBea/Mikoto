//
// Created by zanet on 1/26/2025.
//

#ifndef ASSETSSYSTEM_HH
#define ASSETSSYSTEM_HH

#include <ankerl/unordered_dense.h>

#include <Assets/Font.hh>
#include <Assets/Model.hh>
#include <Assets/Texture.hh>
#include <Audio/AudioDevice.hh>
#include <Common/Common.hh>
#include <Common/Service.hh>
#include <Library/Data/ResourcePool.hh>
#include <Library/Utility/Types.hh>
#include <Renderer/RenderUtility.hh>
#include <Threading/Task.hh>

#include "Assets/Audio.hh"

namespace Mikoto {

    /**
    * @struct AssetsServiceDescription
    * @brief Holds the configuration for creating an AssetsService.
    *
    * The `AssetsServiceCreateInfo` structure contains initialization parameters
    * for setting up an `AssetsService`, such as the initial size of the resource pool.
    * It provides a fluent interface for setting its properties.
    */
    struct AssetsServiceDescription {
        GpuDevice* Device{ nullptr };
        AudioDevice* AudDevice{ nullptr };

        /**
        * @brief Sets the device on which we load the assets.
        * @param Device The device to be assigned.
        * @return A reference to the modified `AssetsServiceCreateInfo` instance.
        */
        auto WithDevice( GpuDevice* Device ) -> AssetsServiceDescription&;

        /**
        * @brief Sets the device on which we load the audios.
        * @param Device The device to be assigned.
        * @return A reference to the modified `AssetsServiceCreateInfo` instance.
        */
        auto WithAudioDevice( AudioDevice* Device ) -> AssetsServiceDescription&;
    };

    /**
    * @class AssetsService
    * @brief Manages the loading, retrieval, and deletion of assets.
    *
    * The `AssetsService` class provides an interface for handling assets such as models,
    * textures, fonts, and audio files. It supports both synchronous and asynchronous
    * loading of resources, and provides functionality to manage and clean up resources.
    * This service is part of the overall service-based architecture and helps
    * to avoid reloading ready-to-use resources.
    */
    class AssetsService final : public IService<AssetsService> {
    public:
        /**
        * @brief Constructs an AssetsService with the provided configuration.
        * @param options The configuration for initializing the AssetsService.
        */
        explicit AssetsService( const AssetsServiceDescription& options );

        /**
        * @brief Initializes the AssetsService.
        * This method is called during the service initialization phase.
        */
        auto Init() -> void override;

        /**
        * @brief Shuts down the AssetsService.
        * This method is called during the service shutdown phase.
        */
        auto Shutdown() -> void override;

        /**
        * @brief Retrieves an asset of the specified type by its URI.
        *
        * This function searches for an asset associated with the given URI and attempts
        * to return it as the specified `AssetType`. If the asset is not found or cannot be
        * cast to the requested type, it returns `nullptr`.
        *
        * @tparam AssetType The type of the asset to retrieve. Must be a type derived from the base asset type.
        * @param uri A string view representing the URI or identifier of the asset.
        * @return Pointer to the asset of type `AssetType` if found and valid; otherwise, `nullptr`.
        */
        template<typename AssetType>
        MKT_NODISCARD auto GetAssetByUri( const std::string_view uri ) -> Ref<AssetType> {
            if ( m_LoadedAssets.contains( uri.data() ) ) {
                return Ref<AssetType>( As<AssetType*>( m_LoadedAssets.at( uri.data() ) ) );
            }

            return nullptr;
        }

        template<typename AssetType>
        auto LoadAsset( auto&&... args ) -> Ref<AssetType> {

            if ( const auto& typeInfo{ typeid( AssetType ) }; m_AssetLoaders.contains(typeInfo)) {
                Ref<AssetType> assetRef{
                    m_AssetLoaders.at(typeInfo)(std::forward<decltype(args)>(args)...).template As<AssetType>()
                };
            }

            return {};
        }

        // pass in description and uri
        template<typename AssetType>
        auto LoadAssetAsync( auto&&... args ) -> Task<void>* {
            auto tuple{ std::make_tuple(std::forward<decltype(args)>(args)...) };

            m_LoadTasks.emplace_back([args_tuple = std::move(tuple)]() mutable {
                std::apply( []<typename... Args>(Args&&... unpackedArgs) {
                    LoadAsset<AssetType>(std::forward<Args>(unpackedArgs)...);
                }, std::move(args_tuple));
            });

            return std::addressof( m_LoadTasks.back() );
        }

        ~AssetsService() override = default;

    private:
        // Load assets asynchronous
        auto LoadModelAsset( const ModelLoadDescription& description, const Path_T& uri ) -> RefAny;
        auto LoadTextureAsset( const TextureLoadDescription& description, const Path_T& uri ) -> RefAny;
        auto LoadAudioAsset( const AudioLoadDescription& description, const Path_T& uri ) -> RefAny;
        auto LoadFontAsset( const FontLoadDescription& description, const Path_T& uri ) -> RefAny;

    private:
        GpuDevice* m_GpuDevice{ nullptr };
        AudioDevice* m_AudioDevice{ nullptr };

        std::vector<Task<void>> m_LoadTasks{};
        ankerl::unordered_dense::map<std::string, RefAny> m_LoadedAssets{};

        using AssetLoaderFn = std::function<RefAny(AssetsService*, void*, const Path_T&)>;
        ankerl::unordered_dense::map<std::type_info, AssetLoaderFn> m_AssetLoaders{};
    };
}// namespace Mikoto


#endif//ASSETSSYSTEM_HH
