//    Copyright 2025 ケイト
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

#include <Material/SkyboxMaterial.hh>

namespace mikoto::material {

    SkyboxMaterial::SkyboxMaterial(eastl::string_view name)
        : Material{ name }
    {}

    SkyboxMaterial::SkyboxMaterial( const SkyboxMaterialDescription &desc )
        : Material{ desc.mName }
    {}

    auto SkyboxMaterial::SetFace( SkyboxFace face, renderer::rhi::TextureHandle texture ) -> void {
        mCubeFaces[face] = texture;
    }

    auto SkyboxMaterial::IsType( SkyboxType type ) const -> bool {
        return false;
    }

    auto SkyboxMaterial::GetFace( SkyboxFace face ) -> renderer::rhi::TextureHandle {
        return mCubeFaces[face];
    }

    auto SkyboxMaterial::GetEquirectangular() -> renderer::rhi::TextureHandle {
        return mEquirectangular;
    }

}// namespace mikoto