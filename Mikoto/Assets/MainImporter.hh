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

#ifndef MIKOTO_MAIN_IMPORTER_HH
#define MIKOTO_MAIN_IMPORTER_HH

#include <string>
#include <atomic>
#include <vector>

#include <assimp/IOStream.hpp>
#include <assimp/IOSystem.hpp>
#include <assimp/LogStream.hpp>
#include <assimp/Importer.hpp>

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>

#include <Assets/Model.hh>
#include <Assets/Importer.hh>
#include <Renderer/Core/GpuDevice.hh>


namespace Mikoto {

    // Uses Assimp
    class MainImporter final : public ModelImporter {
    public:
        explicit MainImporter(GpuDevice* device);

        MKT_NODISCARD auto Import( const ModelLoadDescription &description ) -> ModelData* override;

    private:
        struct ImporterInfo {
            Int32 Index{ -1 };
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
        MKT_NODISCARD auto TryAcquireImporter() -> std::vector<Unique<ImporterInfo>>::iterator;
        MKT_NODISCARD auto Import(ImporterInfo& loaderData,const ModelLoadDescription& description) -> ModelData*;

    private:
        std::vector<Unique<ImporterInfo>> m_Importers{};
    };
}

#endif // MIKOTO_MAIN_IMPORTER_HH
