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

#ifndef MIKOTO_D3D12_TEXTURE_HH
#define MIKOTO_D3D12_TEXTURE_HH

#include <Core/Platform.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )

#include <directx/d3d12.h>

#include <Renderer/Core/Rhi.hh>

namespace mikoto::renderer::d3d12 {

    class Sampler final : public rhi::ISampler {
    public:
        explicit Sampler( const rhi::SamplerCreateDescription& desc );

        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) -> rhi::Object override;

        ~Sampler() override;

    private:
        auto Release() -> void override;
        auto Initialize() -> void override;

    private:
    };

    class Texture : public ITexture {
    public:
        explicit Texture( const TextureCreateDescription& desc );

        ~Texture() override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        ID3D12Resource* mResource{};
        D3D12MA::Allocation* mAllocation{};
    };

}// namespace mikoto::renderer::d3d12

#endif

#endif//MIKOTO_D3D12_TEXTURE_HH
