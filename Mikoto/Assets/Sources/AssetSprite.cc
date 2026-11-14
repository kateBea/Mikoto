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

#include <Core/Exception.hh>

#include <Assets/AssetSprite.hh>

#include <Renderer/Rhi/Texture.hh>

namespace mikoto::asset {


    Sprite::Sprite( core::Ref<Texture> resource )
        : mResource{ eastl::move( resource ) } {
        if ( !mResource ) {
            MKT_THROW_RUNTIME_ERROR( "Cannot construct Sprite without a resource." );
        }
    }

    Sprite::~Sprite() = default;
    auto Sprite::GetResource() const -> core::Ref<renderer::rhi::ITexture> {
        return mResource->GetResource();
    }

    auto Sprite::GetResidentMemoryUsage() const -> AssetMemoryUsage {
        return { sizeof( *this ), 0 };
    }
}// namespace mikoto::asset
