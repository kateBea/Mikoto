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

#ifndef MIKOTO_RENDERER_CORE_FRAME_GRAPH_DESCRIPTOR_ALLOCATOR_HH
#define MIKOTO_RENDERER_CORE_FRAME_GRAPH_DESCRIPTOR_ALLOCATOR_HH

#include <mutex>

#include <EASTL/utility.h>
#include <EASTL/hash_map.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Rhi/Descriptor.hh>

namespace mikoto::renderer {

    enum class FGDescriptorBinding : core::u32 {
        eSampler = MKT_SAMPLER_BINDING,
        eTextureSRV = MKT_TEXTURE_SRV_BINDING,
        eTextureUAV = MKT_TEXTURE_UAV_BINDING,
        eBufferSRV = MKT_STRUCTURED_SRV_BINDING,
        eBufferUAV = MKT_STRUCTURED_UAV_BINDING,
        eAccelerationStructure = MKT_ACCELERATION_STRUCTURE_BINDING
    };

    /** Owns descriptor slots and lookup for physical resources, independently of graph IDs.
     * The frame graph supplies the bindless layout; this allocator creates its table.
     * Manager separation and descriptor lookup reference:
     * https://github.com/NVIDIA-RTX/Donut/blob/main/include/donut/engine/DescriptorTableManager.h
     */
    class FrameGraphDescriptorAllocator final : public core::INonCopyable, public core::INonMoveable {
    public:
        explicit FrameGraphDescriptorAllocator( rhi::IGpuDevice* device, rhi::BindingLayoutHandle layout );

        // Requests reuse one allocator-owned entry per physical resource and binding.
        MKT_NODISCARD auto AllocateSamplerIndex( rhi::ISampler* sampler ) -> core::u32;
        MKT_NODISCARD auto AllocateTextureIndex_SRV( rhi::ITexture* texture ) -> core::u32;
        MKT_NODISCARD auto AllocateTextureIndex_UAV( rhi::ITexture* texture ) -> core::u32;
        MKT_NODISCARD auto AllocateBufferIndex_SRV( rhi::IBuffer* buffer ) -> core::u32;
        MKT_NODISCARD auto AllocateBufferIndex_UAV( rhi::IBuffer* buffer ) -> core::u32;
        MKT_NODISCARD auto AllocateAccelerationStructureIndex( rhi::IAccelerationStructure* structure ) -> core::u32;

        /** Reports table allocation, independently of the command list's current bindings. */
        MKT_NODISCARD auto IsAllocated( FGDescriptorBinding binding, const core::Resource* resource ) const -> bool;

        /** Releases an entry once all recorded and GPU uses of its index have finished.
         * The caller owns completion tracking and must stop using cached indices first.
         * Returns false when no matching entry exists.
         */
        MKT_NODISCARD auto Release( FGDescriptorBinding binding, const core::Resource* resource ) -> bool;

        /** Releases all entries for a physical resource under the same completion contract. */
        auto ReleaseResource( const core::Resource* resource ) -> void;

        MKT_NODISCARD auto GetBindlessLayout() const -> rhi::BindingLayoutHandle;
        MKT_NODISCARD auto GetDescriptorTable() const -> rhi::DescriptorTableHandle;

        // Submitted command lists retain the table and its remaining resource references.
        ~FrameGraphDescriptorAllocator() = default;

    private:
        auto AllocateDescriptorIndex( const rhi::BindingTableItem& item ) -> core::u32;

        using DescriptorIndices = eastl::hash_map<const core::Resource*, core::u32>;

        rhi::IGpuDevice* mDevice{};
        rhi::BindingLayoutHandle mBindlessLayout{};
        rhi::DescriptorTableHandle mDescriptorTable{};

        mutable std::mutex mMutex{};

        // Whole-resource views only; the table retains the physical identities used as keys.
        eastl::hash_map<core::u32, DescriptorIndices> mDescriptorIndices{};
    };
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_CORE_FRAME_GRAPH_DESCRIPTOR_ALLOCATOR_HH
