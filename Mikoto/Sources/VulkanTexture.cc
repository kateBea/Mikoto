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

#include <volk.h>

#include <Core/Core.hh>
#include <Core/String.hh>
#include <Core/Types.hh>

#include <Filesystem/FileSystem.hh>
#include <Filesystem/FileService.hh>

#include <Memory/Allocator.hh>

#include <Renderer/Core/HdriToCubemap.hh>

#include <Renderer/Rhi/Vulkan/VulkanContext.hh>
#include <Renderer/Rhi/Vulkan/VulkanDevice.hh>
#include <Renderer/Rhi/Vulkan/VulkanHelpers.hh>
#include <Renderer/Rhi/Vulkan/VulkanTexture.hh>

namespace mikoto::renderer::vulkan {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    Sampler::Sampler( const SamplerCreateDescription& info )
        : ISampler{ info } {
        // Create a Sampler for the texture we will display in the viewport
        mCreateInfo = initializers::SamplerCreateInfo();

        mCreateInfo.magFilter = GetSamplerFilter( mMagFilter );
        mCreateInfo.minFilter = GetSamplerFilter( mMinFilter );
        mCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

        mCreateInfo.addressModeU = GetSamplerWrap( mWrapU );
        mCreateInfo.addressModeV = GetSamplerWrap( mWrapV );
        mCreateInfo.addressModeW = GetSamplerWrap( mWrapW );

        mCreateInfo.maxAnisotropy = 1.0f;
        mCreateInfo.mipLodBias = 0.0f;
        mCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        mCreateInfo.minLod = 0.0f;
        mCreateInfo.maxLod = mMipLevels;
        mCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        mCreateInfo.anisotropyEnable = VK_TRUE;

        // Currently we only support transparent white or transparent black
        if ( info.mBorderColor == kColorWhite ) {
            mCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        }
    }

    auto Sampler::SetDebugName( eastl::string_view name ) -> void {
        mDebugName = name;

        // There is no device before we call Initialize()
        if (!mIsAllocated) {
            return;
        }

        Device* device{ checked_cast<Device*>( mDevice ) };
        device->SetDebugName( VK_OBJECT_TYPE_SAMPLER, rc_cast<u64>( mSampler), mDebugName );
    }

    auto Sampler::GetNativeHandle( ObjectType type ) -> Object {
        if ( type != ObjectType::Vk_Sampler ) {
            return Object( nullptr );
        }

        return Object( mSampler );
    }

    auto Sampler::GetNativeHandle( ObjectType type ) const -> Object {
        if ( type != ObjectType::Vk_Sampler ) {
            return Object( nullptr );
        }

        return Object( mSampler );
    }

    Sampler::operator VkSampler() const {
        return mSampler;
    }

    Sampler::~Sampler() {
        if ( mIsAllocated ) {
            Release();
        }
    }

    auto Sampler::Release() -> void {
        vkDestroySampler( checked_cast<Device*>( mDevice )->GetDevice(), mSampler, nullptr );
        mIsAllocated = false;
    }

    auto Sampler::Initialize() -> void {
        mCreateInfo.maxAnisotropy = checked_cast<Device*>( mDevice )->GetPhysicalDevice()->mProperties.limits.maxSamplerAnisotropy;
        MKT_VK_CHECK( vkCreateSampler( checked_cast<Device*>( mDevice )->GetDevice(), MKT_ADDRESSOF( mCreateInfo ), nullptr, MKT_ADDRESSOF( mSampler ) ) );

        mIsAllocated = true;
    }

    Texture::Texture( const TextureCreateDescription& data )
        : ITexture{ data }, mKeepInitializerResources{ data.mKeepInitializerResources } {}

    Texture::Texture( const ExternalTextureDescription& info )
        : ITexture{
              TextureCreateDescription{}
                      .SetWidth( info.mWidth )
                      .SetHeight( info.mHeight )
                      .SetFormat( info.mSurfaceFormat ) },
                  mImageViewCreateInfo{ info.mImageViewCreateInfo },
                  mIsImageExternal{ true } {

        mImageAllocation.mImage = mImageViewCreateInfo.image;
        mDebugName = string::Format( "Mikoto Swap chain Texture. Id:", GetHandle() );
    }

    auto Texture::Release() -> void {
        if ( !mIsAllocated ) {
            return;
        }

        vkDestroyImageView( checked_cast<Device*>( mDevice )->GetDevice(), mImageViewSrv, nullptr );

        for ( auto& faces: mImageViewRtvList ) {
            for (auto& face : faces ) {
                vkDestroyImageView( checked_cast<Device*>( mDevice )->GetDevice(), face, nullptr );
            }
        }

        if ( !mIsImageExternal ) {
            auto* allocator{ as<GpuMemoryAllocator*>( checked_cast<Device*>( mDevice )->GetAllocator() ) };
            allocator->FreeImage( mImageAllocation );
        }

        mIsAllocated = false;
    }

    auto Texture::InitInitialData2D( memory::BufferSpanHandle buffer ) -> void {
        u64 fenceValue{ 0 };
        FenceHandle fence{ mDevice->CreateFence( fenceValue++ ) };
        CommandListHandle cmd{ mDevice->CreateCommandList( QueueType::eTransfer ) };
        cmd->Begin( { .mScopeName = string::Format( "Texture upload: {}", mDebugName ) } );

        // Data is always writen at mip zero
        cmd->Write( this, 0, buffer->GetData(), buffer->GetSize() );

        // These textures are often loaded to be read from shaders
        cmd->SetTransition( this, ResourceStates::eShaderResource );

        cmd->End();

        // Signal fenceValue on one fence on completion of these
        // commands, then we wait for that completion this blocks the caller
        // but client should ideally offload this task to worker threads
        const auto submitInfo{ SubmitInfo{}
            .AddSignal( fence, fenceValue )
            .AddCommandList( cmd ) };
        mDevice->GetQueue( QueueType::eTransfer )->ExecuteCommandLists( submitInfo );
        ( void )fence->Wait( fenceValue, eastl::numeric_limits<u64>::max() ); // Host wait
    }

    auto Texture::SetDebugName( eastl::string_view name )  -> void {
        mDebugName = name;

        // There is no device before we call Initialize()
        if (!mIsAllocated) {
            return;
        }

        auto* device{ checked_cast<Device*>( mDevice ) };
        device->SetDebugName( VK_OBJECT_TYPE_IMAGE, rc_cast<u64>( mImageAllocation.mImage ), mDebugName );

        for (const auto& faces: mImageViewRtvList ) {
            for (auto& face : faces ) {
                device->SetDebugName( VK_OBJECT_TYPE_IMAGE_VIEW, rc_cast<u64>( face ), mDebugName );
            }
        }
    }

    Texture::operator VkImage() const {
        return mImageAllocation.mImage;
    }

    auto Texture::InitInitialDataCube( memory::BufferSpanHandle buffer ) -> void {

    }

    Texture::~Texture() {
        if ( mIsAllocated ) {
            Release();
        }
    }

    auto Texture::GetAspectMask() const -> VkImageAspectFlags {
        return mAspectFlags;
    }

    auto Texture::GetRenderView( u32 mipLevel, u32 face ) const -> const VkImageView& {
        MKT_ASSERT( mipLevel < mImageViewRtvList.size(), "Mip level out of bounds" );
        return mImageViewRtvList[mipLevel][face];
    }

    auto Texture::GetNativeHandle( ObjectType type ) -> Object {
        switch ( type ) {
            case ObjectType::Vk_Image:
                return Object( mImageAllocation.mImage );

            case ObjectType::Vk_ImageView:
                return Object( mImageViewSrv );

            case ObjectType::Vk_Format:
                return Object( std::addressof( mImageAllocation.mImageCreateInfo.format ) );

            default:;
        }

        return Object( nullptr );
    }

    auto Texture::GetNativeHandle( ObjectType type ) const -> Object {
        return const_cast<Texture*>( this )->GetNativeHandle( type );
    }

    auto Texture::Initialize() -> void {
        if ( mImageAllocation.mImage == VK_NULL_HANDLE ) {
            mImageAllocation.mImageCreateInfo = initializers::ImageCreateInfo();

            const VkExtent3D extent{
                as<u32>( mWidth ),
                as<u32>( mHeight ),
                1
            };

            mImageAllocation.mImageCreateInfo.format = vulkan::GetFormat( mFormat );
            mImageAllocation.mImageCreateInfo.extent = extent;

            // This texture is a 2D image always
            mImageAllocation.mImageCreateInfo.imageType = GetTextureType(mDimension);
            mImageAllocation.mImageCreateInfo.usage = GetImageUsage( mTextureUsage );

            mImageAllocation.mImageCreateInfo.mipLevels = mMipCount;
            mImageAllocation.mImageCreateInfo.arrayLayers = vulkan::GetArraLayerCount(mDimension);
            mImageAllocation.mImageCreateInfo.samples = vulkan::GetSampleCount(mMultisampling);
            mImageAllocation.mImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            mImageAllocation.mImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            // The image will only be used by one queue family:
            // the one that supports transfer operations, often graphics one suffices.
            mImageAllocation.mImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            mImageAllocation.mImageCreateInfo.flags = 0;

            if (mDimension == TextureDimension::eTextureCube ) {
                mImageAllocation.mImageCreateInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
            }

            mImageAllocation.mAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

            // vkBindImageMemory(): Trying to bind VkImage 0x3d500000003d5 to a memory block which is fully consumed by the image.
            // The required size of the allocation is 16384, but smaller images like this should be sub-allocated from larger
            // memory blocks. (Current threshold is 1048576 bytes)
            // const size_t threshHold{ MKT_MIBIBYTES( 1 ) };
            // const auto& formatInfo{ rhi::GetFormatInfo( mFormat ) };
            // const auto imageSize{ mWidth * mHeight * formatInfo.bytesPerBlock };
            // if ( imageSize >= threshHold) {
            //     mImageAllocation.mAllocationCreateInfo.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
            // }

            mImageAllocation.mAllocationCreateInfo.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

            mImageAllocation.mAllocationCreateInfo.priority = 1.0f;

            auto* allocator{ checked_cast<Device*>( mDevice )->GetAllocator() };
            MKT_VK_CHECK( allocator->AllocateImage( mImageAllocation ) );

            if ( !mImageData.IsEmpty() ) {
                if (mDimension == TextureDimension::eTexture2D) {
                    InitInitialData2D( mImageData->mBufferSpan );
                } else if (mDimension == TextureDimension::eTextureCube) {
                    InitInitialDataCube( mImageData->mBufferSpan );
                }
            }

            if ( !mBufferSpan.IsEmpty() ) {
                if (mDimension == TextureDimension::eTexture2D) {
                    InitInitialData2D( mBufferSpan );
                } else if (mDimension == TextureDimension::eTextureCube) {
                    InitInitialDataCube( mBufferSpan );
                }
            }

            if (!mKeepInitializerResources) {
                mImageData.Reset();
                mBufferSpan.Reset();
            }

            if ( mTextureUsage & TextureUsageFlagsBits::kDepthTarget ) {
                mAspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
            } else if ( mTextureUsage & TextureUsageFlagsBits::kDepthStencilTarget ) {
                mAspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            } else {
                mAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
            }

            // Prepare for view creation
            mImageViewCreateInfo = initializers::ImageViewCreateInfo();
            mImageViewCreateInfo.image = mImageAllocation.mImage;
            mImageViewCreateInfo.viewType = vulkan::GetViewType(mDimension);
            mImageViewCreateInfo.format = mImageAllocation.mImageCreateInfo.format;

            mImageViewCreateInfo.subresourceRange.aspectMask = mAspectFlags;

            // This is one because for cube images we use one image view per face which is
            // basically one flat image per face, we do not have a whole image view for the whole image cube
            // in the case of using cube images
            //mImageViewCreateInfo.subresourceRange.layerCount = mDimension == TextureDimension::eTextureCube ? kMaxCubeFaces : 1;
            mImageViewCreateInfo.subresourceRange.layerCount = 1;
        }

        u32 faceCount{ 1u };
        if (mDimension == TextureDimension::eTextureCube) {
            faceCount = 6u;
        }

        // Prepare mip levels
        mImageViewRtvList.resize( GetMipLevelCount() );
        for (auto& faces: mImageViewRtvList ) {
            faces.resize( faceCount );
        }

        // Create main image render view (RTV)
        // face 0 mip 0
        // face 1 mip 0
        // ...
        // face 5 mip 0
        //
        // face 0 mip n
        // face 1 mip n
        // ...
        for ( size_t mipLevelIndex{ 0 }; mipLevelIndex < GetMipLevelCount(); ++mipLevelIndex ) {
            for (u32 faceIndex{}; faceIndex < faceCount; faceIndex++) {
                mImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                mImageViewCreateInfo.subresourceRange.levelCount = 1;
                mImageViewCreateInfo.subresourceRange.baseMipLevel = mipLevelIndex;
                mImageViewCreateInfo.subresourceRange.baseArrayLayer = faceIndex; // 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z

                MKT_VK_CHECK( vkCreateImageView( checked_cast<Device*>( mDevice )->GetDevice(),
                    MKT_ADDRESSOF( mImageViewCreateInfo ), nullptr,
                    MKT_ADDRESSOF( mImageViewRtvList[mipLevelIndex][faceIndex] ) ) );
            }
        }

        // Create main image view (SRV)
        mImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        mImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        mImageViewCreateInfo.subresourceRange.levelCount = mMipCount;
        mImageViewCreateInfo.viewType = vulkan::GetViewType(mDimension);
        mImageViewCreateInfo.subresourceRange.layerCount = mDimension == TextureDimension::eTextureCube ? kMaxCubeFaces : 1;
        MKT_VK_CHECK( vkCreateImageView( checked_cast<Device*>( mDevice )->GetDevice(),
                    MKT_ADDRESSOF( mImageViewCreateInfo ), nullptr,
                    MKT_ADDRESSOF( mImageViewSrv ) ) );

        mIsAllocated = true;
    }
}// namespace mikoto::renderer::vulkan