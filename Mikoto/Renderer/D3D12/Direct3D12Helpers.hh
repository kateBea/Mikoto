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

#ifndef MIKOTOROOT_DIRECT3D12HELPERS_HH
#define MIKOTOROOT_DIRECT3D12HELPERS_HH

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

// For readability
#define MKT_D3D12_NO_FLAGS 0

// D3D12 extension library.
#include <directx/d3d12.h>

#include <Renderer/Core/Rhi.hh>

namespace mikoto::renderer::d3d12 {

    using DescriptorIndex = core::u32;

    auto ThrowIfFailed(HRESULT hr) -> void;
    auto ThrowIfFailed(HRESULT hr, IGpuDevice* gpuDevice) -> void;

    MKT_NODISCARD auto GetQueueType( rhi::QueueType type ) -> D3D12_COMMAND_LIST_TYPE;

    MKT_NODISCARD auto GetShaderVisibility( rhi::ShaderStage stage ) -> D3D12_SHADER_VISIBILITY;

    MKT_NODISCARD auto GetFormat( rhi::Format format ) -> DXGI_FORMAT;
    MKT_NODISCARD auto GetFillMode( rhi::PolygonMode type ) -> D3D12_FILL_MODE;
    MKT_NODISCARD auto GetCullMode( rhi::CullMode type ) -> D3D12_CULL_MODE;
    MKT_NODISCARD auto GetDepthCompareOp( rhi::DepthCompareOp op ) -> D3D12_COMPARISON_FUNC;
    MKT_NODISCARD auto GetTopologyType( rhi::PrimitiveTopology type ) -> D3D12_PRIMITIVE_TOPOLOGY_TYPE;

    MKT_NODISCARD auto GetHeapType( rhi::HeapType type ) -> D3D12_HEAP_TYPE;
    MKT_NODISCARD auto GetDescriptorHeapType( rhi::ResourceType type ) -> D3D12_DESCRIPTOR_HEAP_TYPE;
    MKT_NODISCARD auto GetDescriptorRangeType( rhi::ResourceType type ) -> D3D12_DESCRIPTOR_RANGE_TYPE;

    MKT_NODISCARD auto IsDescriptorHeapRequired( rhi::ResourceType type ) -> bool;

    MKT_NODISCARD auto GetDimension( rhi::TextureDimension dimension ) -> D3D12_RESOURCE_DIMENSION;
    MKT_NODISCARD auto GetSampleCount( rhi::Multisampling multisampling ) -> UINT;
    MKT_NODISCARD auto GetResourceFlags( rhi::TextureUsageFlags flags ) -> D3D12_RESOURCE_FLAGS;

}// namespace mikoto::renderer::d3d12

#endif

#endif//MIKOTOROOT_DIRECT3D12HELPERS_HH
