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

#ifndef MIKOTO_ASSETS_MESH_FACTORY_HH
#define MIKOTO_ASSETS_MESH_FACTORY_HH

#include <EASTL/unique_ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Service.hh>
#include <Core/Singleton.hh>

#include <Assets/Importer.hh>
#include <Assets/AssetModel.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>

namespace mikoto::asset {

    struct MeshFactoryCreateInfo {
        renderer::rhi::IGpuDevice* mDevice{ nullptr };
    };

    class MeshFactory final : public core::Singleton<MeshFactory>, public core::IService {
    public:

        explicit MeshFactory(const MeshFactoryCreateInfo& createInfo);

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        auto ImportModel( const ModelLoadDescription& loadInfo ) -> ModelHandle;

    private:
        auto ConstructModel(ModelDataDescription& data, const ModelLoadDescription &loadInfo  ) -> ModelHandle;

    private:
        renderer::rhi::IGpuDevice* mDevice{ nullptr };

        eastl::unique_ptr<ModelImporter> mAssimpImporter{};
        eastl::unique_ptr<ModelImporter> mGltfImporter{};
    };
}

#endif// MIKOTO_ASSETS_MESH_FACTORY_HH
