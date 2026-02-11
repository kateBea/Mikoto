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

#ifndef MIKOTO_MESH_FACTORY_HH
#define MIKOTO_MESH_FACTORY_HH

#include <atomic>

#include <assimp/IOStream.hpp>
#include <assimp/IOSystem.hpp>
#include <assimp/LogStream.hpp>
#include <assimp/Importer.hpp>

#include <Common/Service.hh>
#include <Common/Singleton.hh>
#include <Library/Utility/Types.hh>

#include <Assets/Model.hh>
#include <Assets/Importer.hh>

namespace Mikoto {

    /**
    * @class MeshFactory
    * @brief Manages the creation and processing of 3D models.
    *
    * The `MeshFactory` class provides functionality for loading, importing, and creating 3D models.
    * It uses Assimp for model processing and maintains a pool of importers to support multithreaded operations.
    */
    class MeshFactory final : public Singleton<MeshFactory> {
    public:
        /**
        * @brief Constructs a MeshFactory with the given configuration.
        * @param createInfo Configuration parameters for the mesh factory.
        */
        explicit MeshFactory(const MeshFactoryCreateInfo& createInfo);

        /**
        * @brief Initializes the mesh factory.
        */
        auto Init() -> void;

        /**
        * @brief Shuts down the mesh factory and releases resources.
        */
        auto Shutdown() -> void;

        /**
        * @brief Creates a model from the given load information.
        * @param loadInfo The model loading parameters.
        * @return Scoped pointer to the created Model. (Caller responsible for free)
        */
        auto ImportModel( const ModelLoadDescription& loadInfo ) -> ModelHandle;

    private:
        /**
        * @struct Logging
        * @brief Holds logging configuration for Assimp.
        *
        * This structure maintains log severity levels and custom logging implementations.
        */
        struct Logging {
            UInt32 Severity{};
            Assimp::LogStream* CustomLogImpl{};
        };

        /**
        * @struct ImporterInfo
        * @brief Contains information related to Assimp importers.
        *
        * Each importer instance is assigned to a thread.
        */
        struct ImporterInfo {
            Assimp::Importer MeshImporter{};
            std::atomic_bool IsFree{ true };

            Unique<Assimp::IOSystem> CustomFileHandlingImpl{};

            ImporterInfo() = default;
            ~ImporterInfo() = default;

            // Assimp::Importer copy is forbidden
            ImporterInfo(const ImporterInfo&) = delete;
            ImporterInfo& operator=(const ImporterInfo&) = delete;
        };

    private:
        // [Internal]
        auto SetupCustomAssimpLogger() -> void;

        static auto ImportModel(GpuDevice* device, Assimp::Importer& importer, const ModelLoadDescription& loadInfo ) -> Model*;

    private:
        GpuDevice* m_Device{ nullptr };

        Unique<Assimp::LogStream> m_CustomLoggingImpl{};

        bool m_IsInitialized{ false };

        bool m_WantCustomLog{ false };

        Logging m_AssimpLogger{};
        std::vector<Unique<ImporterInfo>> m_Importers{};

        Unique<ModelImporter> m_MainImporter{};
        Unique<ModelImporter> m_GLTFImporter{};
    };

}

#endif // MIKOTO_MESH_FACTORY_HH
