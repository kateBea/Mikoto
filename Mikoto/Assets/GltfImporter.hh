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

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>

#include <Assets/Model.hh>
#include <Assets/Importer.hh>
#include <Renderer/Core/GpuDevice.hh>

#include <tiny_gltf.h>

namespace Mikoto {

    class GLTFImporter final : public ModelImporter {
    public:
        explicit GLTFImporter(GpuDevice* device);

        MKT_NODISCARD auto Import(const ModelLoadDescription& description) -> Model * override;

    private:
        struct LoaderData {
            tinygltf::TinyGLTF Loader{};
            std::string Err{};
            std::string Warn{};
        };

    private:
        std::vector<LoaderData> m_Loaders{};
    };
}

#endif // MIKOTO_GLTF_IMPORTER_HH
