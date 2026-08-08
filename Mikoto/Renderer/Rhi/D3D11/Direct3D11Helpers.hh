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

#ifndef MIKOTO_D3D11_HELPERS_HH
#define MIKOTO_D3D11_HELPERS_HH

#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Platform.hh>
#include <Renderer/Rhi/Types.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )

#include <d3d11.h>

// For readability
#define MKT_D3D11_NO_FLAGS 0

namespace mikoto::renderer::d3d11 {

    struct Semantic {
        eastl::string_view mName{};
        u32 mIndex{ 0 };
    };

    MKT_NODISCARD auto GetUsageFromHeapType( rhi::HeapType type ) -> D3D11_USAGE;
    MKT_NODISCARD auto GetBindFlags( rhi::BufferUsageFlags flags ) -> UINT;
    MKT_NODISCARD auto GetFormat( rhi::Format format ) -> DXGI_FORMAT;
    MKT_NODISCARD auto GetBindFlags( rhi::TextureUsageFlags flags ) -> D3D11_BIND_FLAG;

    MKT_NODISCARD auto GetPrimitiveTopology( rhi::PrimitiveTopology topology ) -> D3D11_PRIMITIVE_TOPOLOGY;

    MKT_NODISCARD auto GetCullMode( rhi::CullMode mode ) -> D3D11_CULL_MODE;
    MKT_NODISCARD auto GetComparisonFunc( rhi::DepthCompareOp op ) -> D3D11_COMPARISON_FUNC;

    MKT_NODISCARD auto GetFilter( rhi::SamplerFilter min, rhi::SamplerFilter mag ) -> D3D11_FILTER;
    MKT_NODISCARD auto GetAddressMode( rhi::SamplerWrapMode mode ) -> D3D11_TEXTURE_ADDRESS_MODE;

    MKT_NODISCARD auto GetInputElementDescription( const rhi::VertexAttributeDescription& desc, rhi::InputRate rate ) -> D3D11_INPUT_ELEMENT_DESC;

    MKT_NODISCARD auto ParseSemantic( eastl::string_view value ) -> Semantic;

}// namespace mikoto::renderer::d3d11

#endif

#endif//MIKOTO_D3D11_HELPERS_HH