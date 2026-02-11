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

#ifndef MIKOTO_IMPORTER_HH
#define MIKOTO_IMPORTER_HH

#include <Library/Utility/Types.hh>

#include <Assets/Model.hh>
#include <Renderer/Core/GpuDevice.hh>

namespace Mikoto {

    /**
    * @struct MeshFactoryCreateInfo
    * @brief Configuration structure for initializing a MeshFactory instance.
    *
    * The `MeshFactoryCreateInfo` structure holds configuration parameters required for setting up a `MeshFactory`,
    * including the number of importers to be allocated for processing mesh data and optional custom configurations.
    */
    struct MeshFactoryCreateInfo {
        UInt32 ImportersCount{};
        bool UseCustomLogger{ false };
        bool UseCustomLoader{ false };

        GpuDevice* Device{ nullptr };

        /**
         * @brief Sets the number of importers for the factory.
         * @param count Number of importers.
         * @return Reference to the modified MeshFactoryCreateInfo.
         */
        auto WithImportersCount(Size count) -> MeshFactoryCreateInfo&;

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

    struct ModelImporter {
    public:
        explicit ModelImporter(GpuDevice* device)
            : m_Device{ device } {}

        MKT_NODISCARD virtual auto Import(const ModelLoadDescription& description) -> Model* = 0;

        virtual ~ModelImporter() = default;
    private:
        GpuDevice* m_Device{ nullptr };
    };

}

#endif//MIKOTO_IMPORTER_HH
