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

#include <volk.h>

#include <EASTL/numeric_limits.h>

#include <Core/Core.hh>
#include <Core/Profiler.hh>
#include <Core/Types.hh>

#include <Filesystem/FileService.hh>
#include <Filesystem/FileSystem.hh>

#include <Logging/Assert.hh>

#include <Memory/Allocator.hh>

#include <Renderer/Rhi/Vulkan/VulkanContext.hh>
#include <Renderer/Rhi/Vulkan/VulkanDevice.hh>
#include <Renderer/Rhi/Vulkan/VulkanHelpers.hh>
#include <Renderer/Rhi/Vulkan/VulkanTexture.hh>

namespace mikoto::renderer::vulkan {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    Sampler::operator VkSampler() const {
        MKT_BEGIN_PROFILER();
        return mSampler;
    }

    Texture::operator VkImage() const {
        MKT_BEGIN_PROFILER();
        return mImageAllocation.mImage;
    }


    Sampler::Sampler( const renderer::rhi::SamplerCreateDescription &desc )
        : mMipLevels{ desc.mMipLevels },
          mMinFilter{ desc.mMinFilter },
          mMagFilter{ desc.mMagFilter },
          mWrapU{ desc.mWrapU },
          mWrapV{ desc.mWrapV },
          mWrapW{ desc.mWrapW },
          mBorderColor{ desc.mBorderColor } {
        MKT_BEGIN_PROFILER();
        // Create a Sampler for the texture we will display in the viewport
        mCreateInfo = initializers::SamplerCreateInfo();

        mCreateInfo.magFilter = GetSamplerFilter( mMagFilter );
        mCreateInfo.minFilter = GetSamplerFilter( mMinFilter );
        mCreateInfo.mipmapMode = vulkan::GetMipmapMode( desc.mMipMapMode );

        mCreateInfo.addressModeU = GetSamplerWrap( mWrapU );
        mCreateInfo.addressModeV = GetSamplerWrap( mWrapV );
        mCreateInfo.addressModeW = GetSamplerWrap( mWrapW );

        mCreateInfo.maxAnisotropy = desc.mMaxAnisotropy;
        mCreateInfo.mipLodBias = 0.0f;
        mCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        mCreateInfo.minLod = 0.0f;
        mCreateInfo.maxLod = mMipLevels;
        mCreateInfo.anisotropyEnable = VK_TRUE;

        if ( desc.mCompareOp == rhi::CompareOp::eNever ) {
            mCreateInfo.compareEnable = VK_FALSE;
            mCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
        }
        else {
            mCreateInfo.compareEnable = VK_TRUE;
            mCreateInfo.compareOp = GetCompareOp( desc.mCompareOp );
        }

        // Currently we only support transparent white or transparent black
        if ( desc.mBorderColor == renderer::rhi::kColorWhite ) {
            mCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        }
    }

    auto Sampler::SetDebugName( eastl::string_view name ) -> void {
        MKT_BEGIN_PROFILER();

        // There is no device before we call Initialize()
        if ( !mIsAllocated || name.empty() ) {
            return;
        }

        mDebugName = name;

        Device *device{ core::checked_cast<Device *>( mDevice ) };
        device->SetDebugName( VK_OBJECT_TYPE_SAMPLER, core::rc_cast<core::u64>( mSampler ), mDebugName );
    }

    auto Sampler::GetNativeHandle( renderer::rhi::ObjectType type ) -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        if ( type != renderer::rhi::ObjectType::Vk_Sampler ) {
            return renderer::rhi::Object{ nullptr };
        }

        return renderer::rhi::Object{ mSampler };
    }

    auto Sampler::GetNativeHandle( renderer::rhi::ObjectType type ) const -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        if ( type != renderer::rhi::ObjectType::Vk_Sampler ) {
            return renderer::rhi::Object{ nullptr };
        }

        return renderer::rhi::Object{ mSampler };
    }

    auto Sampler::GetMinFilter() const -> rhi::SamplerFilter {
        return mMinFilter;
    }

    auto Sampler::GetMagFilter() const -> rhi::SamplerFilter {
        return mMagFilter;
    }

    auto Sampler::GetWrapU() const -> rhi::SamplerWrapMode {
        return mWrapU;
    }

    auto Sampler::GetWrapV() const -> rhi::SamplerWrapMode {
        return mWrapV;
    }

    auto Sampler::GetWrapW() const -> rhi::SamplerWrapMode {
        return mWrapW;
    }

    auto Sampler::GetMipLevels() const -> float {
        return mMipLevels;
    }


    Sampler::~Sampler() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto Sampler::Destroy() -> void {
        MKT_BEGIN_PROFILER();
        vkDestroySampler( core::checked_cast<Device *>( mDevice )->GetDevice(), mSampler, nullptr );
        mIsAllocated = false;
    }

    auto Sampler::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        const Device *device{ core::checked_cast<const Device *>( mDevice ) };
        mCreateInfo.anisotropyEnable = device->GetActivePhysicalDeviceFeatures().samplerAnisotropy;

        const VkPhysicalDeviceProperties &deviceProps{ device->GetPhysicalDevice()->mProperties };
        const core::f32 deviceMaxAnisotropy{ deviceProps.limits.maxSamplerAnisotropy };

        if ( mCreateInfo.maxAnisotropy > deviceMaxAnisotropy ) {
            mCreateInfo.maxAnisotropy = deviceMaxAnisotropy;
        }

        MKT_VK_CHECK( vkCreateSampler( core::checked_cast<Device *>( mDevice )->GetDevice(),
            MKT_ADDRESSOF( mCreateInfo ), nullptr, MKT_ADDRESSOF( mSampler ) ) );

        mCreateInfo.pNext = nullptr;

        mIsAllocated = true;
    }

    Texture::Texture( const renderer::rhi::TextureCreateDescription &data )
        : mWidth{ data.mWidth },
          mHeight{ data.mHeight },
          mDepth{ data.mDimension == rhi::TextureDimension::eTexture3D ? data.mDepth : 1u },
          mMipCount{ data.mMipCount },
          mImageData{ data.mImageHandle },
          mBufferSpan{ data.mBufferSpan },
          mFormat{ data.mFormat },
          mDimension{ data.mDimension },
          mInitialState{ data.mInitialState },
          mSubResources{ data.mSubresourceSet },
          mMultisampling{ data.mMultisampling },
          mTextureUsage{ data.mUsage },
          mKeepInitializerResources{ data.mKeepInitializerResources } {
        MKT_BEGIN_PROFILER();

        mHeapType = data.mHeapType;
        mResourceType = data.mResourceType;
    }

    Texture::Texture( const ExternalTextureDescription &info )
        : Texture{ renderer::rhi::TextureCreateDescription{}
            .SetWidth( info.mWidth )
            .SetHeight( info.mHeight )
            .SetFormat( info.mSurfaceFormat )
            .SetUsage( info.mUsage ) } {
        MKT_BEGIN_PROFILER();

        mImageViewCreateInfo = info.mImageViewCreateInfo;
        mAspectFlags = mImageViewCreateInfo.subresourceRange.aspectMask;
        mExternalOwner = info.mOwner;
        mIsImageExternal = true;

        mImageAllocation.mImage = mImageViewCreateInfo.image;
        mDebugName = "Mikoto Swap chain Texture";
    }

    auto Texture::Destroy() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mIsAllocated ) {
            return;
        }

        vkDestroyImageView( core::checked_cast<Device *>( mDevice )->GetDevice(), mImageViewSrv, nullptr );

        for ( auto &faces: mImageViewRtvList ) {
            for ( auto &face: faces ) {
                vkDestroyImageView( core::checked_cast<Device *>( mDevice )->GetDevice(), face, nullptr );
            }
        }

        if ( !mIsImageExternal ) {
            auto *allocator{ core::as<GpuMemoryAllocator *>( core::checked_cast<Device *>( mDevice )->GetAllocator() ) };
            allocator->FreeImage( mImageAllocation );
        }

        mIsAllocated = false;
    }

    auto Texture::CopyInitialData( memory::BufferSpanHandle buffer ) -> void {
        MKT_BEGIN_PROFILER();

        core::u64 fenceValue{ 0 };
        renderer::rhi::FenceHandle fence{ mDevice->CreateFence( fenceValue++ ) };
        renderer::rhi::CommandListHandle cmd{ mDevice->CreateCommandList( renderer::rhi::QueueType::eTransfer ) };
        cmd->Open( { .mScopeName = string::Format( "Texture upload: {}", mDebugName ) } );

        // Data is always writen at mip zero
        cmd->Write( this, buffer->GetData(), buffer->GetSize() );

        if ( mInitialState != renderer::rhi::ResourceStates::eUnknown ) {
            cmd->SetTransition( { this, mInitialState } );
        }
        else {
            // These textures are often loaded to be read from shaders
            cmd->SetTransition( { this, renderer::rhi::ResourceStates::eShaderResource } );
        }

        cmd->Close();

        // Signal fenceValue on one fence on completion of these
        // commands, then we wait for that completion this blocks the caller
        // but client should ideally offload this task to worker threads
        const auto submitInfo{ renderer::rhi::SubmitInfo{}
                .AddSignal( fence, fenceValue )
                .AddCommandList( cmd ) };
        mDevice->GetQueue( renderer::rhi::QueueType::eTransfer )->ExecuteCommandLists( submitInfo );
        ( void )fence->Wait( fenceValue, eastl::numeric_limits<core::u64>::max() );// Host wait
    }

    auto Texture::SetDebugName( eastl::string_view name ) -> void {
        MKT_BEGIN_PROFILER();

        // There is no device before we call Initialize()
        if ( !mIsAllocated || name.empty() ) {
            return;
        }

        mDebugName = name;

        Device *device{ core::checked_cast<Device *>( mDevice ) };

        device->SetDebugName( VK_OBJECT_TYPE_IMAGE, core::rc_cast<core::u64>( mImageAllocation.mImage ), mDebugName );

        if ( mImageAllocation.mAllocation ) {
            auto *allocator{ core::checked_cast<Device *>( mDevice )->GetAllocator() };
            allocator->SetAllocationName( mImageAllocation.mAllocation, mDebugName.c_str() );
        }

        for ( const auto &faces: mImageViewRtvList ) {
            for ( auto &face: faces ) {
                device->SetDebugName( VK_OBJECT_TYPE_IMAGE_VIEW, core::rc_cast<core::u64>( face ), mDebugName );
            }
        }
    }


    Texture::~Texture() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto Texture::GetAspectMask() const -> VkImageAspectFlags {
        MKT_BEGIN_PROFILER();
        return mAspectFlags;
    }

    auto Texture::GetRenderView( core::u32 mipLevel, core::u32 face ) const -> const VkImageView & {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( mipLevel < mImageViewRtvList.size(), "Mip level out of bounds" );
        return mImageViewRtvList[mipLevel][face];
    }

    auto Texture::GetNativeHandle( renderer::rhi::ObjectType type ) -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        switch ( type ) {
            case renderer::rhi::ObjectType::Vk_Image:
                return renderer::rhi::Object( mImageAllocation.mImage );

            case renderer::rhi::ObjectType::Vk_ImageView:
                return renderer::rhi::Object( mImageViewSrv );

            case renderer::rhi::ObjectType::Vk_Format:
                return renderer::rhi::Object( std::addressof( mImageAllocation.mImageCreateInfo.format ) );

            default:;
        }

        return renderer::rhi::Object( nullptr );
    }

    auto Texture::GetNativeHandle( renderer::rhi::ObjectType type ) const -> renderer::rhi::Object {
        return const_cast<Texture *>( this )->GetNativeHandle( type );
    }

    auto Texture::Initialize() -> void {
        MKT_BEGIN_PROFILER();

        if ( mImageAllocation.mImage == VK_NULL_HANDLE ) {
            mImageAllocation.mImageCreateInfo = initializers::ImageCreateInfo();

            const VkExtent3D extent{
                core::as<core::u32>( mWidth ),
                core::as<core::u32>( mHeight ),
                mDepth
            };

            mImageAllocation.mImageCreateInfo.format = vulkan::GetFormat( mFormat );
            mImageAllocation.mImageCreateInfo.extent = extent;

            // Voxel depth is independent of array layers, including storage views.
            mImageAllocation.mImageCreateInfo.imageType = GetTextureType( mDimension );
            mImageAllocation.mImageCreateInfo.usage = GetImageUsage( mTextureUsage );

            mImageAllocation.mImageCreateInfo.mipLevels = mMipCount;
            mImageAllocation.mImageCreateInfo.arrayLayers = vulkan::GetArraLayerCount( mDimension );
            mImageAllocation.mImageCreateInfo.samples = vulkan::GetSampleCount( mMultisampling );// TODO: fallback to device max supported
            mImageAllocation.mImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            mImageAllocation.mImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            // The image will only be used by one queue family:
            // the one that supports transfer operations, often graphics one suffices.
            mImageAllocation.mImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            mImageAllocation.mImageCreateInfo.flags = 0;

            if ( mDimension == renderer::rhi::TextureDimension::eTextureCube ) {
                mImageAllocation.mImageCreateInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
            }

            mImageAllocation.mAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

            // vkBindImageMemory(): Trying to bind VkImage 0x3d500000003d5 to a memory block which is fully consumed by the image.
            // The required size of the allocation is 16384, but smaller images like this should be sub-allocated from larger
            // memory blocks. (Current threshold is 1048576 bytes)
            // const core::usize threshHold{ MKT_MIBIBYTES( 1 ) };
            // const auto& formatInfo{ rhi::GetFormatInfo( mFormat ) };
            // const auto imageSize{ mWidth * mHeight * formatInfo.bytesPerBlock };
            // if ( imageSize >= threshHold) {
            //     mImageAllocation.mAllocationCreateInfo.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
            // }

            // TODO:
            // Vulkan [Error] vkCmdCopyBufferToImage(): pRegions[0].bufferOffset (163756972) must be a
            // multiple texel block size (16) for VK_FORMAT_R32G32B32A32_SFLOAT. The Vulkan spec states:
            // If dstImage does not have either a depth/stencil format or a multi-planar format, then for
            // each element of pRegions, bufferOffset must be a multiple of the texel block size

            mImageAllocation.mAllocationCreateInfo.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
            mImageAllocation.mAllocationCreateInfo.priority = 1.0f;

            auto *allocator{ core::checked_cast<Device *>( mDevice )->GetAllocator() };
            MKT_VK_CHECK( allocator->AllocateImage( mImageAllocation ) );

            allocator->SetAllocationName( mImageAllocation.mAllocation, mDebugName.c_str() );

            if ( !mImageData.IsEmpty() ) {
                CopyInitialData( mImageData->mBufferSpan );
            }
            else if ( !mBufferSpan.IsEmpty() ) {
                CopyInitialData( mBufferSpan );
            }
            else if ( mInitialState != renderer::rhi::ResourceStates::eUnknown ) {
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
                mImageData.Reset();
                mBufferSpan.Reset();
            }

            // Depth textures can be sampled or copied without depth-target usage.
            mAspectFlags = vulkan::GetAspectMask( mFormat );

            // Prepare for view creation
            mImageViewCreateInfo = initializers::ImageViewCreateInfo();
            mImageViewCreateInfo.image = mImageAllocation.mImage;
            mImageViewCreateInfo.viewType = vulkan::GetViewType( mDimension );
            mImageViewCreateInfo.format = mImageAllocation.mImageCreateInfo.format;

            mImageViewCreateInfo.subresourceRange.aspectMask = mAspectFlags;

            // This is one because for cube images we use one image view per face which is
            // basically one flat image per face, we do not have a whole image view for the whole image cube
            // in the case of using cube images
            //mImageViewCreateInfo.subresourceRange.layerCount = mDimension == TextureDimension::eTextureCube ? kMaxCubeFaces : 1;
            mImageViewCreateInfo.subresourceRange.layerCount = 1;
        }

        // Color attachments and storage descriptors need per-mip/per-face views.
        // Depth attachments and sampled textures use the main view below.
        constexpr auto subresourceUsage{ rhi::TextureUsageFlagsBits::RenderTarget |
                                         rhi::TextureUsageFlagsBits::UnorderedAccess };

        if ( mTextureUsage & subresourceUsage ) {
            core::u32 faceCount{ 1u };

            if ( mDimension == renderer::rhi::TextureDimension::eTextureCube ) {
                faceCount = 6u;
            }

            // Prepare mip levels
            mImageViewRtvList.resize( GetMipLevelCount() );

            for ( auto &faces: mImageViewRtvList ) {
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
            // One attachment/storage view per mip and cube face.
            for ( core::usize mipLevelIndex{ 0 }; mipLevelIndex < GetMipLevelCount(); ++mipLevelIndex ) {
                for ( core::u32 faceIndex{}; faceIndex < faceCount; faceIndex++ ) {
                    mImageViewCreateInfo.viewType = mDimension == rhi::TextureDimension::eTexture3D ? VK_IMAGE_VIEW_TYPE_3D : VK_IMAGE_VIEW_TYPE_2D;
                    mImageViewCreateInfo.subresourceRange.levelCount = 1;
                    mImageViewCreateInfo.subresourceRange.baseMipLevel = mipLevelIndex;
                    mImageViewCreateInfo.subresourceRange.baseArrayLayer = faceIndex;// 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z

                    MKT_VK_CHECK( vkCreateImageView( core::checked_cast<Device *>( mDevice )->GetDevice(),
                        MKT_ADDRESSOF( mImageViewCreateInfo ), nullptr,
                        MKT_ADDRESSOF( mImageViewRtvList[mipLevelIndex][faceIndex] ) ) );
                }
            }
        }

        // Create main image view (SRV)
        mImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        mImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        mImageViewCreateInfo.subresourceRange.levelCount = mMipCount;
        mImageViewCreateInfo.viewType = vulkan::GetViewType( mDimension );
        mImageViewCreateInfo.subresourceRange.layerCount = mDimension == renderer::rhi::TextureDimension::eTextureCube ? renderer::rhi::kMaxCubeFaces : 1;
        MKT_VK_CHECK( vkCreateImageView( core::checked_cast<Device *>( mDevice )->GetDevice(),
            MKT_ADDRESSOF( mImageViewCreateInfo ), nullptr,
            MKT_ADDRESSOF( mImageViewSrv ) ) );

        mIsAllocated = true;
    }

    auto Texture::GetStorageSizeBytes() const -> usize {
        const auto &format{ rhi::GetFormatInfo( mFormat ) };
        const usize block{ eastl::max<usize>( 1, format.mBlockSize ) };

        usize result{};
        usize width{ mWidth };
        usize height{ mHeight };
        usize depth{ mDepth };

        for ( u32 mip{}; mip < mMipCount; ++mip ) {
            result += ( ( width + block - 1 ) / block ) *
                      ( ( height + block - 1 ) / block ) * depth * format.mBytesPerBlock;

            width = eastl::max<usize>( 1, width / 2 );
            height = eastl::max<usize>( 1, height / 2 );
            depth = eastl::max<usize>( 1, depth / 2 );
        }

        return result * ( mDimension == TextureDimension::eTextureCube ? 6u : 1u );
    }

    auto Texture::GetFormat() const -> rhi::Format {
        return mFormat;
    }

    auto Texture::GetWidth() const -> core::u32 {
        return mWidth;
    }

    auto Texture::GetHeight() const -> core::u32 {
        return mHeight;
    }

    auto Texture::GetDepth() const -> core::u32 {
        return mDepth;
    }

    auto Texture::GetTextureUsage() const -> rhi::TextureUsageFlags {
        return mTextureUsage;
    }

    auto Texture::GetImage() const -> asset::ImageHandle {
        return mImageData;
    }

    auto Texture::GetSizeBytes() const -> core::usize {
        if ( mImageData && mImageData->mBufferSpan ) {
            return mImageData->mBufferSpan->GetSize();
        }

        return mBufferSpan ? mBufferSpan->GetSize() : 0;
    }

    auto Texture::GetDimension() const -> rhi::TextureDimension {
        return mDimension;
    }

    auto Texture::GetSampleCount() const -> rhi::Multisampling {
        return mMultisampling;
    }

    auto Texture::GetMipLevelCount() const -> core::u32 {
        return mMipCount;
    }
}// namespace mikoto::renderer::vulkan
