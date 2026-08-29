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

    auto SkyboxMaterialDescription::SetName( eastl::string_view name ) -> SkyboxMaterialDescription & {
        mName = name;
        return *this;
    }

    auto SkyboxMaterialDescription::SetEquirectangular( renderer::rhi::TextureHandle texture ) -> SkyboxMaterialDescription & {
        mEquirectangularTextureHandle = texture;
        return *this;
    }

    auto SkyboxMaterialDescription::SetCubeFace( SkyboxFace face, renderer::rhi::TextureHandle texture ) -> SkyboxMaterialDescription & {
        mCubeFaces[face] = texture;
        return *this;
    }

    SkyboxMaterial::SkyboxMaterial(eastl::string_view name)
        : Material{ name }
    {}

    SkyboxMaterial::SkyboxMaterial( const SkyboxMaterialDescription &desc )
        : Material{ desc.mName }, mCubeFaces{ desc.mCubeFaces },
            mEquirectangular{ desc.mEquirectangularTextureHandle }, mType{ desc.mSkyboxType } {
        mCubeFaces = desc.mCubeFaces;
        mEquirectangular = desc.mEquirectangularTextureHandle;
    }

    auto SkyboxMaterial::SetEquirectangular( renderer::rhi::TextureHandle texture ) -> void {
        if (texture.IsEmpty()) {
            return;
        }

        mEquirectangular = texture;
    }

    auto SkyboxMaterial::SetFace( SkyboxFace face, renderer::rhi::TextureHandle texture ) -> void {
        mCubeFaces[face] = texture;
    }

    auto SkyboxMaterial::SetType( SkyboxType type ) -> void {
        mType = type;
    }

    auto SkyboxMaterial::IsType( SkyboxType type ) const -> bool {
        return mType == type;
    }

    auto SkyboxMaterial::GetType() const -> SkyboxType {
        return mType;
    }

    auto SkyboxMaterial::GetFace( SkyboxFace face ) -> renderer::rhi::TextureHandle {
        return mCubeFaces[face];
    }

    auto SkyboxMaterial::GetFaceTextures() -> ankerl::unordered_dense::map<SkyboxFace, renderer::rhi::TextureHandle>& {
        return mCubeFaces;
    }

    auto SkyboxMaterial::GetEquirectangular() -> renderer::rhi::TextureHandle {
        return mEquirectangular;
    }

    auto SkyboxMaterial::SetAmbientScale( core::f32 scale ) -> void {
        mAmbientScale = scale;
    }

    auto SkyboxMaterial::GetAmbientScale() const -> core::f32 {
        return mAmbientScale;
    }

}// namespace mikoto