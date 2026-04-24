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

#include <EASTL/atomic.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/unique_ptr.h>

#include <tiny_gltf.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>

#include <Assets/Model.hh>
#include <Assets/Importer.hh>

#include <Renderer/Core/GpuDevice.hh>

#include <Animation/SkinningBuilder.hh>

namespace mikoto::asset {

    struct LoaderData {
        i32 mIndex{ -1 };

        eastl::string mError{};
        eastl::string mWarning{};
        tinygltf::TinyGLTF mLoader{};

        std::atomic_bool mIsFree{ true };
    };

    class GLTFImporter final : public ModelImporter {
    public:
        explicit GLTFImporter(GpuDevice* device);

        auto Import(const ModelLoadDescription& description, ModelDataDescription& out) -> void override;

        // Extension names
        static constexpr eastl::string_view KHR_PBR_Unlit{ "KHR_materials_unlit" };
        static constexpr eastl::string_view KHR_Emissive_Strength{ "KHR_materials_emissive_strength" };
        static constexpr eastl::string_view KHR_PBR_SpecularGlossiness{ "KHR_materials_pbrSpecularGlossiness" };

    private:

        MKT_NODISCARD auto TryAcquireImporter() -> eastl::vector<eastl::unique_ptr<LoaderData>>::iterator;
        auto Import(LoaderData& loaderData,const ModelLoadDescription& description, ModelDataDescription& out) -> void;

    private:
        u32 mMaxConcurrentImporters{};
        eastl::vector<eastl::unique_ptr<LoaderData>> mImporters{};
    };
}

#endif // MIKOTO_GLTF_IMPORTER_HH
