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
#include <EASTL/vector.h>
#include <EASTL/numeric.h>
#include <EASTL/fixed_vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Logging/Assert.hh>

#include <Renderer/Rhi/Vulkan/VulkanDevice.hh>
#include <Renderer/Rhi/Vulkan/VulkanHelpers.hh>
#include <Renderer/Rhi/Vulkan/VulkanSwapchain.hh>

namespace mikoto::renderer::vulkan {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    SwapChain::SwapChain( const SwapChainCreateInfo& createInfo )
        : mWidth{ createInfo.mWidth },
          mHeight{ createInfo.mHeight },
          mPhysicalDevice{ createInfo.mPhysicalDevice },
          mSurface{ createInfo.mSurface },
          mRefreshRate{ createInfo.mRefreshRate } {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( mSurface != VK_NULL_HANDLE, "Surface needs to be a valid handle" );
        MKT_ASSERT( mPhysicalDevice != VK_NULL_HANDLE, "Physical device needs to be a valid" );
    }

    auto SwapChain::GetWidth() const -> core::u32 {
        MKT_BEGIN_PROFILER();
        return mWidth;
    }

    auto SwapChain::GetHeight() const -> core::u32 {
        MKT_BEGIN_PROFILER();
        return mHeight;
    }

    auto SwapChain::GetImageCount() const -> core::usize {
        MKT_BEGIN_PROFILER();
        return mImages.size();
    }

    auto SwapChain::GetImage( core::usize index ) -> renderer::rhi::TextureHandle {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( index < mImages.size(), "Index out of bounds." );
        return mImages.at( index );
    }

    auto SwapChain::GetFormat() -> renderer::rhi::Format {
        MKT_BEGIN_PROFILER();
        return mFormat;
    }

    auto SwapChain::GetRenderFinishedSemaphore( core::usize imageIndex ) const -> BinarySemaphoreHandle {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( imageIndex < mRenderFinishedSemaphores.size(), "Swapchain semaphore index out of bounds." );
        return mRenderFinishedSemaphores.at( imageIndex );
    }

    auto SwapChain::Present( core::u32 imageIndex ) -> VkResult {
        MKT_BEGIN_PROFILER();
        const eastl::array swapChains{ mSwapChain };

        VkSemaphore semaphore{ *GetRenderFinishedSemaphore( imageIndex ) };
        const eastl::array waitSemaphores{ semaphore };

        VkPresentInfoKHR presentInfo{ initializers::PresentInfoKHR() };

        presentInfo.swapchainCount = core::as<core::u32>( swapChains.size() );
        presentInfo.pSwapchains = swapChains.data();

        presentInfo.pImageIndices = &imageIndex;

        // specifies the semaphores to wait for before issuing the present request.
        presentInfo.waitSemaphoreCount = waitSemaphores.size();
        presentInfo.pWaitSemaphores = waitSemaphores.data();

        const auto result{ core::checked_cast<Queue*>( mPresentQueue )->Present( presentInfo ) };

        if ( result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR ) {
            mImagePresentSerials[imageIndex] = ++mPresentSerial;
        }

        return result;
    }

    auto SwapChain::GetNextImageIndex( core::u32& imageIndex, const BinarySemaphore* waitSemaphore ) -> VkResult {
        MKT_BEGIN_PROFILER();
        VkSemaphore semaphore{ *waitSemaphore };
        const auto device{ core::checked_cast<Device*>( mDevice )->GetDevice() };
        VkFence acquired{ VK_NULL_HANDLE };

        if ( !mRetiredSwapChains.empty() ) {
            const VkFenceCreateInfo info{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
            MKT_VK_CHECK( vkCreateFence( device, &info, nullptr, &acquired ) );
        }

        const auto result{ vkAcquireNextImageKHR( device, mSwapChain, ( eastl::numeric_limits<core::u64>::max )(),
            semaphore, acquired, MKT_ADDRESSOF( imageIndex ) ) };

        if ( acquired != VK_NULL_HANDLE ) {
            VkResult waitResult{ VK_SUCCESS };

            if ( result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR ) {
                waitResult = vkWaitForFences( device, 1, &acquired, VK_TRUE, ( eastl::numeric_limits<core::u64>::max )() );
            }

            vkDestroyFence( device, acquired, nullptr );
            MKT_VK_CHECK( waitResult );

            if ( result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR ) {
                // Reacquisition plus the acquire fence proves this image's previous
                // presentation finished, including earlier retired swapchains.
                ReleaseRetiredSwapchains( mImagePresentSerials[imageIndex] );
            }
        }

        return result;
    }

    auto SwapChain::SetRefreshType( renderer::rhi::RefreshRate type ) -> void {
        MKT_BEGIN_PROFILER();
        mRefreshRate = type;
    }

    auto SwapChain::OnResize( core::u32 width, core::u32 height ) -> bool {
        MKT_BEGIN_PROFILER();

        if ( width == 0 || height == 0 ) {
            return false;
        }

        mWidth = width;
        mHeight = height;
        return Recreate();
    }

    auto SwapChain::ReleaseRetiredSwapchains( core::u64 completedPresent ) -> void {
        MKT_BEGIN_PROFILER();
        const auto device{ core::checked_cast<Device*>( mDevice )->GetDevice() };

        for ( auto it{ mRetiredSwapChains.begin() }; it != mRetiredSwapChains.end(); ) {
            if ( it->mAfterPresent > completedPresent ) {
                ++it;
                continue;
            }

            it->mImages.clear();
            it->mSemaphores.clear();
            vkDestroySwapchainKHR( device, it->mSwapchain, nullptr );
            it = mRetiredSwapChains.erase( it );
        }
    }

    SwapChain::~SwapChain() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto SwapChain::Destroy() -> void {
        MKT_BEGIN_PROFILER();
        Device* device{ core::checked_cast<Device*>( mDevice ) };

        // Ensure the device is idle before destroying any swapchain-backed resources.
        device->WaitIdle();

        mImages.clear();
        mRenderFinishedSemaphores.clear();

        if ( mSwapChain != VK_NULL_HANDLE ) {
            vkDestroySwapchainKHR( device->GetDevice(), mSwapChain, nullptr );
            mSwapChain = VK_NULL_HANDLE;
        }

        ReleaseRetiredSwapchains( ( eastl::numeric_limits<core::u64>::max )() );

        mIsAllocated = false;
    }

    auto SwapChain::Initialize() -> void {
        if ( !Recreate() ) {
            MKT_THROW_RUNTIME_ERROR( "Cannot create a Vulkan swapchain for a zero-sized surface." );
        }
    }

    auto SwapChain::Recreate() -> bool {
        MKT_BEGIN_PROFILER();
        // Pick format and color space
        // NOTE: if we only have one format, and it is VK_FORMAT_UNDEFINED, it means the surface supports all formats
        mSurfaceSupportedFormat = vulkan::GetFormat( mFormat );

        // Pick the first format and color space available if our picked format/color-space combo is not supported
        if ( !std::ranges::any_of( mPhysicalDevice->mFormats, [&]( const VkSurfaceFormatKHR& sf ) {
            return sf.format == mSurfaceSupportedFormat && sf.colorSpace == mColorSpace;
        } ) ) {
            mSurfaceSupportedFormat = mPhysicalDevice->mFormats[0].format;
            mColorSpace = mPhysicalDevice->mFormats[0].colorSpace;
        }

        // Pick a present mode
        // If this device supports presenting VK_PRESENT_MODE_FIFO_KHR is always guaranteed
        VkPresentModeKHR presentMode{ VK_PRESENT_MODE_FIFO_KHR };

        if ( mRefreshRate == renderer::rhi::RefreshRate::eUnlimited ) {
            const auto it{ std::ranges::find_if( mPhysicalDevice->mPresentModes, []( const VkPresentModeKHR& pm ) {
                return pm == VK_PRESENT_MODE_MAILBOX_KHR || pm == VK_PRESENT_MODE_IMMEDIATE_KHR;
            } ) };

            if ( it != mPhysicalDevice->mPresentModes.end() ) {
                presentMode = *it;
            }
        }

        // Determine if the currentExtent is set to a special value indicating that the surface size is undefined.
        // This special value is (eastl::numeric_limits<u32>::max)(). If the currentExtent width is equal to
        // this value, it means the surface size can be defined by the application, otherwise, the currentExtent
        // provided by the surface capabilities should be used.

        // If capabilities.currentExtent.width is not equal to the maximum unsigned integer, it means the surface
        // size is defined, and you should use currentExtent.
        // If it is equal to the maximum unsigned integer, you need to define the extent yourself within the bounds
        // of minImageExtent and maxImageExtent.
        // For now, it gets ignored
        Device* device{ core::checked_cast<Device*>( mDevice ) };
        device->WaitIdle();
        MKT_VK_CHECK( vkGetPhysicalDeviceSurfaceCapabilitiesKHR( mPhysicalDevice->mPhysicalDevice, mSurface, &mSurfaceCapabilities ) );

        VkExtent2D actualExtent{};

        if ( mSurfaceCapabilities.currentExtent.width != ( eastl::numeric_limits<core::u32>::max )() ) {
            actualExtent = mSurfaceCapabilities.currentExtent;
        }
        else {
            // Because Windows macros are powerful af, we enclose max between parenthesis
            actualExtent = VkExtent2D{
                .width = ( eastl::max )( mSurfaceCapabilities.minImageExtent.width, eastl::min( mSurfaceCapabilities.maxImageExtent.width, mWidth ) ),
                .height = ( eastl::max )( mSurfaceCapabilities.minImageExtent.height, eastl::min( mSurfaceCapabilities.maxImageExtent.height, mHeight ) ),
            };
        }

        // TRANSFER_DST is largely supported, COLOR is supposed to be the guarantee. This is more of a sanity check.
        MKT_ASSERT( mSurfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            "SwapChain image is expected to support TRANSFER_DST to blit to it." );

        // A minimize can happen after the window-size check. Keep the existing
        // swapchain and retry on a later frame instead of creating zero-sized images.
        if ( actualExtent.width == 0 || actualExtent.height == 0 ) {
            return false;
        }

        // Update current dimensions to match system requirements
        mWidth = actualExtent.width;
        mHeight = actualExtent.height;

        // Compute image count
        // We may sometimes have to wait on the driver to complete internal operations
        // before we can acquire another image to render to. Therefore, it is recommended
        // to request at least one more image, hence why we add 1. Likely the image count
        // results in the maximum swap chain image count so we do the check and clamp the resulting image count
        core::u32 imageCount{ mSurfaceCapabilities.minImageCount /*+ 1*/ };
        if ( mSurfaceCapabilities.maxImageCount > 0 && imageCount > mSurfaceCapabilities.maxImageCount ) {
            imageCount = mSurfaceCapabilities.maxImageCount;
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
        createInfo.oldSwapchain = mSwapChain;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.preTransform = mSurfaceCapabilities.currentTransform;

        VkSwapchainKHR replacement{ VK_NULL_HANDLE };
        const auto result{ vkCreateSwapchainKHR( device->GetDevice(), MKT_ADDRESSOF( createInfo ), nullptr, &replacement ) };

        if ( result == VK_ERROR_OUT_OF_DATE_KHR ) {
            // Another resize can race creation. The old swapchain is retired by
            // the call, so acquisition will request another recreation next frame.
            return false;
        }

        MKT_VK_CHECK( result );

        if ( mSwapChain != VK_NULL_HANDLE ) {
            // See Khronos' swapchain_recreation sample: graphics completion alone
            // does not prove the presentation engine released its wait semaphores.
            mRetiredSwapChains.push_back( RetiredSwapchain{
                mSwapChain, eastl::move( mImages ), eastl::move( mRenderFinishedSemaphores ), mPresentSerial + 1 } );
        }

        mSwapChain = replacement;
        mImages.clear();
        mRenderFinishedSemaphores.clear();

        AcquireSwapChainImages();

        mPresentQueue = core::checked_cast<Queue*>( device->GetQueue( renderer::rhi::QueueType::ePresent ) );
        MKT_ASSERT( mPresentQueue, "No valid presentation queue" );

        mIsAllocated = true;
        return true;
    }

    auto SwapChain::AcquireSwapChainImages() -> void {
        MKT_BEGIN_PROFILER();
        Device* device{ core::checked_cast<Device*>( mDevice ) };

        core::u32 imageCount{};

        // We only specified a minimum number of images in the swap chain, even though the implementation is
        // allowed to create a swap chain with more. That's why we'll first query the final number of
        // images with vkGetSwapchainImagesKHR with the last parameter as nullptr, then resize the container and finally call it again to
        // retrieve the handles.
        vkGetSwapchainImagesKHR( device->GetDevice(), mSwapChain, MKT_ADDRESSOF( imageCount ), nullptr );

        eastl::vector<VkImage> images{ imageCount };
        vkGetSwapchainImagesKHR( device->GetDevice(), mSwapChain, MKT_ADDRESSOF( imageCount ), images.data() );

        for ( core::u32 index{}; const VkImage& image: images ) {
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

            renderer::rhi::TextureHandle presentImage{ device->CreateTexture( spec ) };

            if ( !presentImage.IsEmpty() ) {
                presentImage->SetDebugName( string::Format( "SwapChain Img. Index {}", index++ ) );
                mImages.emplace_back( presentImage );
            }
        }

        MKT_ASSERT( imageCount == mImages.size(), "Wrong image count" );
        mImagePresentSerials.assign( mImages.size(), 0 );

        // Waiting for a frame fence does not prove vkQueuePresentKHR has
        // consumed its wait semaphore. Reacquiring the same image and waiting
        // on its acquire semaphore makes this image's semaphore safe to reuse.
        // Rebuild the set on resize, even if the new image count is unchanged.
        mRenderFinishedSemaphores.reserve( mImages.size() );

        for ( core::usize index{}; index < mImages.size(); ++index ) {
            auto semaphore{ device->CreateBinarySemaphore() };
            MKT_ASSERT( !semaphore.IsEmpty(), "Could not create swapchain presentation semaphore." );
            semaphore->SetDebugName( string::Format( "SwapChain Render Done BinSemaphore image {}", index ) );
            mRenderFinishedSemaphores.emplace_back( eastl::move( semaphore ) );
        }
    }
}// namespace mikoto