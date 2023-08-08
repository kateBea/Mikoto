/**
* VulkanContext.hh
* Created by kate on 7/3/23.
* */


#ifndef KATE_ENGINE_VULKAN_CONTEXT_HH
#define KATE_ENGINE_VULKAN_CONTEXT_HH

// C++ Standard Library
#include <any>
#include <vector>

// Third-Party Libraries
#include <volk.h>
#include <vk_mem_alloc.h>

// Project Headers
#include <Utility/Common.hh>
#include <Utility/Types.hh>

#include <Renderer/Vulkan/VulkanSwapChain.hh>
#include <Platform/Window/MainWindow.hh>
#include <Platform/Window/Window.hh>

namespace kaTe {
    /**
     * The Vulkan context class is the way of the app to encapsulate Vulkan related functionality
     * and provide and interface to interact with the Vulkan API.
     * This is a way to abstract away required data like Physical or Logical devices
     * which is frequently required by objects like index buffers, vertex buffers, etc.
     * */
    class VulkanContext {
    public:
        /*************************************************************
        * PUBLIC INTERFACE
        * ********************************************************+ */
        static auto Init(const std::shared_ptr<Window>& handle) -> void;
        static auto ShutDown() -> void;
        static auto Present() -> void;

        static auto EnableVSync() -> void;
        static auto DisableVSync() -> void;
        KT_NODISCARD static auto IsVSyncActive() -> bool;

    private:
        /*************************************************************
        * FRIENDS
        *
        * These classes will have access to the internal Vulkan
        * Context functionality since no other classes really need them
        * ********************************************************+ */

        friend class VulkanVertexBuffer;
        friend class VulkanPipeline;
        friend class VulkanRenderer;
        friend class VulkanShader;
        friend class VulkanSwapChain;
        friend class VulkanVertexBuffer;
        friend class VulkanIndexBuffer;
        friend class VulkanCommandPool;
        friend class VulkanTexture2D;
        friend class VulkanFrameBuffer;
        friend class VulkanStandardMaterial;
        friend class VulkanUtils;
        friend class ImGuiLayer;
        friend class ScenePanel;

    private:
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

            std::shared_ptr<MainWindow> WindowHandle{}; // Vulkan window uses a MainWindow for now
            const bool EnableValidationLayers{};
        };

        struct SwapChainSupportDetails {
            VkSurfaceCapabilitiesKHR        Capabilities{};
            std::vector<VkSurfaceFormatKHR> Formats{};
            std::vector<VkPresentModeKHR>   PresentModes{};
        };

        struct QueuesData {
            VkQueue     GraphicsQueue{};
            UInt32_T    GraphicsFamilyIndex{};

            VkQueue     PresentQueue{};
            UInt32_T    PresentFamilyIndex{};

            bool GraphicsFamilyHasValue{ false };
            bool PresentFamilyHasValue{ false };

            KT_NODISCARD auto IsComplete() const -> bool { return GraphicsFamilyHasValue && PresentFamilyHasValue; }
        };

        /*************************************************************
        * CONTEXT FUNCTIONS
        * ********************************************************+ */
        KT_NODISCARD static auto GetSurface() -> VkSurfaceKHR& { return s_ContextData.Surface; }
        KT_NODISCARD static auto GetInstance() -> VkInstance& { return s_ContextData.Instance; }
        KT_NODISCARD static auto GetGlfwRequiredExtensions() -> std::vector<const char*>;

        static auto CreateInstance() -> void;
        static auto SetupDebugMessenger() -> void;
        static auto CreateSurface() -> void;
        static auto PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) -> void;
        static auto DisplayGflwRequiredInstanceExtensions() -> void;
        static auto CheckValidationLayerSupport() -> bool;
        static auto PickPrimaryPhysicalDevice() -> void;
        static auto CreatePrimaryLogicalDevice() -> void;
        static auto InitMemoryAllocator() -> void;

        KT_NODISCARD static auto GetDefaultAllocator() -> VmaAllocator& { return s_DefaultAllocator; }

        
        /*************************************************************
        * PRIMARY DEVICE FUNCTIONS
        * ********************************************************+ */
        KT_NODISCARD static auto GetPrimaryPhysicalDevice() -> VkPhysicalDevice { return s_ContextData.PhysicalDevices[s_ContextData.PrimaryPhysicalDeviceIndex]; };
        KT_NODISCARD static auto GetPrimaryLogicalDevice() -> VkDevice { return s_ContextData.LogicalDevices[s_ContextData.PrimaryLogicalDeviceIndex]; }
        KT_NODISCARD static auto GetPrimaryLogicalDeviceFeatures() -> VkPhysicalDeviceFeatures { return s_ContextData.PhysicalDeviceFeatures[s_ContextData.PrimaryLogicalDeviceIndex]; }
        KT_NODISCARD static auto GetPrimaryLogicalDeviceProperties() -> VkPhysicalDeviceProperties { return s_ContextData.PhysicalDeviceProperties[s_ContextData.PrimaryLogicalDeviceIndex]; }
        KT_NODISCARD static auto GetPrimaryLogicalDeviceMemoryProperties() -> VkPhysicalDeviceMemoryProperties { return s_ContextData.PhysicalDeviceMemoryProperties[s_ContextData.PrimaryLogicalDeviceIndex]; }
        KT_NODISCARD static auto GetPrimaryLogicalDeviceGraphicsQueue() -> VkQueue { return s_QueueFamiliesData[s_ContextData.PrimaryLogicalDeviceIndex].GraphicsQueue; }
        KT_NODISCARD static auto GetPrimaryLogicalDevicePresentQueue() -> VkQueue { return s_QueueFamiliesData[s_ContextData.PrimaryLogicalDeviceIndex].PresentQueue; }
        KT_NODISCARD static auto GetPrimaryLogicalDeviceQueuesData() -> QueuesData { return s_QueueFamiliesData[s_ContextData.PrimaryLogicalDeviceIndex]; }
        KT_NODISCARD static auto GetPrimaryLogicalDeviceSwapChainSupport() -> SwapChainSupportDetails { return QuerySwapChainSupport(s_ContextData.PhysicalDevices[s_ContextData.PrimaryLogicalDeviceIndex]); }

        /*************************************************************
        * SWAPCHAIN
        * ********************************************************+ */
        static auto InitSwapChain() -> void;
        static auto RecreateSwapChain(const VulkanSwapChainCreateInfo& info = VulkanSwapChain::GetDefaultCreateInfo()) -> void;
        KT_NODISCARD static auto GetSwapChain() -> std::shared_ptr<VulkanSwapChain> { return s_SwapChain; }


        /*************************************************************
        * UTILITY FUNCTIONS
        * ********************************************************+ */
        KT_NODISCARD static auto IsExtensionAvailable(std::string_view targetExtensionName, VkPhysicalDevice device) -> bool;
        KT_NODISCARD static auto QuerySwapChainSupport(VkPhysicalDevice device) -> SwapChainSupportDetails;
        KT_NODISCARD static auto IsDeviceSuitable(VkPhysicalDevice device) -> bool;

        /**
         * returns true if the device supports all the listed extensions in s_DeviceRequiredExtensions
         * */
        KT_NODISCARD static auto CheckForDeviceRequiredExtensionSupport(VkPhysicalDevice device) -> bool;

        /**
         * Returns the indices for the Graphics Queue family and Present queue family
         * */
        KT_NODISCARD static auto FindQueueFamilies(VkPhysicalDevice device) -> QueuesData;
        KT_NODISCARD static auto FindMemoryType(UInt32_T typeFilter, VkMemoryPropertyFlags properties, VkPhysicalDevice device) -> UInt32_T;
        KT_NODISCARD static auto FindSupportedFormat(VkPhysicalDevice device, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) -> VkFormat;

        /**
         * Will return true if at least one of the operations specified by flags is supported by the queue family
         * */
        KT_NODISCARD static auto QueueFamilySupportsOperation(VkQueueFamilyProperties queueFamily, VkQueueFlagBits flags) -> bool { return queueFamily.queueFlags & flags; }
        KT_NODISCARD static auto QueueFamilySupportsGraphicsOperations(VkQueueFamilyProperties queueFamily, VkQueueFlagBits flags) -> bool { return queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT; }
        KT_NODISCARD static auto QueueFamilySupportsComputeOperations(VkQueueFamilyProperties queueFamily, VkQueueFlagBits flags) -> bool { return queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT; }
        KT_NODISCARD static auto QueueFamilySupportsTransferOperations(VkQueueFamilyProperties queueFamily, VkQueueFlagBits flags) -> bool { return queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT; }

    private:
        /*************************************************************
        * FOR INTERNAL USAGE
        * ********************************************************+ */
        static auto SetupPhysicalDevicesData() -> void;

    private:
        /*************************************************************
        * GENERAL DEVICE REQUIRED EXTENSIONS
        * ********************************************************+ */
        inline static const std::vector<const char *> s_ValidationLayers{ "VK_LAYER_KHRONOS_validation" };
        inline static const std::vector<const char *> s_DeviceRequiredExtensions{
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            //VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME,
        };

        /*************************************************************
        * STATIC DATA
        * ********************************************************+ */
        inline static ContextData s_ContextData{
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

        inline static std::shared_ptr<VulkanSwapChain> s_SwapChain{};
        inline static VmaAllocator s_DefaultAllocator{};

        // This information is physical device specific
        inline static std::vector<SwapChainSupportDetails> s_SwapChainSupportDetails{};
        inline static std::vector<QueuesData> s_QueueFamiliesData{};
    };
}

#endif //KATE_ENGINE_VULKAN_CONTEXT_HH