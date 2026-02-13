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

#include <Common/Service.hh>
#include <Common/Singleton.hh>
#include <Library/Utility/Types.hh>

#include <Assets/Model.hh>
#include <Assets/Importer.hh>

namespace Mikoto {

    struct MeshFactoryCreateInfo {
        GpuDevice* Device{ nullptr };
    };

    class MeshFactory final : public Singleton<MeshFactory>, public IService {
    public:

        explicit MeshFactory(const MeshFactoryCreateInfo& createInfo);

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto ImportModel( const ModelLoadDescription& loadInfo ) -> ModelHandle;

    private:
        auto ConstructModel(ModelData& data, const ModelLoadDescription &loadInfo  ) -> Model*;

    private:
        GpuDevice* m_Device{ nullptr };
        Unique<ModelImporter> m_MainImporter{};
        Unique<ModelImporter> m_GLTFImporter{};
    };
}

#endif // MIKOTO_MESH_FACTORY_HH
