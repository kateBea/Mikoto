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

#include <EASTL/atomic.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/unique_ptr.h>

#include <assimp/IOStream.hpp>
#include <assimp/IOSystem.hpp>
#include <assimp/Importer.hpp>
#include <assimp/LogStream.hpp>


#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>

#include <Assets/Model.hh>
#include <Assets/Importer.hh>

#include <Animation/SkinningBuilder.hh>
#include <Renderer/Core/GpuDevice.hh>

namespace mikoto::asset {

    class MainImporter final : public ModelImporter {
    public:
        explicit MainImporter(IGpuDevice* device);

        auto Import( const ModelLoadDescription &description, ModelDataDescription& out) -> void override;

    private:
        struct ImporterInfo {
            i32 Index{ -1 };
            Assimp::Importer MeshImporter{};
            std::atomic_bool IsFree{ true };

            eastl::unique_ptr<Assimp::IOSystem> CustomFileHandlingImpl{};

            ImporterInfo() = default;
            ~ImporterInfo() = default;

            // Assimp::Importer copy is forbidden
            ImporterInfo(const ImporterInfo&) = delete;
            ImporterInfo& operator=(const ImporterInfo&) = delete;
        };

    private:
        MKT_NODISCARD auto TryAcquireImporter() -> eastl::vector<eastl::unique_ptr<ImporterInfo>>::iterator;
        auto Import(ImporterInfo& loaderData,const ModelLoadDescription& description, ModelDataDescription& modelData) -> void;

    private:
        eastl::unique_ptr<Assimp::LogStream> m_LogImpl{};
        eastl::vector<eastl::unique_ptr<ImporterInfo>> m_Importers{};
    };
}

#endif // MIKOTO_MAIN_IMPORTER_HH
