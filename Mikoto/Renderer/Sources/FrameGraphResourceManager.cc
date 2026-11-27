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


#include <EASTL/algorithm.h>

#include <Core/Exception.hh>
#include <Core/Profiler.hh>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

#include <Renderer/Core/FrameGraphResourceManager.hh>


namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    FGResourceManager::FGResourceManager( renderer::rhi::IGpuDevice *device )
        : mDevice{ device } {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( mDevice, "Resource manager device cannot be null" );
    }

    auto FGResourceManager::Get( FGResourceHandle handle ) -> FGResource & {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( mResources.contains( handle ), "Resource with ID does not exist" );
        return *mResources.at( handle );
    }

    auto FGResourceManager::Get( FGResourceHandle handle ) const -> const FGResource & {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( mResources.contains( handle ), "Resource with ID does not exist" );
        return *mResources.at( handle );
    }

    auto FGResourceManager::GetBufferMappedAddress( FGBufferHandle handle ) const -> const void * {
        MKT_BEGIN_PROFILER();
        auto &resource{ Get( handle.mHandle ) };
        renderer::rhi::BufferHandle buffer{ resource.mResource };
        return mDevice->Map( buffer.GetPtr() );// Maps whole buffer, not parts of it
    }

    auto FGResourceManager::Allocate( FGResourceType type, core::Resource *resource ) -> FGResource & {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mResourceMutex };

        const auto it{ mImportedResources.find( resource ) };

        if ( it != mImportedResources.end() ) {
            return *mResources[it->second];
        }

        auto id{ ++mResourceCount };

        if ( resource ) {
            mImportedResources[resource] = id;
        }

        auto &result{ mResources[id] = eastl::make_unique<FGResource>(
                          FGResource{
                              .mResourceID = id,
                              .mType = type,
                              .mNeedsRebuild = resource == nullptr,
                          } ) };

        return *result;
    }

    auto FGResourceManager::SetResource( FGResourceHandle handle, core::Ref<core::Resource> resource ) -> void {
        const std::lock_guard lock{ mResourceMutex };
        auto &entry{ *mResources.at( handle ) };

        if ( entry.mType == FGResourceType::eBuffer && entry.mResource.GetPtr() != resource.GetPtr() ) {
            ++mBufferRevision;
        }

        if ( entry.mResource && entry.mResource.GetPtr() != resource.GetPtr() ) {
            mReplacedResources.push_back( entry.mResource );
        }

        entry.mResource = eastl::move( resource );
        entry.mBarrierHistory = {};
        entry.mNeedsRebuild = false;
    }

    auto FGResourceManager::IsResourceReferenced( const core::Resource *resource ) const -> bool {
        const std::lock_guard lock{ mResourceMutex };
        for ( const auto &[handle, entry]: mResources ) {
            if ( entry->mResource.GetPtr() == resource ) {
                return true;
            }
        }

        return false;
    }

    auto FGResourceManager::GetReplacedResources() -> eastl::vector<core::Ref<core::Resource>> {
        const std::lock_guard lock{ mResourceMutex };
        eastl::vector<core::Ref<core::Resource>> resources{};
        resources.swap( mReplacedResources );
        return resources;
    }

    auto FGResourceManager::Free( FGResourceHandle handle ) -> bool {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mResourceMutex };
        const auto found{ mResources.find( handle ) };

        if ( found == mResources.end() ) {
            return false;
        }

        if ( found->second->mResource ) {
            mReplacedResources.push_back( found->second->mResource );
        }

        mImportedResources.erase( found->second->mResource.GetPtr() );

        // Descriptor lifetime is managed separately from this virtual resource record.
        mResources.erase( found );
        return true;
    }

    auto FGResourceManager::GetBufferRevision() const -> core::u64 {
        return mBufferRevision;
    }

    auto FGResourceManager::ReplaceBufferStorage( FGBufferHandle handle, core::usize bytes ) -> void {
        auto &resource{ Get( handle.mHandle ) };
        renderer::rhi::BufferHandle old{ resource.mResource };

        if ( old->GetHeapType() != renderer::rhi::HeapType::eDeviceLocal ) {
            MKT_THROW_RUNTIME_ERROR( "Only device-local graph buffers support storage replacement." );
        }

        // Keep a small minimum allocation because zero-sized RHI buffers are
        // invalid. Capacity is otherwise tied to the live payload, not its peak.
        auto replacement{ mDevice->CreateBuffer( renderer::rhi::BufferCreateDescription{}
            .SetName( old->GetDebugName() )
            .SetBufferUsage( old->GetUsage() |
                                 renderer::rhi::BufferUsageFlagsBits::CopySource |
                                 renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetHeapType( renderer::rhi::HeapType::eDeviceLocal )
            .SetByteSize( eastl::max<core::usize>( bytes, 65536 ) ) ) };

        mReplacedResources.push_back( resource.mResource );
        resource.mResource = replacement;
        resource.mBarrierHistory = {};

        ++mBufferRevision;
    }

    auto FGResourceManager::GrowBuffer( FGBufferHandle handle, core::usize bytes ) -> bool {
        auto &resource{ Get( handle.mHandle ) };
        renderer::rhi::BufferHandle old{ resource.mResource };

        if ( bytes <= old->GetSizeBytes() ) {
            return false;
        }

        if ( old->GetHeapType() != renderer::rhi::HeapType::eDeviceLocal ) {
            MKT_THROW_RUNTIME_ERROR( "Only device-local graph buffers support growth." );
        }

        core::usize capacity{ eastl::max<core::usize>( old->GetSizeBytes(), 65536 ) };

        while ( capacity < bytes ) {
            if ( capacity > eastl::numeric_limits<core::usize>::max() / 2 ) {
                MKT_THROW_RUNTIME_ERROR( "Graph buffer capacity overflow." );
            }

            capacity *= 2;
        }

        ReplaceBufferStorage( handle, capacity );

        return true;
    }

    auto FGResourceManager::GetGpuDeviceAddress( FGBufferHandle handle ) -> core::u64 {
        MKT_BEGIN_PROFILER();

        if ( !mResources.contains( handle.mHandle ) ) {
            return 0;
        }

        renderer::rhi::IBuffer *buffer{ core::checked_cast<renderer::rhi::IBuffer *>( mResources.at( handle.mHandle )->mResource.GetPtr() ) };

        MKT_ASSERT( buffer, "Invalid cast of resource to Buffer type" );

        return buffer->GetShaderDeviceAddress();
    }

    auto FGResourceManager::ImportSampledTexture( renderer::rhi::TextureHandle texture ) -> FGTextureHandle {
        MKT_BEGIN_PROFILER();

        if ( !texture ) {
            MKT_THROW_RUNTIME_ERROR( "Cannot import an empty sampled texture." );
        }

        return ImportTexture( eastl::move( texture ) );
    }

    auto FGResourceManager::ImportTexture( renderer::rhi::TextureHandle handle ) -> FGTextureHandle {
        MKT_BEGIN_PROFILER();
        auto &resource{ Allocate( FGResourceType::eTexture, handle.GetPtr() ) };

        resource.mResource = handle;

        return FGTextureHandle{ .mHandle = resource.mResourceID };
    }

    auto FGResourceManager::ImportBuffer( renderer::rhi::BufferHandle handle ) -> FGBufferHandle {
        MKT_BEGIN_PROFILER();
        auto &resource{ Allocate( FGResourceType::eBuffer, handle.GetPtr() ) };

        resource.mResource = handle;

        return FGBufferHandle{ .mHandle = resource.mResourceID };
    }
}// namespace mikoto::renderer
