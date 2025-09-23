// /**
//  * VulkanTexture2D.cc
//  * Created by kate on 7/5/2023.
//  * */
//
// // C++ Standard Library
// #include <filesystem>
// #include <memory>
// #include <stdexcept>
//
// // Third-Party Libraries
// #include <backends/imgui_impl_vulkan.h>
// #include <stb_image.h>
// #include <volk.h>
//
// // Project Headers
// #include <Common/Common.hh>
// #include <Library/Utility/Types.hh>
// #include <Renderer/Vulkan/VulkanContext.hh>
// #include <Renderer/Vulkan/VulkanHelpers.hh>
// #include <Renderer/Vulkan/VulkanRenderer.hh>
// #include <Renderer/Vulkan/VulkanTexture.hh>
//
// namespace Mikoto {
//
//     VulkanTexture::VulkanTexture( const TextureDescription& data )
//         : Texture2D{ data.Type, data.Width, data.Height, data.ChannelCount, data.Data, data.UsageType } {
//
//     }
//
//     auto VulkanTexture::Allocate() -> void {
//         // allocate staging buffer
//         VkBufferCreateInfo stagingBufferInfo{ VulkanHelpers::Initializers::BufferCreateInfo() };
//         stagingBufferInfo.pNext = nullptr;
//
//         stagingBufferInfo.size = m_BufferSize;
//         stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
//
//         //let the VMA library know that this data should be on CPU RAM
//         VmaAllocationCreateInfo vmaStagingAllocationCreateInfo{};
//         vmaStagingAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
//         vmaStagingAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
//
//         const VulkanBufferCreateInfo stagingBufferBufferCreateInfo{
//             .BufferCreateInfo{ stagingBufferInfo },
//             .AllocationCreateInfo{ vmaStagingAllocationCreateInfo },
//             .WantMapping{ true }
//         };
//
//         m_StagingBuffer = VulkanBuffer::Create( stagingBufferBufferCreateInfo );
//
//         // Copy vertex data to staging buffer
//         std::memcpy( m_StagingBuffer->GetVmaAllocationInfo().pMappedData, m_FileData.data(), m_BufferSize );
//
//         m_StagingBuffer->PersistentUnmap();
//
//         // Allocate image
//         const VkExtent3D extent{ static_cast<UInt32_T>( m_Width ), static_cast<UInt32_T>( m_Height ), 1 };
//
//         VkImageCreateInfo vkImageCreateInfo{ VulkanHelpers::Initializers::ImageCreateInfo() };
//
//         // TODO: Add support for other formats. For now, we only support RGBA8, if u pass in a different format, it will default to RGBA8
//         vkImageCreateInfo.format = VulkanHelpers::GetVkFormatFromTextureFormat(m_Format);
//         vkImageCreateInfo.extent = extent;
//         vkImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
//         vkImageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
//
//         vkImageCreateInfo.mipLevels = 1;
//         vkImageCreateInfo.arrayLayers = 1;
//         vkImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
//         vkImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
//         vkImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//
//         // The image will only be used by one queue family: the one that supports graphics (and therefore also) transfer operations.
//         vkImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
//         vkImageCreateInfo.flags = 0;
//
//         VkImageViewCreateInfo imageViewCreateInfo{ VulkanHelpers::Initializers::ImageViewCreateInfo() };
//
//         imageViewCreateInfo.pNext = nullptr;
//         imageViewCreateInfo.flags = 0;
//         imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
//         imageViewCreateInfo.format = vkImageCreateInfo.format;
//
//         imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//         imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
//         imageViewCreateInfo.subresourceRange.levelCount = 1;
//         imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
//         imageViewCreateInfo.subresourceRange.layerCount = 1;
//
//         VulkanImageCreateInfo vulkanImageCreateInfo{
//             .Image{ VK_NULL_HANDLE },
//             .ImageCreateInfo{ vkImageCreateInfo },
//             .ImageViewCreateInfo{ imageViewCreateInfo }
//         };
//
//         m_Image = VulkanImage::Create( vulkanImageCreateInfo );
//
//         auto device{ As<VulkanDevice*>(m_Device) };
//
//         device->TransferQueueSubmit( [this, extent, device]( const VkCommandBuffer& cmd ) -> void {
//             VulkanImage* image{ device->AccessImage( m_Image ) };
//
//             image->SubmitLayoutTransition( VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, cmd );
//
//             // Copy from staging buffer to image buffer
//             VkBufferImageCopy copyRegion{};
//             copyRegion.bufferOffset = 0;
//             copyRegion.bufferRowLength = 0;
//             copyRegion.bufferImageHeight = 0;
//
//             copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//             copyRegion.imageSubresource.mipLevel = 0;
//             copyRegion.imageSubresource.baseArrayLayer = 0;
//             copyRegion.imageSubresource.layerCount = 1;
//             copyRegion.imageExtent = extent;
//
//             //copy the buffer into the image
//             vkCmdCopyBufferToImage( cmd, m_StagingBuffer->Get(), m_Image->Get(), m_Image->GetCurrentLayout(), 1, std::addressof( copyRegion ) );
//         } );
//
//         VulkanContext::Get().ImmediateSubmit( [&]( VkCommandBuffer cmd ) -> void {
//             // Perform second transition for the descriptor set creation
//             m_Image->SubmitLayoutTransition( VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, cmd );
//         } );
//     }
//
//     auto VulkanTexture::GetImage() const -> VkImage {
//         return m_Image;
//     }
//     auto VulkanTexture::GetView() const -> VkImageView {
//         return m_ImageView;
//     }
//
//     auto VulkanTexture::HasExternalImage() const -> bool {
//         return m_IsImageExternal;
//     }
//
//     auto VulkanTexture::GetCurrentLayout() const -> VkImageLayout {
//         return m_CurrentLayout;
//     }
//
//     auto VulkanTexture::GetCreateInfo() const -> const VkImageCreateInfo& {
//         return m_ImageCreateInfo;
//     }
//
//     auto VulkanTexture::GetViewCreateInfo() const -> const VkImageViewCreateInfo& {
//         return m_ImageViewCreateInfo;
//     }
//
//     auto VulkanTexture::AllocateImage() -> void {
//         m_CurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//
//         // The image has not been allocated, and
//         // we need to allocate it on the given device
//         if ( m_Image == VK_NULL_HANDLE ) {
//             // TODO: do logic depending on resource usage type (STATIC, STREAM, DYNAMIC)
//
//             const VkResult result{ vmaCreateImage( dynamic_cast<VulkanDevice*>(m_Device)->GetAllocator(),
//                                            std::addressof( m_ImageCreateInfo ),
//                                            std::addressof( m_AllocationCreateInfo ),
//                                            std::addressof( m_Image ),
//                                            std::addressof( m_Allocation ),
//                                            std::addressof( m_AllocationInfo ) ) };
//
//             if ( result != VK_SUCCESS ) {
//                 MKT_THROW_RUNTIME_ERROR( "VulkanImage::Allocate - Failed to allocate VMA Image!" );
//             }
//         } else {
//             m_IsImageExternal = true;
//             m_ImageCreateInfo.initialLayout = m_CurrentLayout;
//         }
//
//         // Save the created image into the view create info
//         // required to create the image view
//         m_ImageViewCreateInfo.image = m_Image;
//
//         // Here we always create the image view;
//         // the caller can optionally pass a valid image because this VulkanImage is supposed be
//         // usable for the swapchain as well, however, if the latter is the case,
//         // we are responsible for releasing the image views, not the actual images.
//         if ( vkCreateImageView( dynamic_cast<VulkanDevice*>(m_Device)->GetLogicalDevice(),
//             std::addressof( m_ImageViewCreateInfo ), nullptr, std::addressof( m_ImageView ) ) != VK_SUCCESS ) {
//             MKT_THROW_RUNTIME_ERROR( "VulkanImage::VulkanImage - Failed to create the Vulkan Image View!" );
//         }
//
//         m_IsAllocated = true;
//     }
//
//     auto VulkanTexture::SubmitLayoutTransition( const VkImageLayout newLayout, const VkCommandBuffer cmd ) -> void {
//         VkImageMemoryBarrier2 imageBarrier{};
//         imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
//         imageBarrier.pNext = nullptr;
//
//         imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
//         imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
//         imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
//         imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
//
//         imageBarrier.oldLayout = m_CurrentLayout;
//         imageBarrier.newLayout = newLayout;
//
//         VkImageAspectFlags aspectMask{ static_cast<VkImageAspectFlags>( newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL ?
//             VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT ) };
//         imageBarrier.subresourceRange = VulkanHelpers::Initializers::ImageSubresourceRange(aspectMask);
//         imageBarrier.image = m_Image;
//
//         VkDependencyInfo depInfo{};
//         depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
//         depInfo.pNext = nullptr;
//
//         depInfo.imageMemoryBarrierCount = 1;
//         depInfo.pImageMemoryBarriers = std::addressof(imageBarrier);
//
//         vkCmdPipelineBarrier2(cmd, std::addressof(depInfo));
//
//         // Update the current layout
//         m_CurrentLayout = newLayout;
//     }
//
//     auto VulkanTexture::Release() -> void {
//         if (!m_IsAllocated) {
//             return;
//         }
//
//         dynamic_cast<VulkanDevice*>(m_Device)->WaitIdle();
//
//         vkDestroyImageView( dynamic_cast<VulkanDevice*>(m_Device)->GetLogicalDevice(), m_ImageView, nullptr );
//
//         if (!m_IsImageExternal) {
//             vmaDestroyImage( dynamic_cast<VulkanDevice*>(m_Device)->GetAllocator(), m_Image, m_Allocation );
//         }
//
//         m_IsAllocated = false;
//     }
//
//     VulkanTexture::~VulkanTexture() {
//         if (!m_IsAllocated) {
//             Release();
//         }
//     }
// }// namespace Mikoto