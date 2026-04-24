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

#include <Core/Core.hh>
#include <Core/String.hh>
#include <Core/Types.hh>

#include <Filesystem/FileService.hh>
#include <Filesystem/FileSystem.hh>

#include <Memory/Allocator.hh>

#include <Renderer/Core/HdriToCubemap.hh>

#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>
#include <Renderer/Vulkan/VulkanTexture.hh>

namespace mikoto::renderer::vulkan {

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

    auto Sampler::GetNativeHandle( ObjectType type ) -> Object {
        if ( type != ObjectType::Vk_Sampler ) {
            return Object( nullptr );
        }

        return Object( mSampler );
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

    // Vulkan Texture 2D  ------------------------------------------------------------------------------------------------
    Texture::Texture( const TextureCreateDescription& data )
        : ITexture{ data } {

        // Prepare mip levels
        mImageViews.resize( GetMipLevelCount() );
    }

    Texture::Texture( const ExternalTextureDescription& info )
        : ITexture{
              TextureCreateDescription{}
                      .SetWidth( info.mWidth )
                      .SetHeight( info.mHeight )
                      .SetFormat( info.mSurfaceFormat )
          },
          mImageViewCreateInfo{ info.mImageViewCreateInfo },
          mIsImageExternal{ true } {
        mImageAllocation.mImage = mImageViewCreateInfo.image;

        mDebugName = string::Format( "Mikoto Swap chain Texture. Id:", GetHandle() );

        // Prepare mip levels
        mImageViews.resize( GetMipLevelCount() );
    }

    auto Texture::Release() -> void {
        if ( !mIsAllocated ) {
            return;
        }

        for ( auto& imageView: mImageViews ) {
            vkDestroyImageView( as<Device*>( mDevice )->GetDevice(), imageView, nullptr );
        }

        if ( !mIsImageExternal ) {
            auto* allocator{ as<GpuMemoryAllocator*>( as<Device*>( mDevice )->GetAllocator() ) };
            allocator->FreeImage( mImageAllocation );
        }

        mIsAllocated = false;
    }

    auto Texture::InitInitialData2D() -> void {
        CommandListHandle cmd{ mDevice->CreateCommandList( QueueType::eTransfer ) };
        cmd->Begin();

        // Data is always writen at mip zero
        cmd->WriteTexture( this, 0, mImageData->mBufferSpan->GetData(), mImageData->mBufferSpan->GetSize() );

        // These textures are often loaded to be read from shaders
        cmd->SetResourceState( this, ResourceStates::eShaderResource );

        cmd->End();
        mDevice->ExecuteCommands( cmd );
    }

    auto Texture::InitInitialDataCube() -> void {

    }

    Texture::~Texture() {
        if ( mIsAllocated ) {
            Release();
        }
    }

    auto Texture::HasExternalImage() const -> bool {
        return mIsImageExternal;
    }

    auto Texture::GetView( u32 mipLevel ) const -> const VkImageView& {
        MKT_ASSERT( mipLevel < mImageViews.size(), "Mip level out of bounds" );
        return mImageViews[mipLevel];
    }

    auto Texture::SetDebugName( eastl::string_view name ) -> void {
    }

    auto Texture::IsSwapChainImage() const -> bool {
        return mIsImageExternal;
    }

    auto Texture::GetNativeHandle( ObjectType type ) -> Object {
        switch ( type ) {
            case ObjectType::Vk_Image:
                return Object( mImageAllocation.mImage );

            case ObjectType::Vk_ImageView:
                return Object( mImageViews[0] );// Returns view at mip 0

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
            mImageAllocation.mImageCreateInfo.usage = InferImageUsage( mTextureUsage );

            mImageAllocation.mImageCreateInfo.mipLevels = mMipCount;
            mImageAllocation.mImageCreateInfo.arrayLayers = 1;
            mImageAllocation.mImageCreateInfo.samples = vulkan::GetSampleCount(mMultisampling);
            mImageAllocation.mImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            mImageAllocation.mImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            // The image will only be used by one queue family:
            // the one that supports transfer operations, often graphics one suffices.
            mImageAllocation.mImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            mImageAllocation.mImageCreateInfo.flags = 0;

            if (mDimension == TextureDimension::eTextureCube ) {
                mImageAllocation.mImageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
            }

            mImageAllocation.mAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

            // I still do not have a reason to not make images to be in device local
            // memory so this stays as VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT
            mImageAllocation.mAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
            mImageAllocation.mAllocationCreateInfo.priority = 1.0f;

            auto* allocator{ as<Device*>( mDevice )->GetAllocator() };
            MKT_VK_CHECK( allocator->AllocateImage( mImageAllocation ) );

            if ( !mImageData.IsEmpty() ) {
                if (mDimension == TextureDimension::eTexture2D) {
                    InitInitialData2D();
                } else if (mDimension == TextureDimension::eTextureCube) {
                    InitInitialDataCube();
                }
            }

            VkImageAspectFlags aspectFlags{};
            if ( mTextureUsage == TextureUsageFlagsBits::kDepthTarget ) {
                aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
            } else if ( mTextureUsage == TextureUsageFlagsBits::kDepthStencilTarget ) {
                aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            } else {
                aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
            }

            // Prepare for view creation
            mImageViewCreateInfo = initializers::ImageViewCreateInfo();
            mImageViewCreateInfo.image = mImageAllocation.mImage;
            mImageViewCreateInfo.viewType = vulkan::GetViewType(mDimension);
            mImageViewCreateInfo.format = mImageAllocation.mImageCreateInfo.format;

            mImageViewCreateInfo.subresourceRange.aspectMask = aspectFlags;
            mImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
            mImageViewCreateInfo.subresourceRange.levelCount = mMipCount;
            mImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
            mImageViewCreateInfo.subresourceRange.layerCount = mDimension == TextureDimension::eTextureCube ? kMaxCubeFaces : 1;
        }

        for ( size_t mipLevelIndex{ 0 }; mipLevelIndex < GetMipLevelCount(); ++mipLevelIndex ) {
            mImageViewCreateInfo.subresourceRange.baseMipLevel = mipLevelIndex;
            MKT_VK_CHECK( vkCreateImageView( as<Device*>( mDevice )->GetDevice(), MKT_ADDRESSOF( mImageViewCreateInfo ), nullptr, MKT_ADDRESSOF( mImageViews[mipLevelIndex] ) ) );
        }

        mIsAllocated = true;
    }
}// namespace mikoto::renderer::vulkan