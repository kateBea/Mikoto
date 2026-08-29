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

#include <ranges>

#include <EASTL/array.h>
#include <EASTL/numeric.h>
#include <EASTL/vector.h>
#include <EASTL/fixed_vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Logging/Assert.hh>

#include <Renderer/Rhi/Vulkan/VulkanDevice.hh>
#include <Renderer/Rhi/Vulkan/VulkanHelpers.hh>
#include <Renderer/Rhi/Vulkan/VulkanSwapchain.hh>

namespace mikoto::renderer::vulkan {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    SwapChain::SwapChain( const SwapChainCreateInfo &createInfo )
        : mWidth{ createInfo.mWidth },
        mHeight{ createInfo.mHeight },
        mPhysicalDevice{ createInfo.mPhysicalDevice },
        mSurface{ createInfo.mSurface },
        mRefreshRate{ createInfo.mRefreshRate } {
        MKT_ASSERT( mSurface != VK_NULL_HANDLE, "Surface needs to be a valid handle" );
        MKT_ASSERT( mPhysicalDevice != VK_NULL_HANDLE, "Physical device needs to be a valid" );
    }

    auto SwapChain::GetWidth() const -> u32 {
        return mWidth;
    }

    auto SwapChain::GetHeight() const -> u32 {
        return mHeight;
    }

    auto SwapChain::GetImageCount() const -> size_t {
        return mImages.size();
    }

    auto SwapChain::GetImage( size_t index ) -> TextureHandle {
        MKT_ASSERT( index < mImages.size(), "Index out of bounds." );
        return mImages.at(index);
    }

    auto SwapChain::GetFormat() -> Format {
        return mFormat;
    }

    auto SwapChain::Present( u32 imageIndex, const BinarySemaphore* signalSemaphore ) -> VkResult {
        const eastl::array swapChains{ mSwapChain };

        VkSemaphore semaphore{ *signalSemaphore };
        const eastl::array waitSemaphores{ semaphore };

        VkPresentInfoKHR presentInfo{ initializers::PresentInfoKHR() };

        presentInfo.swapchainCount = as<u32>(swapChains.size());
        presentInfo.pSwapchains = swapChains.data();

        presentInfo.pImageIndices = &imageIndex;

        // specifies the semaphores to wait for before issuing the present request.
        presentInfo.waitSemaphoreCount = waitSemaphores.size();
        presentInfo.pWaitSemaphores = waitSemaphores.data();

        return checked_cast<Queue*>( mPresentQueue )->Present( presentInfo );
    }

    auto SwapChain::GetNextImageIndex( u32 &imageIndex, const BinarySemaphore* waitSemaphore ) -> VkResult {
        VkSemaphore semaphore{ *waitSemaphore };
        return vkAcquireNextImageKHR( checked_cast<Device*>(mDevice)->GetDevice(), mSwapChain, ( eastl::numeric_limits<u64>::max )(),
           semaphore, VK_NULL_HANDLE, MKT_ADDRESSOF( imageIndex ) );
    }

    auto SwapChain::SetRefreshType( RefreshRate type ) -> void {
        mRefreshRate = type;
    }

    auto SwapChain::OnResize( u32 width, u32 height ) -> void {
        Device* device{ checked_cast<Device*>(mDevice) };
        device->WaitIdle();

        mWidth = width;
        mHeight = height;
        mOldSwapChain = mSwapChain;

        mImages.clear();

        Initialize();
    }

    SwapChain::~SwapChain() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto SwapChain::Release() -> void {
        Device* device{ checked_cast<Device*>(mDevice) };

        mImages.clear();

        // Destroy handles
        // The device is owned by the context and is destroyed before the instance and after any object is
        // created from it has finished being used
        vkDestroySwapchainKHR( device->GetDevice(), mSwapChain, nullptr );

        mIsAllocated = false;
    }

    auto SwapChain::Initialize() -> void {
        // Pick format and color space
        // NOTE: if we only have one format, and it is VK_FORMAT_UNDEFINED, it means the surface supports all formats
        mSurfaceSupportedFormat = vulkan::GetFormat( mFormat );

        // Pick the first format and color space available if our picked format/color-space combo is not supported
        if (!std::ranges::any_of(mPhysicalDevice->mFormats, [&](const VkSurfaceFormatKHR& sf) {
            return sf.format == mSurfaceSupportedFormat && sf.colorSpace == mColorSpace;
        } )) {
            mSurfaceSupportedFormat = mPhysicalDevice->mFormats[0].format;
            mColorSpace = mPhysicalDevice->mFormats[0].colorSpace;
        }

        // Pick a present mode
        // If this device supports presenting VK_PRESENT_MODE_FIFO_KHR is always guaranteed
        VkPresentModeKHR presentMode{ VK_PRESENT_MODE_FIFO_KHR };
        if (mRefreshRate == RefreshRate::eUnlimited) {
            const auto it{ std::ranges::find_if(mPhysicalDevice->mPresentModes, [](const VkPresentModeKHR& pm) {
                return pm == VK_PRESENT_MODE_MAILBOX_KHR || pm == VK_PRESENT_MODE_IMMEDIATE_KHR;
            }) };

            if (it != mPhysicalDevice->mPresentModes.end()) {
                presentMode = *it;
            }
        }

        // Determine if the currentExtent is set to a special value indicating that the surface size is undefined.
        // This special value is (std::numeric_limits<u32>::max)(). If the currentExtent width is equal to
        // this value, it means the surface size can be defined by the application, otherwise, the currentExtent
        // provided by the surface capabilities should be used.

        // If capabilities.currentExtent.width is not equal to the maximum unsigned integer, it means the surface
        // size is defined, and you should use currentExtent.
        // If it is equal to the maximum unsigned integer, you need to define the extent yourself within the bounds
        // of minImageExtent and maxImageExtent.
        // For now, it gets ignored
        MKT_VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice->mPhysicalDevice, mSurface, &mPhysicalDevice->mCapabilities));

        VkExtent2D actualExtent{};
        if ( mPhysicalDevice->mCapabilities.currentExtent.width != ( eastl::numeric_limits<u32>::max )() ) {
            actualExtent = mPhysicalDevice->mCapabilities.currentExtent;
        } else {
            // Because Windows macros are powerful af, we enclose max between parenthesis
            actualExtent = VkExtent2D{
                .width = ( eastl::max )( mPhysicalDevice->mCapabilities.minImageExtent.width, eastl::min( mPhysicalDevice->mCapabilities.maxImageExtent.width, mWidth ) ),
                .height = ( eastl::max )( mPhysicalDevice->mCapabilities.minImageExtent.height, eastl::min( mPhysicalDevice->mCapabilities.maxImageExtent.height, mHeight ) ),
            };
        }

        // Update current dimensions to match system requirements
        mWidth = actualExtent.width;
        mHeight = actualExtent.height;

        // Compute image count
        // We may sometimes have to wait on the driver to complete internal operations
        // before we can acquire another image to render to. Therefore, it is recommended
        // to request at least one more image, hence why we add 1. Likely the image count
        // results in the maximum swap chain image count so we do the check and clamp the resulting image count
        u32 imageCount{ mPhysicalDevice->mCapabilities.minImageCount };
        if ( mPhysicalDevice->mCapabilities.maxImageCount > 0 && imageCount > mPhysicalDevice->mCapabilities.maxImageCount ) {
            imageCount = mPhysicalDevice->mCapabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{ initializers::SwapchainCreateInfoKHR() };
        createInfo.surface = mSurface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = mSurfaceSupportedFormat;
        createInfo.imageColorSpace = mColorSpace;
        createInfo.imageExtent = VkExtent2D{ mWidth, mHeight };
        createInfo.imageArrayLayers = 1;

        // Swap chain images are used for drawing or copying to it (in case we render first to a texture and copy to it for presentation)
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        // Right now we assume they belong to same queue family index
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;

        // In case we want swapchain images to be shared across queue family indices
        // Swapchain images are presented via a queue that supports presentation
        // Queue* presentQueue{ as<Device*>(mDevice)->GetQueue( QueueType::ePresent ) };
        // MKT_ASSERT( presentQueue, "No valid presentation queue" );
        //
        // eastl::vector<u32> uniqueQueueFamilyIndices{ presentQueue->GetFamilyIndex() };
        // createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        // createInfo.queueFamilyIndexCount = uniqueQueueFamilyIndices.size();
        // createInfo.pQueueFamilyIndices = uniqueQueueFamilyIndices.data();

        createInfo.clipped = VK_TRUE;
        createInfo.presentMode = presentMode;
        createInfo.oldSwapchain = mOldSwapChain;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.preTransform = mPhysicalDevice->mCapabilities.currentTransform;

        MKT_VK_CHECK( vkCreateSwapchainKHR( checked_cast<Device*>(mDevice)->GetDevice(), MKT_ADDRESSOF( createInfo ), nullptr, MKT_ADDRESSOF( mSwapChain ) ) );

        AcquireSwapChainImages();

        Device* device{ checked_cast<Device*>( mDevice ) };
        mPresentQueue = checked_cast<Queue*>( device->GetQueue( QueueType::ePresent ) );
        MKT_ASSERT( mPresentQueue, "No valid presentation queue" );

        mIsAllocated = true;
    }

    auto SwapChain::AcquireSwapChainImages() -> void {
        Device* device{ checked_cast<Device*>(mDevice) };

        u32 imageCount{};

        // We only specified a minimum number of images in the swap chain, even though the implementation is
        // allowed to create a swap chain with more. That's why we'll first query the final number of
        // images with vkGetSwapchainImagesKHR with the last parameter as nullptr, then resize the container and finally call it again to
        // retrieve the handles.
        vkGetSwapchainImagesKHR( device->GetDevice(), mSwapChain, MKT_ADDRESSOF( imageCount ), nullptr );

        eastl::vector<VkImage> images{ imageCount };
        vkGetSwapchainImagesKHR( device->GetDevice(), mSwapChain, MKT_ADDRESSOF( imageCount ), images.data() );

        for ( u32 index{}; const VkImage& image: images ) {
            VkImageViewCreateInfo createInfo{ initializers::ImageViewCreateInfo() };
            createInfo.image = image;
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = mSurfaceSupportedFormat;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.layerCount = 1;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.baseMipLevel = 0;

            ExternalTextureDescription spec{
                .mWidth = mWidth,
                .mHeight = mHeight,
                .mSurfaceFormat = mFormat,
                .mImageViewCreateInfo = createInfo
            };

            TextureHandle presentImage{ device->CreateTexture( spec ) };
            if (!presentImage.IsEmpty()) {
                presentImage->SetDebugName( string::Format( "Swapchain Img. Index {}", index++ ) );
                mImages.emplace_back(presentImage);
            }
        }

        MKT_ASSERT( imageCount == mImages.size(), "Wrong image count" );
    }
}// namespace mikoto