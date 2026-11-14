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

#include <EASTL/numeric_limits.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Logging/Assert.hh>

#include <Renderer/Rhi/Vulkan/VulkanBuffer.hh>
#include <Renderer/Rhi/Vulkan/VulkanDevice.hh>
#include <Renderer/Rhi/Vulkan/VulkanHelpers.hh>

namespace mikoto::renderer::vulkan {

    using namespace mikoto::core;
    using namespace mikoto::memory;
    using namespace mikoto::renderer::rhi;

    Buffer::operator VkBuffer() const {
        MKT_BEGIN_PROFILER();
        return mAllocation.mBuffer;
    }

    Buffer::Buffer( const renderer::rhi::BufferCreateDescription& createInfo )
        : mUploadContents{ createInfo.mSpanHandle },
          mElementCount{ createInfo.mElementCount },
          mElementSize{ createInfo.mElementSize },
          mDataType{ createInfo.mDataType },
          mUsage{ createInfo.mUsageFlags },
          mInitialState{ createInfo.mInitialState },
          mIsVolatile{ createInfo.mIsVolatile },
          mMaxVersions{ createInfo.mMaxVersions },
          mFormat{ createInfo.mFormat },
          mKeepInitializerResources{ createInfo.mKeepInitializerResources } {
        mHeapType = createInfo.mHeapType;
        mResourceType = createInfo.mResourceType;
        mAllocation.mAllocationCreateInfo.priority = 1.0f;
        mAllocation.mAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

        mAllocation.mBufferCreateInfo = initializers::BufferCreateInfo();

        // Perf. Warn: vkBindBufferMemory(): Trying to bind VkBuffer 0x2780000000278 to a memory block which is
        // fully consumed by the buffer. The required size of the allocation is 2880, but smaller buffers like
        // this should be sub-allocated from larger memory blocks. (Current threshold is 1048576 bytes)
        mAllocation.mBufferCreateInfo.size = mElementCount == 0 ? mElementSize : mElementCount * mElementSize;

        switch ( mHeapType ) {
            case renderer::rhi::HeapType::eDeviceLocal: {
                mAllocation.mAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

                constexpr VkDeviceSize threshold{ MKT_MIBIBYTES( 64 ) };

                if ( mAllocation.mBufferCreateInfo.size >= threshold ) {
                    mAllocation.mAllocationCreateInfo.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
                }

                break;
            }

            case renderer::rhi::HeapType::eUpload:
                mAllocation.mBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

                mAllocation.mAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

                mAllocation.mAllocationCreateInfo.flags =
                    VMA_ALLOCATION_CREATE_MAPPED_BIT |
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
                break;
            case renderer::rhi::HeapType::eReadback:
                mAllocation.mBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

                mAllocation.mAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

                mAllocation.mAllocationCreateInfo.flags =
                    VMA_ALLOCATION_CREATE_MAPPED_BIT |
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
                break;
        }

        // https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/enabling_buffer_device_address.html
        // The library automatically adds VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT* to allocated memory blocks wherever it might be needed
        mAllocation.mBufferCreateInfo.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

        // https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdBuildAccelerationStructuresKHR.html
        if ( mUsage.Has( renderer::rhi::BufferUsageFlagsBits::AccelStructureBuildInput ) ) {
            mAllocation.mBufferCreateInfo.usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;

            // TLAS instance arrays require a 16-byte aligned device address.
            mAllocation.mAllocationCreateInfo.minAlignment = 16;
        }

        if ( mUsage.Has( renderer::rhi::BufferUsageFlagsBits::AccelStructureStorage ) ) {
            mAllocation.mBufferCreateInfo.usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
        }

        if ( mUsage.Has( renderer::rhi::BufferUsageFlagsBits::AccelStructureScratch ) ) {
            mAllocation.mBufferCreateInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        }

        if ( mUsage.Has( renderer::rhi::BufferUsageFlagsBits::ShaderBindingTable ) ) {
            mAllocation.mBufferCreateInfo.usage |= VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;
        }

        if ( mUsage.Has( BufferUsageFlagsBits::VideoDecodeSource ) ) {
            mAllocation.mBufferCreateInfo.usage |= VK_BUFFER_USAGE_VIDEO_DECODE_SRC_BIT_KHR;
        }

        if ( mUsage.Has( renderer::rhi::BufferUsageFlagsBits::Vertex ) ) {
            mAllocation.mBufferCreateInfo.usage |=
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        }

        if ( mUsage.Has( renderer::rhi::BufferUsageFlagsBits::Index ) ) {
            mAllocation.mBufferCreateInfo.usage |=
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        }

        if ( mUsage.Has( renderer::rhi::BufferUsageFlagsBits::Structured ) ) {
            mAllocation.mBufferCreateInfo.usage |=
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        }

        if ( mUsage.Has( renderer::rhi::BufferUsageFlagsBits::Constant ) ) {
            mAllocation.mBufferCreateInfo.usage |=
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        }

        if ( mUsage.Has( renderer::rhi::BufferUsageFlagsBits::IndirectDraw ) ) {
            // Mark as storage because it can be used to read and write from compute shaders
            mAllocation.mBufferCreateInfo.usage |=
                VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
        }

        // Explicit copy flags (this is what you were missing)
        if ( mUsage.Has( renderer::rhi::BufferUsageFlagsBits::CopySource ) ) {
            mAllocation.mBufferCreateInfo.usage |=
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        }

        if ( mUsage.Has( renderer::rhi::BufferUsageFlagsBits::CopyDest ) ) {
            mAllocation.mBufferCreateInfo.usage |=
                VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        }

        // Look into VkUpdateBuffer when buffer is less than 64kb
    }

    auto Buffer::GetShaderDeviceAddress() -> rhi::DeviceAddress {
        return GetGpuDeviceAddress();
    }

    auto Buffer::GetGpuDeviceAddress() -> rhi::DeviceAddress {
        MKT_BEGIN_PROFILER();
        Device* device{ core::checked_cast<Device*>( mDevice ) };

        if ( !device ) {
            return renderer::rhi::IBuffer::kNullDeviceAddress;
        }

        VkBufferDeviceAddressInfo addressInfo{};
        addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        addressInfo.pNext = nullptr;
        addressInfo.buffer = mAllocation.mBuffer;
        return core::as<renderer::rhi::DeviceAddress>( vkGetBufferDeviceAddress( device->GetDevice(), MKT_ADDRESSOF( addressInfo ) ) );
    }

    auto Buffer::InvalidateHostReads() -> void {
        MKT_BEGIN_PROFILER();

        if ( mHeapType == renderer::rhi::HeapType::eReadback ) {
            core::checked_cast<Device*>( mDevice )->GetAllocator()->InvalidateBuffer( mAllocation );
        }
    }

    auto Buffer::FlushHostWrites( core::usize offset, core::usize size ) -> void {
        MKT_BEGIN_PROFILER();
        core::checked_cast<Device*>( mDevice )->GetAllocator()->FlushBuffer( mAllocation, offset, size );
    }

    Buffer::~Buffer() {
        MKT_BEGIN_PROFILER();

        if ( mAllocation.mAllocation ) {
            Destroy();
        }
    }

    auto Buffer::Destroy() -> void {
        MKT_BEGIN_PROFILER();
        auto* allocator{ core::checked_cast<Device*>( mDevice )->GetAllocator() };

        if ( mAllocation.mBuffer != VK_NULL_HANDLE ) {
            if ( mAllocation.mAllocationInfo.pMappedData && !( mAllocation.mAllocationCreateInfo.flags & VMA_ALLOCATION_CREATE_MAPPED_BIT ) ) {
                allocator->UnmapBuffer( mAllocation );
            }
        }

        allocator->FreeBuffer( mAllocation );
        mAllocation = {};

        mIsAllocated = false;
    }

    auto Buffer::Initialize( IGpuDevice* device, const VkVideoProfileInfoKHR& profile ) -> void {
        // Video buffers must declare their codec profile when VMA creates the native buffer.
        // https://docs.vulkan.org/refpages/latest/refpages/source/VkBufferCreateInfo.html#VUID-VkBufferCreateInfo-usage-04813
        auto profiles{ initializers::VideoProfileListInfoKHR() };
        profiles.profileCount = 1;
        profiles.pProfiles = &profile;
        mAllocation.mBufferCreateInfo.pNext = &profiles;
        DeviceObject::Initialize( device );
        mAllocation.mBufferCreateInfo.pNext = nullptr;
    }

    auto Buffer::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( mAllocation.mBufferCreateInfo.usage != MKT_VK_FLAGS_NONE, "Buffer usage must be specified" );
        MKT_ASSERT( mAllocation.mBufferCreateInfo.size != 0, "Buffer size must be different than 0" );

        auto* allocator{ core::checked_cast<Device*>( mDevice )->GetAllocator() };
        MKT_VK_CHECK( allocator->AllocateBuffer( mAllocation ) );

        allocator->SetAllocationName( mAllocation.mAllocation, mDebugName.c_str() );

        if ( !mUploadContents.IsEmpty() ) {
            core::u64 fenceValue{ 0 };
            renderer::rhi::FenceHandle fence{ mDevice->CreateFence( fenceValue++ ) };
            renderer::rhi::CommandListHandle cmd{ mDevice->CreateCommandList( renderer::rhi::QueueType::eTransfer ) };

            cmd->Open( { .mScopeName = string::Format( "Buffer upload: {}", mDebugName ) } );
            cmd->Write( this, mUploadContents->GetData(), mUploadContents->GetSize() );

            if ( mInitialState != renderer::rhi::ResourceStates::eUnknown ) {
                cmd->SetTransition( { this, mInitialState } );
            }

            cmd->Close();

            // Signal fenceValue on one fence on completion of these
            // commands, then we wait for that completion this blocks the caller
            // but client should ideally offload this task to worker threads
            const auto submitInfo{ renderer::rhi::SubmitInfo{}
                .AddSignal( fence, fenceValue )
                .AddCommandList( cmd ) };
            mDevice->GetQueue( renderer::rhi::QueueType::eTransfer )->ExecuteCommandLists( submitInfo );
            ( void )fence->Wait( fenceValue, eastl::numeric_limits<core::u64>::max() );
        } else if ( mInitialState != renderer::rhi::ResourceStates::eUnknown ) {
            // Just transition state if needed, otherwise leave in undefined because that is
            // the state in which resources are created
            core::u64 fenceValue{ 0 };
            renderer::rhi::FenceHandle fence{ mDevice->CreateFence( fenceValue++ ) };
            renderer::rhi::CommandListHandle cmd{ mDevice->CreateCommandList( renderer::rhi::QueueType::eTransfer ) };

            cmd->Open( { .mScopeName = string::Format( "Buffer Transition: {}", mDebugName ) } );
            cmd->SetTransition( { this, mInitialState } );
            cmd->Close();

            // Signal fenceValue on one fence on completion of these
            // commands, then we wait for that completion this blocks the caller
            // but client should ideally offload this task to worker threads
            const auto submitInfo{ renderer::rhi::SubmitInfo{}
                .AddSignal( fence, fenceValue )
                .AddCommandList( cmd ) };
            mDevice->GetQueue( renderer::rhi::QueueType::eTransfer )->ExecuteCommandLists( submitInfo );
            ( void )fence->Wait( fenceValue, eastl::numeric_limits<core::u64>::max() );
        }

        if ( !mKeepInitializerResources ) {
            mUploadContents.Reset();
        }

        mIsAllocated = true;
    }

    auto Buffer::GetNativeHandle( renderer::rhi::ObjectType type ) -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        switch ( type ) {
            case renderer::rhi::ObjectType::Vk_Buffer:
                return renderer::rhi::Object( mAllocation.mBuffer );
            default:;
        }

        return renderer::rhi::Object( nullptr );
    }

    auto Buffer::GetNativeHandle( renderer::rhi::ObjectType type ) const -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        switch ( type ) {
            case renderer::rhi::ObjectType::Vk_Buffer:
                return renderer::rhi::Object( mAllocation.mBuffer );
            default:;
        }

        return renderer::rhi::Object( nullptr );
    }

    auto Buffer::IsMapped() const -> bool {
        MKT_BEGIN_PROFILER();
        return mAllocation.mAllocationInfo.pMappedData != nullptr;
    }

    auto Buffer::GetMappedAddress() -> void* {
        MKT_BEGIN_PROFILER();
        return mAllocation.mAllocationInfo.pMappedData;
    }

    auto Buffer::GetMappedAddress() const -> const void* {
        MKT_BEGIN_PROFILER();
        return mAllocation.mAllocationInfo.pMappedData;
    }


    auto Buffer::PersistentMap() -> void {
        MKT_BEGIN_PROFILER();

        if ( mAllocation.mBuffer == VK_NULL_HANDLE || mAllocation.mAllocationInfo.pMappedData != nullptr ) {
            return;
        }

        auto allocator{ core::checked_cast<Device*>( mDevice )->GetAllocator() };
        allocator->MapBuffer( mAllocation );
    }

    auto Buffer::PersistentUnmap() -> void {
        MKT_BEGIN_PROFILER();

        if ( mAllocation.mBuffer == VK_NULL_HANDLE || mAllocation.mAllocationInfo.pMappedData == nullptr ) {
            return;
        }

        // Publish upload writes even when VMA keeps the allocation persistently mapped.
        // https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/memory_mapping.html
        if ( mHeapType == HeapType::eUpload ) {
            FlushHostWrites( 0, GetSizeBytes() );
        }

        if ( mAllocation.mAllocationCreateInfo.flags & VMA_ALLOCATION_CREATE_MAPPED_BIT ) {
            return;
        }

        auto allocator{ core::checked_cast<Device*>( mDevice )->GetAllocator() };
        allocator->UnmapBuffer( mAllocation );

        mAllocation.mAllocationInfo.pMappedData = nullptr;
    }

    auto Buffer::SetDebugName( eastl::string_view name ) -> void {
        MKT_BEGIN_PROFILER();

        if ( name.empty() || !mDevice ) {
            return;
        }

        mDebugName = name;

        auto* device{ core::checked_cast<Device*>( mDevice ) };
        device->SetDebugName( VK_OBJECT_TYPE_BUFFER, core::rc_cast<core::u64>( mAllocation.mBuffer ), mDebugName );

        if ( mAllocation.mAllocation ) {
            auto* allocator{ core::checked_cast<Device*>( mDevice )->GetAllocator() };
            allocator->SetAllocationName( mAllocation.mAllocation, mDebugName.c_str() );
        }
    }

    auto Buffer::GetUsage() const -> rhi::BufferUsageFlags {
        return mUsage;
    }


    auto Buffer::GetDataType() const -> rhi::BufferDataType {
        return mDataType;
    }


    auto Buffer::GetData() const -> memory::BufferSpanHandle {
        return mUploadContents;
    }


    auto Buffer::GetSizeBytes() const -> core::usize {
        return mElementCount == 0 ? mElementSize : mElementCount * mElementSize;
    }


    auto Buffer::GetFormat() const -> rhi::Format {
        return mFormat;
    }


    auto Buffer::GetCount() const -> core::usize {
        return mElementCount == 0 ? rhi::InferElementCount( mFormat, mElementSize ) : mElementCount;
    }
}