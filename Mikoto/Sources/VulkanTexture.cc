/**
 * VulkanTexture2D.cc
 * Created by kate on 7/5/2023.
 * */

// C++ Standard Library
#include <filesystem>
#include <memory>
#include <stdexcept>

// Third-Party Libraries
#include <backends/imgui_impl_vulkan.h>
#include <stb_image.h>
#include <volk.h>

// Project Headers
#include <Common/Common.hh>
#include <Library/Utility/Types.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>
#include <Renderer/Vulkan/VulkanTexture.hh>

namespace Mikoto {

    VulkanSampler::VulkanSampler( const SamplerDescription& desc ) {
    }

    VulkanSampler::~VulkanSampler() {
        if ( !m_IsAllocated ) {
            return;
        }

        Release();
    }

    auto VulkanSampler::Release() -> void {
    }
    auto VulkanSampler::Allocate() -> void {
    }
    VulkanTexture::VulkanTexture( const TextureDescription& data )
        : Texture2D{ data.Type, data.Width, data.Height, data.ChannelCount, data.Data, data.UsageType } {
        m_ImageSize =  m_Width * m_Height * m_Channels;
    }

    VulkanTexture::VulkanTexture( VkImageViewCreateInfo viewCreateInfo )
        : Texture2D{ TextureType::TEXTURE_2D, 0, 0, 0, nullptr, ResourceUsageType::RESOURCE_USAGE_STATIC },
          m_Image{ viewCreateInfo.image },
          m_ImageViewCreateInfo{ viewCreateInfo },
          m_IsImageExternal{ true } {
        m_ImageSize =  m_Width * m_Height * m_Channels;
    }

    auto VulkanTexture::Release() -> void {
        if ( !m_IsAllocated ) {
            return;
        }

        dynamic_cast<VulkanDevice*>( m_Device )->WaitIdle();

        vkDestroyImageView( dynamic_cast<VulkanDevice*>( m_Device )->GetLogicalDevice(), m_ImageView, nullptr );

        if ( !m_IsImageExternal ) {
            // Free from VMA Allocator or GpuAllocator
            auto allocator{ dynamic_cast<VulkanMemoryAllocator*>( dynamic_cast<VulkanDevice*>( m_Device )->GetAllocator() ) };
            allocator->FreeImage( this );
        }

        m_IsAllocated = false;
    }

    auto VulkanTexture::GetVMAllocation() -> VmaAllocation* {
        return std::addressof( m_Allocation );
    }

    auto VulkanTexture::GetVMAllocationInfo() -> VmaAllocationInfo* {
        return std::addressof( m_AllocationInfo );
    }

    auto VulkanTexture::GetImageCreateInfo() -> const VkImageCreateInfo* {
        return std::addressof( m_ImageCreateInfo );
    }

    auto VulkanTexture::GetAllocationCreateInfo() -> const VmaAllocationCreateInfo* {
        return std::addressof( m_AllocationCreateInfo );
    }

    VulkanTexture::~VulkanTexture() {
        if ( m_IsAllocated ) {
            Release();
        }
    }

    auto VulkanTexture::GetImage() -> VkImage* {
        return std::addressof( m_Image );
    }

    auto VulkanTexture::GetImage() const -> const VkImage* {
        return std::addressof( m_Image );
    }

    auto VulkanTexture::GetView() -> VkImageView* {
        return std::addressof( m_ImageView );
    }

    auto VulkanTexture::GetView() const -> const VkImageView* {
        return std::addressof( m_ImageView );
    }

    auto VulkanTexture::HasExternalImage() const -> bool {
        return m_IsImageExternal;
    }

    auto VulkanTexture::GetCurrentLayout() const -> VkImageLayout {
        return m_CurrentLayout;
    }

    auto VulkanTexture::GetCreateInfo() const -> const VkImageCreateInfo& {
        return m_ImageCreateInfo;
    }

    auto VulkanTexture::GetViewCreateInfo() const -> const VkImageViewCreateInfo& {
        return m_ImageViewCreateInfo;
    }

    auto VulkanTexture::SubmitLayoutTransition( const VkImageLayout newLayout, const VkCommandBuffer cmd ) -> void {
        VkImageMemoryBarrier2 imageBarrier{};
        imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        imageBarrier.pNext = nullptr;

        imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
        imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

        imageBarrier.oldLayout = m_CurrentLayout;
        imageBarrier.newLayout = newLayout;

        VkImageAspectFlags aspectMask{ static_cast<VkImageAspectFlags>( newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT ) };
        imageBarrier.subresourceRange = VulkanHelpers::Initializers::ImageSubresourceRange( aspectMask );
        imageBarrier.image = m_Image;

        VkDependencyInfo depInfo{};
        depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        depInfo.pNext = nullptr;

        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers = std::addressof( imageBarrier );

        vkCmdPipelineBarrier2( cmd, std::addressof( depInfo ) );

        // Update the current layout
        m_CurrentLayout = newLayout;
    }

    VulkanSwapChain::VulkanSwapChain( const VulkanSwapChainCreateInfo& createInfo )
        : m_Extent{ createInfo.Extent },
          m_OldSwapChain{ createInfo.OldSwapChain },
          m_Surface{ createInfo.Surface },
          m_IsVsyncEnabled{ createInfo.EnableVsync } {
    }

    auto VulkanSwapChain::Allocate() -> void {
        if ( m_Surface == nullptr ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanSwapChain::Init - Error the surface for the swapchain is null." );
        }

        /**
         * [00:11:36] CORE LOG [thread 10211] Validation layer: Validation Error: [ VUID-VkSwapchainCreateInfoKHR-imageExtent-01274 ] Object 0:
         * handle = 0x62e000018450, type = VK_OBJECT_TYPE_DEVICE; | MessageID = 0x7cd0911d | vkCreateSwapchainKHR() called with imageExtent = (1494,921),
         * which is outside the bounds returned by vkGetPhysicalDeviceSurfaceCapabilitiesKHR(): currentExtent = (1495,925), minImageExtent = (1495,925),
         * maxImageExtent = (1495,925). The Vulkan spec states: imageExtent must be between minImageExtent and maxImageExtent, inclusive, where
         * minImageExtent and maxImageExtent are members of the VkSurfaceCapabilitiesKHR structure returned by vkGetPhysicalDeviceSurfaceCapabilitiesKHR
         * for the surface (https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/vkspec.html#VUID-VkSwapchainCreateInfoKHR-imageExtent-01274)
         *
         * this validation error is triggered at times when resizing the main window (GLFW window)
         * */
        CreateSwapChain();

        AcquireSwapchainImages();

        m_IsAllocated = true;
    }

    auto VulkanSwapChain::CreateSwapChain() -> void {
        const auto [Capabilities, Formats, PresentModes]{
            VulkanHelpers::GetSwapChainSupport( TO_VK_DEVICE( m_Device )->GetPhysicalDevice(), *m_Surface )
        };

        const auto [format, colorSpace]{ ChooseSurfaceFormat( Formats ) };
        const VkPresentModeKHR presentMode{ ChoosePresentMode( PresentModes ) };
        const VkExtent2D extent{ ChooseExtent( Capabilities ) };

        // Save for later use
        m_Format = format;
        m_PresentMode = presentMode;

        /**
         * We may sometimes have to wait on the driver to complete internal operations
         * before we can acquire another image to render to. Therefore, it is recommended
         * to request at least one more image, hence why we add 1. Likely the image count
         * results in the maximum swap chain image count so we do the check and clamp the resulting image count
         * */
        UInt32 imageCount{ Capabilities.minImageCount + 1 };
        if ( Capabilities.maxImageCount > 0 && imageCount > Capabilities.maxImageCount ) {
            imageCount = Capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{ VulkanHelpers::Initializers::SwapchainCreateInfoKHR() };
        createInfo.surface = *m_Surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = format;
        createInfo.imageColorSpace = colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;

        // Only the GUI is directly rendering to the swapchain images at the moment.
        // Generally, the renderer is drawing to a texture which can then be copied to a
        // swap chain image ready for render and then be presented
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        const auto& [Present, Graphics, Compute]{
            TO_VK_DEVICE( m_Device )->GetLogicalDeviceQueues()
        };

        // Let swapchain to share images between queues or not. We need to account for it
        // in the case the present queue and the graphics queue are not actually the same
        const std::array queueFamilyIndices{ Graphics->FamilyIndex, Present->FamilyIndex };
        if ( Graphics->FamilyIndex != Present->FamilyIndex ) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = queueFamilyIndices.size();
            createInfo.pQueueFamilyIndices = queueFamilyIndices.data();
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices = nullptr;
        }

        createInfo.preTransform = Capabilities.currentTransform;      // Image transform ot perform on swapchain images
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;// Handle blending, just draw as it is (perform no blending)
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;// TODO: pass old swapchain (need debug currently old swapchain becoming retired which can't be passed here)

        if ( vkCreateSwapchainKHR( VK_DEVICE( m_Device ), std::addressof( createInfo ), nullptr, std::addressof( m_Swapchain ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanSwapChain::CreateSwapChain - Failed to create swap chain." );
        }
    }

    auto VulkanTexture::AllocateImage() -> void {
        // Specify current layout, it should be undefined as this is a newly created image
        m_ImageCreateInfo.initialLayout = m_CurrentLayout;

        // The image has not been allocated, and
        // we need to allocate it on the given device
        if ( m_Image == VK_NULL_HANDLE ) {
            const auto allocator{ dynamic_cast<VulkanMemoryAllocator*>( TO_VK_DEVICE( m_Device )->GetAllocator() ) };

            const VkResult result{ allocator->AllocateImage( this ) };

            if ( result != VK_SUCCESS ) {
                MKT_THROW_RUNTIME_ERROR( "VulkanImage::Allocate - Failed to allocate VMA Image!" );
            }
        } else {
            m_IsImageExternal = true;
            m_ImageCreateInfo.initialLayout = m_CurrentLayout;
        }

        // Save the created image into the view create info
        // required to create the image view
        m_ImageViewCreateInfo.image = m_Image;

        // Here we always create the image view;
        // the caller can optionally pass a valid image because this VulkanImage is supposed be
        // usable for the swapchain as well, however, if the latter is the case,
        // we are responsible for releasing the image views, not the actual images.
        if ( vkCreateImageView( dynamic_cast<VulkanDevice*>( m_Device )->GetLogicalDevice(),
                                std::addressof( m_ImageViewCreateInfo ), nullptr, std::addressof( m_ImageView ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanImage::VulkanImage - Failed to create the Vulkan Image View!" );
        }

        m_IsAllocated = true;
    }

    auto VulkanTexture::Allocate() -> void {
        // Specify the current layout, it should be undefined as this is a newly created image
        m_ImageCreateInfo.initialLayout = m_CurrentLayout;

        // The case for swap chain images
        if ( m_Image != VK_NULL_HANDLE ) {

            // the caller can optionally pass a valid image because this VulkanTexture is supposed be
            // usable for the swapchain as well, however, if the latter is the case,
            // we are responsible for releasing the image views, not the actual images.
            if ( vkCreateImageView( VK_DEVICE( m_Device ), std::addressof( m_ImageViewCreateInfo ), nullptr, std::addressof( m_ImageView ) ) != VK_SUCCESS ) {
                MKT_THROW_RUNTIME_ERROR( "VulkanTexture::Allocate - Failed to create the Vulkan Image View!" );
            }
        } else {
            // Allocate staging buffer to copy over the texture data
            BufferDescription stagingDesc{};
            stagingDesc.WithData( nullptr )
                    .WithUsage( BufferUsage::BUFFER_USAGE_STAGING )
                    .WithSizeBytes( InferSize<UInt32>( m_ImageSize ) )
                    .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_STREAM );

            m_StagingBuffer = m_Device->CreateBuffer( stagingDesc );
            m_StagingBuffer->CopyFromBlock( m_Data, m_ImageSize );

            // Setup
            m_ImageCreateInfo = VulkanHelpers::Initializers::ImageCreateInfo();

            const VkExtent3D extent{
                static_cast<UInt32>( m_Width ),
                static_cast<UInt32>( m_Height ),
                1
            };

            m_ImageCreateInfo.format = VulkanHelpers::GetVkFormatFromTextureFormat( m_Format );
            m_ImageCreateInfo.extent = extent;
            m_ImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
            m_ImageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

            m_ImageCreateInfo.mipLevels = 1;
            m_ImageCreateInfo.arrayLayers = 1;
            m_ImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            m_ImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            m_ImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            // The image will only be used by one queue family:
            // the one that supports transfer operations, often graphics one suffices.
            m_ImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            m_ImageCreateInfo.flags = 0;

            VkImageViewCreateInfo imageViewCreateInfo{ VulkanHelpers::Initializers::ImageViewCreateInfo() };
            imageViewCreateInfo.pNext = nullptr;
            imageViewCreateInfo.flags = 0;
            imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            imageViewCreateInfo.format = m_ImageCreateInfo.format;

            imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
            imageViewCreateInfo.subresourceRange.levelCount = 1;
            imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
            imageViewCreateInfo.subresourceRange.layerCount = 1;

            // Allocate image using GPU Allocator
            const auto* allocator{ dynamic_cast<VulkanMemoryAllocator*>( TO_VK_DEVICE( m_Device )->GetAllocator() ) };
            if ( const VkResult result{ allocator->AllocateImage( this ) }; result != VK_SUCCESS ) {
                MKT_THROW_RUNTIME_ERROR( "Failed to allocate Vulkan buffer!" );
            }

            //Specify optional type operation so we return for instance
            //a command list to be submited in transfer queue
            CommandListHandle cmd{ m_Device->CreateCommandList() };
            cmd->Begin();

            cmd->FillTexture( m_StagingBuffer.GetRaw(), this );

            cmd->Close();
            m_Device->SubmitCommands(cmd);
        }

        m_IsAllocated = true;
    }

    auto VulkanSwapChain::AcquireSwapchainImages() -> void {
        static VulkanDevice& device{ *TO_VK_DEVICE( m_Device ) };

        UInt32 imageCount{};

        // We only specified a minimum number of images in the swap chain, even though the implementation is
        // allowed to create a swap chain with more. That's why we'll first query the final number of
        // images with vkGetSwapchainImagesKHR with the last parameter as nullptr, then resize the container and finally call it again to
        // retrieve the handles.
        vkGetSwapchainImagesKHR( device.GetLogicalDevice(), m_Swapchain, std::addressof( imageCount ), nullptr );

        auto images{ std::vector<VkImage>( imageCount ) };
        vkGetSwapchainImagesKHR( device.GetLogicalDevice(), m_Swapchain, std::addressof( imageCount ), images.data() );

        for ( VkImage image: images ) {
            m_Images.emplace_back( device.CreateSwapChainTextures( CreateSwapchainImageViewCreateInfo( image, m_Format ) ) );
        }
    }

    auto VulkanSwapChain::CreateSwapchainImageViewCreateInfo( VkImage image, const VkFormat& format ) -> VkImageViewCreateInfo {
        VkImageViewCreateInfo createInfo{ VulkanHelpers::Initializers::ImageViewCreateInfo() };
        createInfo.image = image;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = format;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.layerCount = 1;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.baseMipLevel = 0;

        return createInfo;
    }

    auto VulkanSwapChain::GetNextRenderableImage( UInt32& imageIndex, const VkFence fence, const VkSemaphore imageAvailable ) const -> VkResult {
        static VulkanDevice& device{ *TO_VK_DEVICE( m_Device ) };

        // For simplicity, parenthesize std::numeric_limits<std::uint64_t>::max because windows has a macro literally called max that causes conflicts
        vkWaitForFences( device.GetLogicalDevice(), 1, std::addressof( fence ), VK_TRUE, ( std::numeric_limits<std::uint64_t>::max )() );
        vkResetFences( device.GetLogicalDevice(), 1, std::addressof( fence ) );

        return vkAcquireNextImageKHR( VK_DEVICE( m_Device ), m_Swapchain, ( std::numeric_limits<UInt64>::max )(), imageAvailable, VK_NULL_HANDLE, std::addressof( imageIndex ) );
    }

    auto VulkanSwapChain::Present( const UInt32 imageIndex, const VkSemaphore& renderFinished ) -> VkResult {
        static VulkanDevice& device{ *TO_VK_DEVICE( m_Device ) };

        const std::array swapChains{ m_Swapchain };
        const std::array signalSemaphores{ renderFinished };

        VkPresentInfoKHR presentInfo{ VulkanHelpers::Initializers::PresentInfoKHR() };

        presentInfo.swapchainCount = swapChains.size();
        presentInfo.pSwapchains = swapChains.data();

        presentInfo.pImageIndices = &imageIndex;

        // specifies the semaphores to wait for before issuing the present request.
        presentInfo.waitSemaphoreCount = signalSemaphores.size();
        presentInfo.pWaitSemaphores = signalSemaphores.data();

        // Only the GUI is directly rendering to the swapchain images at the moment.
        // Generally, the renderer is drawing to a texture which can then be copied to a
        // swap chain image ready for render and then be presented
        m_CurrentFrame = ( m_CurrentFrame + 1 ) % MAX_FRAMES_IN_FLIGHT;

        const auto& [Present, Graphics, Compute]{ device.GetLogicalDeviceQueues() };
        VkQueue presentQueue{ VK_NULL_HANDLE };

        if ( Present.has_value() && Present->Queue != VK_NULL_HANDLE ) {
            presentQueue = Present->Queue;
        } else if ( Present.has_value() && Present->Queue != VK_NULL_HANDLE ) {
            // Present and graphics queue may have the same index
            presentQueue = Graphics->Queue;
        } else {
            MKT_CORE_LOGGER_ERROR( "VulkanSwapChain::Present - No presentation queue available." );
        }

        return vkQueuePresentKHR( presentQueue, std::addressof( presentInfo ) );
    }

    auto VulkanSwapChain::ChooseSurfaceFormat( const std::vector<VkSurfaceFormatKHR>& availableFormats ) -> VkSurfaceFormatKHR {
        // NOTE: if we only have one format, and it is VK_FORMAT_UNDEFINED, it means the surface supports all formats
        for ( const auto& availableFormat: availableFormats ) {
            if ( availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR ) {
                return availableFormat;
            }
        }

        return availableFormats[0];
    }

    auto VulkanSwapChain::ChoosePresentMode( const std::vector<VkPresentModeKHR>& availablePresentModes ) const -> VkPresentModeKHR {
        if ( m_IsVsyncEnabled ) {
            return VK_PRESENT_MODE_FIFO_KHR;
        }

        for ( const auto& availablePresentMode: availablePresentModes ) {
            if ( availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR || availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR ) {
                return availablePresentMode;
            }
        }

        // We return this one because if we do not find the present mode we are looking for,
        // VK_PRESENT_MODE_FIFO_KHR is guaranteed to be always available
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    auto VulkanSwapChain::ChooseExtent( const VkSurfaceCapabilitiesKHR& capabilities ) const -> VkExtent2D {
        // Determine if the currentExtent is set to a special value indicating that the surface size is undefined.
        // This special value is (std::numeric_limits<UInt32_T>::max)(). If the currentExtent width is equal to
        // this value, it means the surface size can be defined by the application, otherwise, the currentExtent
        // provided by the surface capabilities should be used.

        // If capabilities.currentExtent.width is not equal to the maximum unsigned integer, it means the surface
        // size is defined, and you should use currentExtent.
        // If it is equal to the maximum unsigned integer, you need to define the extent yourself within the bounds
        // of minImageExtent and maxImageExtent.

        if ( capabilities.currentExtent.width != ( std::numeric_limits<UInt32>::max )() ) {
            return capabilities.currentExtent;
        }

        const VkExtent2D actualExtent{
            .width{ ( std::max )( capabilities.minImageExtent.width, std::min( capabilities.maxImageExtent.width, m_Extent.width ) ) },
            .height{ ( std::max )( capabilities.minImageExtent.height, std::min( capabilities.maxImageExtent.height, m_Extent.height ) ) },
        };

        return actualExtent;
    }

    VulkanSwapChain::~VulkanSwapChain() {
        Release();
    }

    auto VulkanSwapChain::Release() -> void {
        if ( !m_IsAllocated ) {
            return;
        }

        // Wait on outstanding queue operations because there might be some objects still in use by the GPU
        TO_VK_DEVICE( m_Device )->WaitIdle();

        m_Surface = nullptr;

        m_Images.clear();

        // Destroy handles
        // The device is owned by the context and is destroyed before the instance and after any object is
        // created from it has finished being used
        vkDestroySwapchainKHR( VK_DEVICE( m_Device ), m_Swapchain, nullptr );

        m_IsAllocated = false;
    }

}// namespace Mikoto