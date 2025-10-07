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
#include <Common/Singleton.hh>
#include <Library/Data/ResourcePool.hh>
#include <Library/Utility/Types.hh>
#include <Renderer/FontService.hh>
#include <Renderer/RenderUtility.hh>
#include <Threading/Task.hh>

#include "Assets/Audio.hh"
#include "MeshFactory.hh"

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
    class AssetsService final : public IService, public Singleton<AssetsService> {
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
            if constexpr (std::is_same_v<AssetType, Model>) {
                if ( const auto it{ m_Models.find(std::string(uri)) }; it != m_Models.end())
                    return it->second;
            }
            else if constexpr (std::is_same_v<AssetType, Texture>) {
                if ( const auto it{ m_Textures.find(std::string(uri)) }; it != m_Textures.end())
                    return it->second;
            }
            else if constexpr (std::is_same_v<AssetType, Audio>) {
                if ( const auto it{ m_Audios.find(std::string(uri)) }; it != m_Audios.end())
                    return it->second;
            }
            else if constexpr (std::is_same_v<AssetType, Font>) {
                if ( const auto it{ m_Fonts.find(std::string(uri)) }; it != m_Fonts.end())
                    return it->second;
            }

            return Ref<AssetType>::CreateEmpty();
        }

        // pass in description and uri
        template<typename AssetType>
        auto LoadAsset( auto&&... args ) -> Ref<AssetType> {
            return LoadAssetTyped( std::forward<decltype(args)>(args)... );
        }

        // pass in description and uri
        template<typename AssetType>
        auto LoadAssetAsync( auto&&... args ) -> Task<void>* {
            auto tuple = std::make_tuple(std::forward<decltype(args)>(args)...);

            m_LoadTasks.emplace_back([this, args_tuple = std::move(tuple)]() mutable {
                std::apply([this]<typename... Args>(Args&&... unpackedArgs) {
                    LoadAsset<AssetType>(std::forward<Args>(unpackedArgs)...);
                }, std::move(args_tuple));
            });

            return std::addressof(m_LoadTasks.back());
        }

        ~AssetsService() override = default;

    private:
        auto LoadAssetTyped( const ModelLoadDescription& description) -> ModelHandle;
        auto LoadAssetTyped( const TextureLoadDescription& description) -> TextureHandle;
        auto LoadAssetTyped( const AudioLoadDescription& description) -> AudioHandle;
        auto LoadAssetTyped( const FontLoadDescription& description) -> FontHandle;

    private:
        Unique<MeshFactory> m_MeshFactory{};

        GpuDevice* m_GpuDevice{ nullptr };
        AudioDevice* m_AudioDevice{ nullptr };

        std::vector<Task<void>> m_LoadTasks{};

        ankerl::unordered_dense::map<std::string, ModelHandle> m_Models{};
        ankerl::unordered_dense::map<std::string, TextureHandle> m_Textures{};
        ankerl::unordered_dense::map<std::string, AudioHandle> m_Audios{};
        ankerl::unordered_dense::map<std::string, FontHandle> m_Fonts{};
    };
}// namespace Mikoto


#endif//ASSETSSYSTEM_HH
