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

#include <EASTL/unique_ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Service.hh>
#include <Core/Singleton.hh>

#include <Assets/Importer.hh>
#include <Assets/Model.hh>

namespace mikoto::asset {

    struct MeshFactoryCreateInfo {
        GpuDevice* mDevice{ nullptr };
    };

    class MeshFactory final : public Singleton<MeshFactory>, public IService {
    public:

        explicit MeshFactory(const MeshFactoryCreateInfo& createInfo);

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        auto ImportModel( const ModelLoadDescription& loadInfo ) -> ModelHandle;

    private:
        auto ConstructModel(ModelDataDescription& data, const ModelLoadDescription &loadInfo  ) -> ModelHandle;

    private:
        GpuDevice* mDevice{ nullptr };

        eastl::unique_ptr<ModelImporter> mMainImporter{};
        eastl::unique_ptr<ModelImporter> mGltfImporter{};
    };
}

#endif // MIKOTO_MESH_FACTORY_HH
