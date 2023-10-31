/**
 * VulkanContext.hh
 * Created by kate on 7/3/23.
 * */

#ifndef MIKOTO_VULKAN_CONTEXT_HH
#define MIKOTO_VULKAN_CONTEXT_HH

// C++ Standard Library
#include <any>
#include <vector>

// Third-Party Libraries
#include "vk_mem_alloc.h"
#include "volk.h"

// Project Headers
#include "Common/Common.hh"
#include "Common/Types.hh"
#include <Common/VulkanUtils.hh>

#include "Platform/GlfwWindow.hh"
#include "Platform/Window.hh"
#include "VulkanCommandPool.hh"
#include "VulkanSwapChain.hh"

/**
 * The Vulkan context space that exposes Vulkan related functionality
 * and provide and interface to interact with the Vulkan API.
 * This is a way to abstract away required data like Physical or Logical devices
 * which is frequently required by objects like index buffers, vertex buffers, etc.
 * */

// TODO: turn into static class for better organization
namespace Mikoto {
    class VulkanContext {
    public:
        /**
         * Initializes the Vulkan API for usage and initializes the context with some
         * commonly required objects like a physical device, logical device, a swapchain, etc.
         * @param handle window handle required to create a valid context
         * */
        static auto Init(const std::shared_ptr<Window>& handle) -> void;
        static auto Shutdown() -> void;

        static auto Present() -> void;
        static auto PrepareFrame() -> void;
        static auto SubmitFrame() -> void;

        static auto EnableVSync() -> void;
        static auto DisableVSync() -> void;
        MKT_NODISCARD static auto IsVSyncActive() -> bool;

        /**
         * This structure wraps all the data managed by the Vulkan Context, such as
         * the physical devices, logical devices, control to whether we want validation layers or not.
         * */
        struct ContextData {
            std::vector<VkPhysicalDevice> PhysicalDevices{};
            std::vector<VkDevice> LogicalDevices{};

            UInt32_T PrimaryPhysicalDeviceIndex{};
            UInt32_T PrimaryLogicalDeviceIndex{};

            std::vector<VkPhysicalDeviceFeatures> PhysicalDeviceFeatures{};
            std::vector<VkPhysicalDeviceProperties> PhysicalDeviceProperties{};
            std::vector<VkPhysicalDeviceMemoryProperties> PhysicalDeviceMemoryProperties{};

            VkInstance Instance{};
            VkSurfaceKHR Surface{};
            VkDebugUtilsMessengerEXT DebugMessenger{};

            bool VOLKInitSuccess{};

            std::shared_ptr<GlfwWindow> WindowHandle{};// Vulkan window uses a MainWindow for now
            const bool EnableValidationLayers{};
        };

        // Used for short-lived commands
        struct ImmediateSubmitContext {
            VkFence UploadFence{};            // Notify the host a task has finished executing
            VulkanCommandPool CommandPool{};  // Command pool to allocate command buffer from
            VkCommandBuffer CommandBuffer{};  // Command buffer to submit work to
        };


        /*************************************************************
         * CONTEXT FUNCTIONS
         * ***********************************************************/
        MKT_NODISCARD static inline auto GetSurface() -> VkSurfaceKHR & { return s_ContextData.Surface; }
        MKT_NODISCARD static inline auto GetInstance() -> VkInstance & { return s_ContextData.Instance; }
        MKT_NODISCARD static auto GetGlfwRequiredExtensions() -> std::vector<const char *>;
        MKT_NODISCARD static inline auto GetDefaultAllocator() -> VmaAllocator & { return s_DefaultAllocator; }

        MKT_NODISCARD static inline auto GetDrawCommandBuffersHandles() -> std::vector<VkCommandBuffer> & { return s_RenderCommandBufferHandles; }
        MKT_NODISCARD static inline auto GetCurrentImageIndex() -> UInt32_T { return s_CurrentImageIndex; }


        static auto CreatePrimaryLogicalDeviceCommandPools() -> void;
        static auto CreatePrimaryLogicalDeviceCommandBuffers() -> void;
        static auto CreateSynchronizationPrimitives() -> void;

        static auto SubmitToQueue(const QueueSubmitInfo &submitInfo) -> void;
        static auto SubmitCommands(const VkCommandBuffer *commands, const UInt32_T count) -> void;

        static auto BatchCommandBuffer(VkCommandBuffer cmd) -> void;

        /**
         * Slow, use mainly for debug.
         * */
        MKT_NODISCARD static auto GetDetailedStatistics() -> const VmaTotalStatistics &;

        /*************************************************************
         * UTILITY FUNCTIONS
         * ********************************************************+ */
        MKT_NODISCARD static auto IsExtensionAvailable(std::string_view targetExtensionName, VkPhysicalDevice device) -> bool;
        MKT_NODISCARD static auto QuerySwapChainSupport(VkPhysicalDevice device) -> SwapChainSupportDetails;
        MKT_NODISCARD static auto IsDeviceSuitable(VkPhysicalDevice device) -> bool;

        static auto ImmediateSubmit(const std::function<void(VkCommandBuffer)>& task, VkQueue queue = GetPrimaryLogicalDeviceGraphicsQueue()) -> void;


        /*************************************************************
         * PRIMARY LOGICAL DEVICE GETTERS
         * ***********************************************************/
        MKT_NODISCARD static inline auto GetPrimaryPhysicalDevice() -> VkPhysicalDevice { return s_ContextData.PhysicalDevices[s_ContextData.PrimaryPhysicalDeviceIndex]; };
        MKT_NODISCARD static inline auto GetPrimaryLogicalDevice() -> VkDevice { return s_ContextData.LogicalDevices[s_ContextData.PrimaryLogicalDeviceIndex]; }
        MKT_NODISCARD static inline auto GetPrimaryLogicalDeviceFeatures() -> VkPhysicalDeviceFeatures { return s_ContextData.PhysicalDeviceFeatures[s_ContextData.PrimaryLogicalDeviceIndex]; }
        MKT_NODISCARD static inline auto GetPrimaryLogicalDeviceProperties() -> VkPhysicalDeviceProperties { return s_ContextData.PhysicalDeviceProperties[s_ContextData.PrimaryLogicalDeviceIndex]; }
        MKT_NODISCARD static inline auto GetPrimaryLogicalDeviceMemoryProperties() -> VkPhysicalDeviceMemoryProperties { return s_ContextData.PhysicalDeviceMemoryProperties[s_ContextData.PrimaryLogicalDeviceIndex]; }
        MKT_NODISCARD static inline auto GetPrimaryLogicalDeviceGraphicsQueue() -> VkQueue { return s_QueueFamiliesData[s_ContextData.PrimaryLogicalDeviceIndex].GraphicsQueue; }
        MKT_NODISCARD static inline auto GetPrimaryLogicalDevicePresentQueue() -> VkQueue { return s_QueueFamiliesData[s_ContextData.PrimaryLogicalDeviceIndex].PresentQueue; }
        MKT_NODISCARD static inline auto GetPrimaryLogicalDeviceQueuesData() -> QueuesData { return s_QueueFamiliesData[s_ContextData.PrimaryLogicalDeviceIndex]; }
        MKT_NODISCARD static inline auto GetPrimaryLogicalDeviceSwapChainSupport() -> SwapChainSupportDetails { return QuerySwapChainSupport(s_ContextData.PhysicalDevices[s_ContextData.PrimaryLogicalDeviceIndex]); }

        /*************************************************************
         * PRIMARY PHYSICAL DEVICE GETTERS
         * ***********************************************************/
        MKT_NODISCARD static inline auto GetPrimaryPhysicalDeviceFeatures() -> VkPhysicalDeviceFeatures { return s_ContextData.PhysicalDeviceFeatures[s_ContextData.PrimaryPhysicalDeviceIndex]; }
        MKT_NODISCARD static inline auto GetPrimaryPhysicalDeviceProperties() -> VkPhysicalDeviceProperties { return s_ContextData.PhysicalDeviceProperties[s_ContextData.PrimaryPhysicalDeviceIndex]; }
        MKT_NODISCARD static inline auto GetPrimaryPhysicalDeviceMemoryProperties() -> VkPhysicalDeviceMemoryProperties { return s_ContextData.PhysicalDeviceMemoryProperties[s_ContextData.PrimaryPhysicalDeviceIndex]; }


        /*************************************************************
         * SWAPCHAIN
         * ***********************************************************/
        static auto InitSwapChain() -> void;
        static auto RecreateSwapChain(VulkanSwapChainCreateInfo &&info) -> void;
        MKT_NODISCARD static inline auto GetSwapChain() -> std::shared_ptr<VulkanSwapChain> { return s_SwapChain; }

        /**
         * returns true if the device supports all the listed extensions in s_DeviceRequiredExtensions
         * */
        MKT_NODISCARD static auto CheckForDeviceRequiredExtensionSupport(VkPhysicalDevice device) -> bool;

        /**
         * Returns the indices for the Graphics Queue family and Present queue family
         * */
        MKT_NODISCARD static auto FindQueueFamilies(VkPhysicalDevice device) -> QueuesData;
        MKT_NODISCARD static auto FindMemoryType(UInt32_T typeFilter, VkMemoryPropertyFlags properties, VkPhysicalDevice device) -> UInt32_T;
        MKT_NODISCARD static auto FindSupportedFormat(VkPhysicalDevice device, const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features) -> VkFormat;

        /**
         * Will return true if at least one of the operations specified by flags is supported by the queue family
         * */
        MKT_NODISCARD static inline auto QueueFamilySupportsOperation(VkQueueFamilyProperties queueFamily, VkQueueFlagBits flags) -> bool { return queueFamily.queueFlags & flags; }
        MKT_NODISCARD static inline auto QueueFamilySupportsGraphicsOperations(VkQueueFamilyProperties queueFamily, VkQueueFlagBits flags) -> bool { return queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT; }
        MKT_NODISCARD static inline auto QueueFamilySupportsComputeOperations(VkQueueFamilyProperties queueFamily, VkQueueFlagBits flags) -> bool { return queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT; }
        MKT_NODISCARD static inline auto QueueFamilySupportsTransferOperations(VkQueueFamilyProperties queueFamily, VkQueueFlagBits flags) -> bool { return queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT; }

    private:
        static auto SwitchVSync_H(bool value) -> void;

    private:
        static auto CreateSurface() -> void;
        static auto CreateInstance() -> void;
        static auto SetupDebugMessenger() -> void;
        static auto InitMemoryAllocator() -> void;
        static auto SetupPhysicalDevicesData() -> void;
        static auto PickPrimaryPhysicalDevice() -> void;
        static auto CreatePrimaryLogicalDevice() -> void;
        static auto CheckValidationLayerSupport() -> bool;
        static auto DisplayGflwRequiredInstanceExtensions() -> void;
        static auto PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo) -> void;

    private:
        // Context main data, such as device, device properties, etc
        inline static ContextData s_ContextData {
            .PhysicalDevices{},
            .LogicalDevices{},

            .PrimaryPhysicalDeviceIndex{},
            .PrimaryLogicalDeviceIndex{},

            .PhysicalDeviceFeatures{},
            .PhysicalDeviceProperties{},
            .PhysicalDeviceMemoryProperties{},

            .Instance{},
            .Surface{},
            .DebugMessenger{},

            .VOLKInitSuccess{},
            .WindowHandle{},
#if defined(NDEBUG)
            // Disavle validation layers for non-debug builds
            .EnableValidationLayers = false,
#else
            .EnableValidationLayers = true,
#endif
        };

        // Global VAM Allocator
        inline static VmaAllocator s_DefaultAllocator{};

        // VMA allocator stats. Avoid refreshing often as it drains performance, mainly useful for debugging
        inline static VmaTotalStatistics s_TotalStatistics{};

        // Family queue info (one per device)
        inline static std::vector<QueuesData> s_QueueFamiliesData{};

        // Swap chain support info
        inline static std::vector<SwapChainSupportDetails> s_SwapChainSupportDetails{};

        // Contains all command buffers to be submitted to a graphics queue
        inline static std::vector<VkCommandBuffer> s_BatchedGraphicQueueCommands{};

        // Index to an valid swap chain image we can render to
        inline static UInt32_T s_CurrentImageIndex{};

        // Main command pool
        inline static VulkanCommandPool s_MainCommandPool{};

        // Single swap chain
        inline static std::shared_ptr<VulkanSwapChain> s_SwapChain{};

        // Synchronization primitives to synchronize rendering and presentation operations
        inline static FrameSynchronizationPrimitives s_SwapChainSyncObjects{ VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};

        // Contains the actual VkCommandBuffer handles, easier to pass to Vulkan structures such as VkSubmitInfo
        inline static std::vector<VkCommandBuffer> s_RenderCommandBufferHandles{};

        inline static ImmediateSubmitContext s_ImmediateSubmitContext{ VK_NULL_HANDLE, VulkanCommandPool{}, VK_NULL_HANDLE };

        // Target validation layers
        inline static const std::vector<const char *> s_ValidationLayers{ "VK_LAYER_KHRONOS_validation" };

        // Required application extensions
        inline static const std::vector<const char *> s_DeviceRequiredExtensions{
                VK_KHR_SWAPCHAIN_EXTENSION_NAME,

                // Passing your vertex data just like in OpenGL, using the same state (as the pipeline setup)
                // and shaders as in OpenGL, your scene will likely not display as you’d expect.
                // The viewport’s origin in OpenGL is in the lower left of the screen, with Y pointing up.
                // In Vulkan the origin is in the top left of the screen, with Y pointing downwards.
                // Starting from Vulkan 1.1 though, this feature is part of core Vulkan, so checking for it is not really necessary
                // See: https://www.saschawillems.de/blog/2019/03/29/flipping-the-vulkan-viewport/
                VK_KHR_MAINTENANCE1_EXTENSION_NAME,

                //VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME,
        };
    };
}

#endif // MIKOTO_VULKAN_CONTEXT_HH