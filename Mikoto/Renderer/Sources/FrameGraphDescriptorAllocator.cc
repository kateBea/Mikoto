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

#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Logging/Assert.hh>

#include <Renderer/Core/FrameGraphDescriptorAllocator.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    FrameGraphDescriptorAllocator::FrameGraphDescriptorAllocator( rhi::IGpuDevice* device, rhi::BindingLayoutHandle layout )
        : mDevice{ device }, mBindlessLayout{ eastl::move( layout ) } {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( mDevice, "Descriptor allocator device cannot be null" );
        MKT_ASSERT( mBindlessLayout, "Descriptor allocator layout cannot be empty" );

        mDescriptorTable = mDevice->CreateDescriptorTable( mBindlessLayout );
        mDescriptorTable->SetDebugName( "FrameGraph Resource Table" );
    }

    auto FrameGraphDescriptorAllocator::AllocateDescriptorIndex( const rhi::BindingTableItem& item ) -> core::u32 {
        MKT_BEGIN_PROFILER();
        const std::lock_guard lock{ mMutex };

        // Replaced storage gets a new entry, preserving indices in earlier recordings.
        auto& indices{ mDescriptorIndices[item.mBindingIndex] };

        if ( const auto found{ indices.find( item.mResource ) }; found != indices.end() ) {
            return found->second;
        }

        const auto index{ mDevice->WriteDescriptorTable( mDescriptorTable, item ) };

        if ( index == rhi::kInvalidBindingItemIndex ) {
            MKT_THROW_RUNTIME_ERROR( "Frame graph descriptor allocation failed." );
        }

        const core::u32 shaderIndex{ core::as<core::u32>( index ) };
        indices.emplace( item.mResource, shaderIndex );
        return shaderIndex;
    }

    auto FrameGraphDescriptorAllocator::AllocateSamplerIndex( rhi::ISampler* sampler ) -> core::u32 {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( sampler, "Sampler cannot be null" );

        return AllocateDescriptorIndex( rhi::BindingTableItem::Sampler( MKT_SAMPLER_BINDING, sampler ) );
    }

    auto FrameGraphDescriptorAllocator::AllocateTextureIndex_SRV( rhi::ITexture* texture ) -> core::u32 {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( texture, "Texture cannot be null" );

        return AllocateDescriptorIndex( rhi::BindingTableItem::TextureSRV( MKT_TEXTURE_SRV_BINDING,
            texture, texture->GetFormat(), rhi::kAllSubResources, texture->GetDimension() ) );
    }

    auto FrameGraphDescriptorAllocator::AllocateTextureIndex_UAV( rhi::ITexture* texture ) -> core::u32 {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( texture, "Texture cannot be null" );

        return AllocateDescriptorIndex( rhi::BindingTableItem::TextureUAV( MKT_TEXTURE_UAV_BINDING,
            texture, texture->GetFormat(), rhi::kAllSubResources, texture->GetDimension() ) );
    }

    auto FrameGraphDescriptorAllocator::AllocateBufferIndex_SRV( rhi::IBuffer* buffer ) -> core::u32 {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( buffer, "Buffer cannot be null" );

        return AllocateDescriptorIndex( rhi::BindingTableItem::RawBufferSRV( MKT_STRUCTURED_SRV_BINDING, buffer ) );
    }

    auto FrameGraphDescriptorAllocator::AllocateBufferIndex_UAV( rhi::IBuffer* buffer ) -> core::u32 {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( buffer, "Buffer cannot be null" );

        return AllocateDescriptorIndex( rhi::BindingTableItem::RawBufferUAV( MKT_STRUCTURED_UAV_BINDING, buffer ) );
    }

    auto FrameGraphDescriptorAllocator::AllocateAccelerationStructureIndex( rhi::IAccelerationStructure* structure ) -> core::u32 {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( structure, "Acceleration structure cannot be null" );

        return AllocateDescriptorIndex( rhi::BindingTableItem::AccelerationStructure( MKT_ACCELERATION_STRUCTURE_BINDING, structure ) );
    }

    auto FrameGraphDescriptorAllocator::IsAllocated( FGDescriptorBinding binding, const core::Resource* resource ) const -> bool {
        MKT_BEGIN_PROFILER();
        const std::lock_guard lock{ mMutex };
        const auto group{ mDescriptorIndices.find( core::as<core::u32>( binding ) ) };

        return group != mDescriptorIndices.end() && group->second.find( resource ) != group->second.end();
    }

    auto FrameGraphDescriptorAllocator::Release( FGDescriptorBinding binding, const core::Resource* resource ) -> bool {
        MKT_BEGIN_PROFILER();
        const std::lock_guard lock{ mMutex };
        const core::u32 bindingIndex{ core::as<core::u32>( binding ) };
        const auto group{ mDescriptorIndices.find( bindingIndex ) };

        if ( group == mDescriptorIndices.end() ) {
            return false;
        }

        auto& indices{ group->second };
        const auto entry{ indices.find( resource ) };

        if ( entry == indices.end() ) {
            return false;
        }

        mDevice->ReleaseDescriptorTableIndex( mDescriptorTable, bindingIndex, entry->second );
        indices.erase( entry );

        if ( indices.empty() ) {
            mDescriptorIndices.erase( group );
        }

        return true;
    }

    auto FrameGraphDescriptorAllocator::ReleaseResource( const core::Resource* resource ) -> void {
        MKT_BEGIN_PROFILER();
        const std::lock_guard lock{ mMutex };

        for ( auto group{ mDescriptorIndices.begin() }; group != mDescriptorIndices.end(); ) {
            auto& indices{ group->second };

            if ( const auto entry{ indices.find( resource ) }; entry != indices.end() ) {
                mDevice->ReleaseDescriptorTableIndex( mDescriptorTable, group->first, entry->second );
                indices.erase( entry );
            }

            if ( indices.empty() ) {
                group = mDescriptorIndices.erase( group );
            }
            else {
                ++group;
            }
        }
    }

    auto FrameGraphDescriptorAllocator::GetBindlessLayout() const -> rhi::BindingLayoutHandle {
        MKT_BEGIN_PROFILER();
        return mBindlessLayout;
    }

    auto FrameGraphDescriptorAllocator::GetDescriptorTable() const -> rhi::DescriptorTableHandle {
        MKT_BEGIN_PROFILER();
        return mDescriptorTable;
    }
}// namespace mikoto::renderer
