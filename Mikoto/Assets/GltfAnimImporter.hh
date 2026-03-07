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

#ifndef MIKOTO_GLTF_IMPORTER_HH
#define MIKOTO_GLTF_IMPORTER_HH

#include <string>
#include <atomic>
#include <string_view>

#include <tiny_gltf.h>

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>

#include <Animation/SkinningBuilder.hh>

#include <Assets/Model.hh>
#include <Assets/Importer.hh>
#include <Renderer/Core/GpuDevice.hh>

namespace Mikoto {

    class GLTFImporter final : public ModelImporter {
    public:
        explicit GLTFImporter(GpuDevice* device);

        auto Import(const ModelLoadDescription& description, ModelData& out) -> void override;

    private:
        struct LoaderData {
            Int32 Index{ -1 };

            std::string Err{};
            std::string Warn{};
            tinygltf::TinyGLTF Loader{};

            std::atomic_bool IsFree{ true };
        };

    private:
        // Extension names
        static constexpr std::string_view KHR_PBR_Unlit{ "KHR_materials_unlit" };
        static constexpr std::string_view KHR_Emissive_Strength{ "KHR_materials_emissive_strength" };
        static constexpr std::string_view KHR_PBR_SpecularGlossiness{ "KHR_materials_pbrSpecularGlossiness" };

    private:

        auto LoadPrimitives(tinygltf::Model& model, ModelData& modelData) -> void;
        auto LoadMaterials(tinygltf::Model& model, ModelData& modelData, const std::string& rootPath) -> void;

        MKT_NODISCARD auto TryAcquireImporter() -> std::vector<Unique<LoaderData>>::iterator;
        auto Import(LoaderData& loaderData,const ModelLoadDescription& description, ModelData& out) -> void;

    private:
        UInt32 m_MaxConcurrentImporters{};

        std::vector<Unique<LoaderData>> m_Importers{};
    };
}

#endif // MIKOTO_GLTF_IMPORTER_HH
