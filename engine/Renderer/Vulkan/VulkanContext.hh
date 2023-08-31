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
#include <volk.h>
#include <vk_mem_alloc.h>

// Project Headers
#include <Utility/Common.hh>
#include <Utility/Types.hh>
#include <Platform/Window/MainWindow.hh>
#include <Platform/Window/Window.hh>
#include <Renderer/Vulkan/VulkanSwapChain.hh>

namespace Mikoto::VulkanContext {
    /**
     * The Vulkan context space that exposes Vulkan related functionality
     * and provide and interface to interact with the Vulkan API.
     * This is a way to abstract away required data like Physical or Logical devices
     * which is frequently required by objects like index buffers, vertex buffers, etc.
     * */

    /*************************************************************
    * PUBLIC INTERFACE
    * ***********************************************************/
    auto Init(const std::shared_ptr<Window> &handle) -> void;
    auto ShutDown() -> void;
    auto Present() -> void;

    auto EnableVSync() -> void;
    auto DisableVSync() -> void;
    MKT_NODISCARD auto IsVSyncActive() -> bool;

    /*************************************************************
        * STRUCTURES
        * ***********************************************************/
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

        std::shared_ptr<MainWindow> WindowHandle{};// Vulkan window uses a MainWindow for now
        const bool EnableValidationLayers{};
    };

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR Capabilities{};
        std::vector<VkSurfaceFormatKHR> Formats{};
        std::vector<VkPresentModeKHR> PresentModes{};
    };

    struct QueuesData {
        VkQueue GraphicsQueue{};
        UInt32_T GraphicsFamilyIndex{};

        VkQueue PresentQueue{};
        UInt32_T PresentFamilyIndex{};

        bool GraphicsFamilyHasValue{false};
        bool PresentFamilyHasValue{false};

        MKT_NODISCARD auto IsComplete() const -> bool { return GraphicsFamilyHasValue && PresentFamilyHasValue; }
    };

    /*************************************************************
    * CONTEXT DATA
    * ***********************************************************/
    inline ContextData s_ContextData {
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

    inline std::shared_ptr<VulkanSwapChain> s_SwapChain{};
    inline VmaAllocator s_DefaultAllocator{};

    // This information is physical device specific
    inline std::vector<SwapChainSupportDetails> s_SwapChainSupportDetails{};
    inline std::vector<QueuesData> s_QueueFamiliesData{};

    /*************************************************************
    * CONTEXT FUNCTIONS
    * ***********************************************************/
    MKT_NODISCARD inline auto GetSurface() -> VkSurfaceKHR & { return s_ContextData.Surface; }
    MKT_NODISCARD inline auto GetInstance() -> VkInstance & { return s_ContextData.Instance; }
    MKT_NODISCARD inline auto GetGlfwRequiredExtensions() -> std::vector<const char *>;
    MKT_NODISCARD inline auto GetDefaultAllocator() -> VmaAllocator & { return s_DefaultAllocator; }

    /*************************************************************
        * UTILITY FUNCTIONS
        * ********************************************************+ */
    MKT_NODISCARD auto IsExtensionAvailable(std::string_view targetExtensionName, VkPhysicalDevice device) -> bool;
    MKT_NODISCARD auto QuerySwapChainSupport(VkPhysicalDevice device) -> SwapChainSupportDetails;
    MKT_NODISCARD auto IsDeviceSuitable(VkPhysicalDevice device) -> bool;


    /*************************************************************
    * PRIMARY DEVICE FUNCTIONS
    * ***********************************************************/
    MKT_NODISCARD inline auto GetPrimaryPhysicalDevice() -> VkPhysicalDevice { return s_ContextData.PhysicalDevices[s_ContextData.PrimaryPhysicalDeviceIndex]; };
    MKT_NODISCARD inline auto GetPrimaryLogicalDevice() -> VkDevice { return s_ContextData.LogicalDevices[s_ContextData.PrimaryLogicalDeviceIndex]; }
    MKT_NODISCARD inline auto GetPrimaryLogicalDeviceFeatures() -> VkPhysicalDeviceFeatures { return s_ContextData.PhysicalDeviceFeatures[s_ContextData.PrimaryLogicalDeviceIndex]; }
    MKT_NODISCARD inline auto GetPrimaryLogicalDeviceProperties() -> VkPhysicalDeviceProperties { return s_ContextData.PhysicalDeviceProperties[s_ContextData.PrimaryLogicalDeviceIndex]; }
    MKT_NODISCARD inline auto GetPrimaryLogicalDeviceMemoryProperties() -> VkPhysicalDeviceMemoryProperties { return s_ContextData.PhysicalDeviceMemoryProperties[s_ContextData.PrimaryLogicalDeviceIndex]; }
    MKT_NODISCARD inline auto GetPrimaryLogicalDeviceGraphicsQueue() -> VkQueue { return s_QueueFamiliesData[s_ContextData.PrimaryLogicalDeviceIndex].GraphicsQueue; }
    MKT_NODISCARD inline auto GetPrimaryLogicalDevicePresentQueue() -> VkQueue { return s_QueueFamiliesData[s_ContextData.PrimaryLogicalDeviceIndex].PresentQueue; }
    MKT_NODISCARD inline auto GetPrimaryLogicalDeviceQueuesData() -> QueuesData { return s_QueueFamiliesData[s_ContextData.PrimaryLogicalDeviceIndex]; }
    MKT_NODISCARD inline auto GetPrimaryLogicalDeviceSwapChainSupport() -> SwapChainSupportDetails { return QuerySwapChainSupport(s_ContextData.PhysicalDevices[s_ContextData.PrimaryLogicalDeviceIndex]); }

    /*************************************************************
    * SWAPCHAIN
    * ***********************************************************/
    auto InitSwapChain() -> void;
    auto RecreateSwapChain(const VulkanSwapChainCreateInfo &info = VulkanSwapChain::GetDefaultCreateInfo()) -> void;
    MKT_NODISCARD inline auto GetSwapChain() -> std::shared_ptr<VulkanSwapChain> { return s_SwapChain; }

    /**
     * returns true if the device supports all the listed extensions in s_DeviceRequiredExtensions
     * */
    MKT_NODISCARD auto CheckForDeviceRequiredExtensionSupport(VkPhysicalDevice device) -> bool;

    /**
     * Returns the indices for the Graphics Queue family and Present queue family
     * */
    MKT_NODISCARD auto FindQueueFamilies(VkPhysicalDevice device) -> QueuesData;
    MKT_NODISCARD auto FindMemoryType(UInt32_T typeFilter, VkMemoryPropertyFlags properties, VkPhysicalDevice device) -> UInt32_T;
    MKT_NODISCARD auto FindSupportedFormat(VkPhysicalDevice device, const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features) -> VkFormat;

    /**
     * Will return true if at least one of the operations specified by flags is supported by the queue family
     * */
    MKT_NODISCARD inline auto QueueFamilySupportsOperation(VkQueueFamilyProperties queueFamily, VkQueueFlagBits flags) -> bool { return queueFamily.queueFlags & flags; }
    MKT_NODISCARD inline auto QueueFamilySupportsGraphicsOperations(VkQueueFamilyProperties queueFamily, VkQueueFlagBits flags) -> bool { return queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT; }
    MKT_NODISCARD inline auto QueueFamilySupportsComputeOperations(VkQueueFamilyProperties queueFamily, VkQueueFlagBits flags) -> bool { return queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT; }
    MKT_NODISCARD inline auto QueueFamilySupportsTransferOperations(VkQueueFamilyProperties queueFamily, VkQueueFlagBits flags) -> bool { return queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT; }


    /*************************************************************
    * FOR INTERNAL USAGE
    * ***********************************************************/
    auto SetupPhysicalDevicesData() -> void;
    auto CreateInstance() -> void;
    auto SetupDebugMessenger() -> void;
    auto CreateSurface() -> void;
    auto PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo) -> void;
    auto DisplayGflwRequiredInstanceExtensions() -> void;
    auto CheckValidationLayerSupport() -> bool;
    auto PickPrimaryPhysicalDevice() -> void;
    auto CreatePrimaryLogicalDevice() -> void;
    auto InitMemoryAllocator() -> void;

    /*************************************************************
    * GENERAL DEVICE REQUIRED EXTENSIONS
    * ***********************************************************/
    inline const std::vector<const char *> s_ValidationLayers{"VK_LAYER_KHRONOS_validation"};
    inline const std::vector<const char *> s_DeviceRequiredExtensions{
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,

            // Passing your vertex data jus like in OpenGL, using the same state (as the pipeline setup)
            // and shaders as in OpenGL, your scene will likely not display as you’d expect.
            // The viewport’s origin in OpenGL is in the lower left of the screen, with Y pointing up.
            // In Vulkan the origin is in the top left of the screen, with Y pointing downwards.
            // Starting from Vulkan 1.1 though, this feature is part of core Vulkan, so checking for it is not really necessary
            // See: https://www.saschawillems.de/blog/2019/03/29/flipping-the-vulkan-viewport/
            VK_KHR_MAINTENANCE1_EXTENSION_NAME,

            //VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME,
    };
}

#endif // MIKOTO_VULKAN_CONTEXT_HH