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

#include <Renderer/Rhi/Vulkan/VulkanBuffer.hh>
#include <Renderer/Rhi/Vulkan/VulkanDevice.hh>
#include <Renderer/Rhi/Vulkan/VulkanHelpers.hh>

namespace mikoto::renderer::vulkan {

    using namespace mikoto::core;
    using namespace mikoto::memory;
    using namespace mikoto::renderer::rhi;

    Buffer::Buffer( const BufferCreateDescription &createInfo )
        : IBuffer{ createInfo }, mKeepInitializerResources{ createInfo.mKeepInitializerResources } {
        mAllocation.mAllocationCreateInfo.priority = 1.0f;
        mAllocation.mAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

        mAllocation.mBufferCreateInfo =  initializers::BufferCreateInfo();

        // Perf. Warn: vkBindBufferMemory(): Trying to bind VkBuffer 0x2780000000278 to a memory block which is
        // fully consumed by the buffer. The required size of the allocation is 2880, but smaller buffers like
        // this should be sub-allocated from larger memory blocks. (Current threshold is 1048576 bytes)
        const size_t threshHold{ MKT_MIBIBYTES( 1 ) };
        mAllocation.mBufferCreateInfo.size = mElementCount == 0 ? mElementSize : mElementCount * mElementSize;

        switch (mHeapType) {
            case HeapType::eDeviceLocal:
                if ( mAllocation.mBufferCreateInfo.size >= threshHold) {
                    mAllocation.mAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
                }
                break;
            case HeapType::eUpload:
                mAllocation.mBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

                mAllocation.mAllocationCreateInfo.flags =
                    VMA_ALLOCATION_CREATE_MAPPED_BIT |
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
                break;
            case HeapType::eReadback:
                mAllocation.mBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                mAllocation.mAllocationCreateInfo.flags =
                    VMA_ALLOCATION_CREATE_MAPPED_BIT |
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
                break;
        }

        if (mUsage.Has( BufferUsageFlagsBits::kVertex )) {
            mAllocation.mBufferCreateInfo.usage |=
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        }

        if (mUsage.Has( BufferUsageFlagsBits::kIndex )) {
            mAllocation.mBufferCreateInfo.usage |=
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        }

        if (mUsage.Has( BufferUsageFlagsBits::kStorage )) {
            mAllocation.mBufferCreateInfo.usage |=
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        }

        if (mUsage.Has( BufferUsageFlagsBits::kConstant )) {
            mAllocation.mBufferCreateInfo.usage |=
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        }

        if (mUsage.Has( BufferUsageFlagsBits::kIndirectDraw )) {
            // Mark as storage because it can be used to read and write from compute shaders
            mAllocation.mBufferCreateInfo.usage |=
                VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
        }

        // Explicit copy flags (this is what you were missing)
        if (mUsage.Has(BufferUsageFlagsBits::kCopySrc)) {
            mAllocation.mBufferCreateInfo.usage |=
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        }

        if (mUsage.Has(BufferUsageFlagsBits::kCopyDst)) {
            mAllocation.mBufferCreateInfo.usage |=
                VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        }

        // Look into VkUpdateBuffer when buffer is less than 64kb
    }

    Buffer::~Buffer() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto Buffer::Release() -> void {
        auto* allocator{ checked_cast<Device*>( mDevice  )->GetAllocator() };

        if ( mAllocation.mBuffer != VK_NULL_HANDLE ) {
            if (!(mAllocation.mAllocationCreateInfo.flags & VMA_ALLOCATION_CREATE_MAPPED_BIT)) {
                allocator->UnmapBuffer( mAllocation );
            }
        }

        allocator->FreeBuffer( mAllocation );

        mIsAllocated = false;
    }

    auto Buffer::Initialize() -> void {
        MKT_ASSERT( mAllocation.mBufferCreateInfo.usage != MKT_VK_FLAGS_NONE, "Buffer usage must be specified" );
        MKT_ASSERT( mAllocation.mBufferCreateInfo.size != 0, "Buffer size must be different than 0" );

        auto* allocator{ checked_cast<Device*>( mDevice )->GetAllocator() };
        MKT_VK_CHECK( allocator->AllocateBuffer( mAllocation ) );

        if (!mUploadContents.IsEmpty()) {
            CommandListHandle cmd{ mDevice->CreateCommandList( QueueType::eTransfer ) };
            cmd->Begin( {} );
            cmd->Write( this, mUploadContents->GetData(), mUploadContents->GetSize() );
            cmd->End();

            auto submitInfo{ SubmitInfo{}
                .AddCommandList( cmd ) };
            mDevice->GetQueue(QueueType::eTransfer)->ExecuteCommandLists( submitInfo );
        }

        if (!mKeepInitializerResources) {
            mUploadContents.Reset();
        }

        mIsAllocated = true;
    }

    auto Buffer::GetNativeHandle( ObjectType type ) -> Object {
        switch (type) {
            case ObjectType::Vk_Buffer:
                return Object( mAllocation.mBuffer );
            default:;
        }

        return Object(nullptr);
    }

    auto Buffer::GetNativeHandle( ObjectType type ) const -> Object {
        switch (type) {
            case ObjectType::Vk_Buffer:
                return Object( mAllocation.mBuffer );
            default:;
        }

        return Object(nullptr);
    }

    auto Buffer::IsMapped() const -> bool {
        return mAllocation.mAllocationInfo.pMappedData != nullptr;
    }

    auto Buffer::GetMappedAddress() -> void * {
        return mAllocation.mAllocationInfo.pMappedData;
    }

    auto Buffer::GetMappedAddress() const -> const void * {
        return mAllocation.mAllocationInfo.pMappedData;
    }

    auto Buffer::PersistentMap() -> void {
        if (mAllocation.mBuffer == VK_NULL_HANDLE || mAllocation.mAllocationInfo.pMappedData != nullptr) {
            return;
        }

        auto allocator{ checked_cast<Device*>( mDevice )->GetAllocator() };
        allocator->MapBuffer( mAllocation );
    }

    auto Buffer::PersistentUnmap() -> void {
        if (mAllocation.mBuffer == VK_NULL_HANDLE || mAllocation.mAllocationInfo.pMappedData != nullptr) {
            return;
        }

        auto allocator{ checked_cast<Device*>( mDevice )->GetAllocator() };
        allocator->UnmapBuffer(  mAllocation );

        mAllocation.mAllocationInfo.pMappedData = nullptr;
    }

    auto Buffer::SetDebugName( eastl::string_view name )  -> void {
        mDebugName = name;

        auto* device{ checked_cast<Device*>( mDevice ) };
        device->SetDebugName( VK_OBJECT_TYPE_BUFFER, rc_cast<u64>( mAllocation.mBuffer ), mDebugName );
    }

    auto Buffer::GetAlignedSize() const -> u32 {
        return mAlignedSizeBytes;
    }
}// namespace Mikoto