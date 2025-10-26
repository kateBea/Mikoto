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

        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;

        ~VulkanSampler() override;

    private:
        auto Release() -> void override;
        auto Initialize() -> void override;

    private:
        VkSampler m_Sampler{ VK_NULL_HANDLE };
        VkSamplerCreateInfo m_CreateInfo{};
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
        explicit VulkanTexture( const VkImageViewCreateInfo& viewCreateInfo, VkExtent2D extent);

        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;

        MKT_NODISCARD auto GetImplHandle() -> VkImage* { return std::addressof(m_Image); }

        MKT_NODISCARD auto GetImage() -> VkImage*;
        MKT_NODISCARD auto GetImage() const -> const VkImage*;

        MKT_NODISCARD auto GetView() -> VkImageView*;
        MKT_NODISCARD auto GetView() const -> const VkImageView*;

        MKT_NODISCARD auto HasExternalImage() const -> bool;

        MKT_NODISCARD auto GetCurrentLayout() const -> VkImageLayout;
        MKT_NODISCARD auto GetCreateInfo() const -> const VkImageCreateInfo&;
        MKT_NODISCARD auto GetViewCreateInfo() const -> const VkImageViewCreateInfo&;

        MKT_NODISCARD auto IsSwapChainImage() const -> bool;

        auto SetTextureIndex( Int32 index ) -> void;
        MKT_NODISCARD auto GetTextureIndex() const -> Int32;
        MKT_NODISCARD auto HasBindlessIndex() const -> bool;

        auto SubmitLayoutTransition( VkImageLayout newLayout, VkCommandBuffer cmd ) -> void;

        auto GetVMAllocation() -> VmaAllocation*;
        auto GetVMAllocationInfo() -> VmaAllocationInfo*;
        auto GetImageCreateInfo() -> const VkImageCreateInfo*;
        auto GetAllocationCreateInfo() -> const VmaAllocationCreateInfo*;

        ~VulkanTexture() override;

    private:
        auto Initialize() -> void override;
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

        // For Dynamic rendering
        // Set by the device when created
        Int32 m_TextureArrayIndex{ -1 };
    };

    struct VulkanSwapChainCreateInfo {
        VkExtent2D Extent{};
        VkSurfaceKHR* Surface{};
        bool EnableVsync{ false };
    };

    class VulkanSwapChain final : public DeviceObject {
    public:
        explicit VulkanSwapChain( const VulkanSwapChainCreateInfo& createInfo );

        MKT_NODISCARD auto GetImplHandle() -> VkSwapchainKHR* { return std::addressof(m_Swapchain); }

        MKT_NODISCARD auto Present( UInt32 imageIndex, const VkSemaphore& renderFinished ) -> VkResult;

        MKT_NODISCARD auto GetImageCount() const -> Size;

        MKT_NODISCARD auto GetImage( Size index ) -> TextureHandle;

        MKT_NODISCARD auto GetExtent() const -> VkExtent2D;

        MKT_NODISCARD auto IsVsyncEnabled() const -> bool;

        MKT_NODISCARD auto GetCurrentFrameIndex() const -> UInt32 { return m_CurrentFrame; }

        MKT_NODISCARD auto GetNextRenderableImage( UInt32& imageIndex, VkFence fence = VK_NULL_HANDLE, VkSemaphore imageAvailable = VK_NULL_HANDLE ) const -> VkResult;

        // TODO: Destroy this swap chain and create a new one
        // you do not really want to do the swap chain of creating a new one, that is maybe in the cae you have another window
        // but if you want to recreate this swap chain because a resizing of the surface happened, then you don't need a new one
        // as in new VulkanSwapChain instance you just need a "recreate logic"
        auto OnResize(VkExtent2D newDimensions, bool vsync = false) -> void;

        ~VulkanSwapChain() override;

        DISABLE_COPY_AND_MOVE_FOR( VulkanSwapChain );

    private:
        auto Release() -> void override;
        auto Initialize() -> void override;

        auto CreateSwpChain() -> void;
        auto GetImages() -> void;

        MKT_NODISCARD auto ChoosePresentMode( const std::vector<VkPresentModeKHR>& availablePresentModes ) const -> VkPresentModeKHR;
        MKT_NODISCARD auto ChooseExtent( const VkSurfaceCapabilitiesKHR& capabilities ) const -> VkExtent2D;

        MKT_NODISCARD static auto ConstructImgViewInfo( VkImage image, const VkFormat& format ) -> VkImageViewCreateInfo;
        MKT_NODISCARD static auto ChooseSurfaceFormat( const std::vector<VkSurfaceFormatKHR>& availableFormats ) -> VkSurfaceFormatKHR;

    private:
        /**
         * Maximum number of frames that can be processed concurrently.
         * */
        static constexpr Int32 MAX_FRAMES_IN_FLIGHT{ 2 };

    private:
        VkExtent2D m_Extent{};
        VkSwapchainKHR m_Swapchain{ VK_NULL_HANDLE };

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
