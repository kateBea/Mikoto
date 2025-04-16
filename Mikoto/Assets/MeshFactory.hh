//
// Created by zanet on 3/28/2025.
//

#ifndef MESHFACTORY_HH
#define MESHFACTORY_HH

#include <atomic>

// Third Party Libraries
#include <Common/Service.hh>
#include <Library/Utility/Types.hh>
#include <assimp/Importer.hpp>

#include "Assets/Model.hh"
#include "assimp/LogStream.hpp"

namespace Mikoto {

    /**
    * @struct MeshFactoryCreateInfo
    * @brief Configuration structure for initializing a MeshFactory instance.
    *
    * The `MeshFactoryCreateInfo` structure holds configuration parameters required for setting up a `MeshFactory`,
    * including the number of importers to be allocated for processing mesh data and optional custom configurations.
    */
    struct MeshFactoryCreateInfo {
        UInt32_T ImportersCount{};
        bool UseCustomLogger{ false };
        bool UseCustomLoader{ false };

        GpuDevice* Device{ nullptr };

        /**
         * @brief Sets the number of importers for the factory.
         * @param count Number of importers.
         * @return Reference to the modified MeshFactoryCreateInfo.
         */
        auto WithImportersCount(Size_T count) -> MeshFactoryCreateInfo&;

        /**
         * @brief Enables or disables custom logging for Assimp.
         * @param enable True to enable custom logging, false to disable.
         * @return Reference to the modified MeshFactoryCreateInfo.
         */
        auto WithCustomLogger(bool enable) -> MeshFactoryCreateInfo&;

        /**
         * @brief Enables or disables a custom loader for Assimp.
         * @param enable True to enable a custom loader, false to disable.
         * @return Reference to the modified MeshFactoryCreateInfo.
         */
        auto WithCustomLoader(bool enable) -> MeshFactoryCreateInfo&;
    };

    using ModelHandle = Ref<Model>;

    /**
    * @class MeshFactory
    * @brief Manages the creation and processing of 3D models.
    *
    * The `MeshFactory` class provides functionality for loading, importing, and creating 3D models.
    * It uses Assimp for model processing and maintains a pool of importers to support multithreaded operations.
    */
    class MeshFactory final : public IService<MeshFactory> {
    public:
        /**
        * @brief Constructs a MeshFactory with the given configuration.
        * @param createInfo Configuration parameters for the mesh factory.
        */
        explicit MeshFactory(const MeshFactoryCreateInfo& createInfo);

        /**
        * @brief Initializes the mesh factory.
        */
        auto Init() -> void override;

        /**
        * @brief Shuts down the mesh factory and releases resources.
        */
        auto Shutdown() -> void override;

        /**
        * @brief Creates a model from the given load information.
        * @param loadInfo The model loading parameters.
        * @return Scoped pointer to the created Model. (Caller responsible for free)
        */
        auto CreateModel( const ModelLoadDescription& loadInfo ) -> ModelHandle;

    private:
        /**
        * @struct Logging
        * @brief Holds logging configuration for Assimp.
        *
        * This structure maintains log severity levels and custom logging implementations.
        */
        struct Logging {
            UInt32_T Severity{};
            Scope_T<Assimp::LogStream> CustomLogImpl{};
        };

        /**
        * @struct ImporterInfo
        * @brief Contains information related to Assimp importers.
        *
        * Each importer instance is assigned to a thread for safe multi-threaded operation.
        */
        struct ImporterInfo {
            Assimp::Importer MeshImporter{};
            std::atomic_bool IsFree{ true };

            Scope_T<Assimp::IOSystem> LoaderImplementation{};
            Scope_T<Assimp::IOStream> StreamImplementation{};
        };

    private:
        /**
        * @brief Sets up a custom logger for the Assimp library.
        *
        * This function configures a custom logging implementation for Assimp, allowing for better
        * debugging and monitoring of model import operations.
        */
        auto SetupCustomAssimpLogger() -> void;

        /**
         * @brief Registers and configures custom loaders for asset importers.
         *
         * This function sets up additional or custom asset loaders for the import pipeline,
         * helps integrate an Assimp loading pipeline within the Mikoto ecosystem.
         */
        auto SetupCustomLoaderForImporters() -> void;

    private:
        GpuDevice* m_Device{ nullptr };

        bool m_WantCustomLog{ false };
        bool m_WantCustomLoader{ false };

        Logging m_AssimpLogger{};
        std::vector<ImporterInfo> m_Importers{};
    };

}// namespace Mikoto
#endif//MESHFACTORY_HH
