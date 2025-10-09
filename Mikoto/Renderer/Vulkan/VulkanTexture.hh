/**
 * VulkanTexture.hh
 * Created by kate on 7/5/2023.
 * */

#ifndef MIKOTO_VULKAN_TEXTURE2D_HH
#define MIKOTO_VULKAN_TEXTURE2D_HH

// C++ Standard Library
#include <filesystem>

// Third-Party Libraries
#include <stb_image.h>
#include <volk.h>
#include <vk_mem_alloc.h>

// Project Headers
#include <Common/Common.hh>
#include <Library/IO/File.hh>
#include <Library/Utility/Types.hh>
#include <Material/Texture2D.hh>

namespace Mikoto {

    /**
    * @brief Represents a sampler object used for texture sampling.
    *
    * This class encapsulates the functionality of a sampler, allowing for
    * texture sampling with various filtering and wrapping modes.
    */
    class VulkanSampler final : public Sampler {
    public:
        explicit VulkanSampler( const SamplerDescription& desc );

        MKT_NODISCARD auto GetImplHandle() -> VkSampler* { return std::addressof(m_Sampler); }

        auto GetSampler() const -> VkSampler { return m_Sampler; }
        ~VulkanSampler() override;

    private:
        auto Release() -> void override;
        auto Allocate() -> void override;

    private:
        VkSampler m_Sampler{ VK_NULL_HANDLE };
    };

    /**
     * @class VulkanTexture
     * @brief Represents a 2D texture used in Vulkan renderer.
     *
     * Extends the Texture2D class for Vulkan-specific texture functionality. It manages loading and handling
     * 2D textures for Vulkan rendering, including creating images, image views, samplers, and descriptor sets.
     * */
    class VulkanTexture final : public Texture2D {
    public:
        explicit VulkanTexture( const TextureDescription& data );
        explicit VulkanTexture( VkImageViewCreateInfo viewCreateInfo );

        MKT_NODISCARD auto GetImplHandle() -> VkImage* { return std::addressof(m_Image); }

        MKT_NODISCARD auto GetImage() -> VkImage*;
        MKT_NODISCARD auto GetImage() const -> const VkImage*;

        MKT_NODISCARD auto GetView() -> VkImageView*;
        MKT_NODISCARD auto GetView() const -> const VkImageView*;

        MKT_NODISCARD auto HasExternalImage() const -> bool;

        MKT_NODISCARD auto GetCurrentLayout() const -> VkImageLayout;
        MKT_NODISCARD auto GetCreateInfo() const -> const VkImageCreateInfo&;
        MKT_NODISCARD auto GetViewCreateInfo() const -> const VkImageViewCreateInfo&;

        auto SubmitLayoutTransition( VkImageLayout newLayout, VkCommandBuffer cmd ) -> void;

        auto GetVMAllocation() -> VmaAllocation*;
        auto GetVMAllocationInfo() -> VmaAllocationInfo*;
        auto GetImageCreateInfo() -> const VkImageCreateInfo*;
        auto GetAllocationCreateInfo() -> const VmaAllocationCreateInfo*;

        ~VulkanTexture() override;

    private:
        auto AllocateImage() -> void;
        auto Allocate() -> void override;
        auto Release() -> void override;

    private:
        bool m_IsImageExternal{ false };

        BufferHandle m_StagingBuffer{};

        VkDeviceSize m_ImageSize{ 0 };

        VkImage m_Image{ VK_NULL_HANDLE };
        VkImageView m_ImageView{ VK_NULL_HANDLE };

        VmaAllocation m_Allocation{ VK_NULL_HANDLE };
        VmaAllocationInfo m_AllocationInfo{};

        VkImageCreateInfo m_ImageCreateInfo{};
        VkImageViewCreateInfo m_ImageViewCreateInfo{};

        VmaAllocationCreateInfo m_AllocationCreateInfo{};

        VkImageLayout m_CurrentLayout{ VK_IMAGE_LAYOUT_UNDEFINED };
    };

    struct VulkanSwapChainCreateInfo {
        VkExtent2D Extent{};
        VkSurfaceKHR* Surface{};
        bool EnableVsync{ false };
        VkSwapchainKHR OldSwapChain{ VK_NULL_HANDLE };
    };

    class VulkanSwapChain final : public DeviceObject {
    public:
        explicit VulkanSwapChain( const VulkanSwapChainCreateInfo& createInfo );

        MKT_NODISCARD auto GetImplHandle() -> VkSwapchainKHR* { return std::addressof(m_Swapchain); }

        MKT_NODISCARD auto Present( UInt32 imageIndex, const VkSemaphore& renderFinished ) -> VkResult;

        MKT_NODISCARD auto GetImageCount() const -> Size;

        MKT_NODISCARD auto GetImage( Size index ) -> VulkanTexture&;
        MKT_NODISCARD auto GetImage( Size index ) const -> const VulkanTexture&;

        MKT_NODISCARD auto GetSwapChainKHR() const -> VkSwapchainKHR;

        MKT_NODISCARD auto GetExtent() const -> VkExtent2D;

        MKT_NODISCARD auto IsVsyncEnabled() const -> bool;

        MKT_NODISCARD auto GetNextRenderableImage( UInt32& imageIndex, VkFence fence = VK_NULL_HANDLE, VkSemaphore imageAvailable = VK_NULL_HANDLE ) const -> VkResult;

        ~VulkanSwapChain() override;

        DISABLE_COPY_AND_MOVE_FOR( VulkanSwapChain );

    private:
        auto Release() -> void override;
        auto Allocate() -> void override;

        auto CreateSwapChain() -> void;
        auto AcquireSwapchainImages() -> void;

        MKT_NODISCARD auto ChoosePresentMode( const std::vector<VkPresentModeKHR>& availablePresentModes ) const -> VkPresentModeKHR;
        MKT_NODISCARD auto ChooseExtent( const VkSurfaceCapabilitiesKHR& capabilities ) const -> VkExtent2D;

        MKT_NODISCARD static auto CreateSwapchainImageViewCreateInfo( VkImage image, const VkFormat& format ) -> VkImageViewCreateInfo;
        MKT_NODISCARD static auto ChooseSurfaceFormat( const std::vector<VkSurfaceFormatKHR>& availableFormats ) -> VkSurfaceFormatKHR;

    private:
        /**
         * Maximum number of frames that can be processed concurrently.
         * */
        static constexpr Int32 MAX_FRAMES_IN_FLIGHT{ 2 };

    private:
        VkExtent2D m_Extent{};
        VkSwapchainKHR m_Swapchain{ VK_NULL_HANDLE };
        VkSwapchainKHR m_OldSwapChain{ VK_NULL_HANDLE };

        VkFormat m_Format{};
        VkPresentModeKHR m_PresentMode{};

        std::vector<TextureHandle> m_Images{};

        VkSurfaceKHR* m_Surface{ nullptr };

        bool m_IsVsyncEnabled{};

        Size m_CurrentFrame{};
    };

    using SwapChainHandle = Ref<VulkanSwapChain>;
}// namespace Mikoto

#endif// MIKOTO_VULKAN_TEXTURE2D_HH
