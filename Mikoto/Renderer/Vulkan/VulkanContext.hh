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
#include <vk_mem_alloc.h>
#include <volk.h>

// Project Headers
#include <Common/Common.hh>
#include <STL/Utility/Types.hh>
#include <Renderer/Vulkan/VulkanUtils.hh>
#include <Platform/Window/XPWindow.hh>
#include <Platform/Window/Window.hh>
#include <Renderer/Vulkan/VulkanCommandPool.hh>
#include <Renderer/Vulkan/VulkanSwapChain.hh>


namespace Mikoto {
    struct ContextData {
        UInt32_T PrimaryLogicalDeviceIndex{};
        UInt32_T PrimaryPhysicalDeviceIndex{};

        std::vector<VkDevice> LogicalDevices{};
        std::vector<VkPhysicalDevice> PhysicalDevices{};

        // This is a direct mapping between physical devices position in PhysicalDevices with
        // the corresponding position in PhysicalDeviceFeatures, PhysicalDeviceProperties,
        // PhysicalDeviceMemoryProperties (i.e physical device at index 0 has device properties at index 0, and so on)
        std::vector<VkPhysicalDeviceFeatures> PhysicalDeviceFeatures{};
        std::vector<VkPhysicalDeviceProperties> PhysicalDeviceProperties{};
        std::vector<VkPhysicalDeviceMemoryProperties> PhysicalDeviceMemoryProperties{};

        VkInstance Instance{};
        VkSurfaceKHR Surface{};
        VkDebugUtilsMessengerEXT DebugMessenger{};

        bool VOLKInitSuccess{};

        // The Vulkan context works with XPWindow's for now
        Ref_T<XPWindow> WindowHandle{};

        const bool EnableValidationLayers{};
    };

    // Used for short-lived commands
    struct ImmediateSubmitContext {
        VkFence UploadFence{};            // Notify the host a task has finished executing
        VkCommandBuffer CommandBuffer{};  // Command buffer to submit work to
        Ref_T<VulkanCommandPool> CommandPool{};// Command pool to allocate command buffer from
    };

    class VulkanContext {
    public:
        static auto Init(const std::shared_ptr<Window>& handle) -> void;
        static auto Shutdown() -> void;

        static auto Present() -> void;
        static auto PrepareFrame() -> void;
        static auto SubmitFrame() -> void;

        static auto EnableVSync() -> void { SwitchVSync_H( true ); }
        static auto DisableVSync() -> void { SwitchVSync_H( false ); }
        MKT_NODISCARD static auto IsVSyncActive() -> bool { return s_SwapChain->GetSwapChainCreateInfo().VSyncEnable; }


        static auto CreatePrimaryLogicalDeviceCommandPools() -> void;
        static auto CreatePrimaryLogicalDeviceCommandBuffers() -> void;
        static auto CreateSynchronizationPrimitives() -> void;

        static auto PushCommandBuffer(VkCommandBuffer cmd) -> void;

        MKT_NODISCARD static auto GetDetailedStatistics() -> const VmaTotalStatistics&;
        MKT_NODISCARD static auto QuerySwapChainSupport(VkPhysicalDevice device) -> SwapChainSupportDetails;

        static auto ImmediateSubmit(const std::function<void(VkCommandBuffer)>& task, VkQueue queue = GetPrimaryLogicalDeviceGraphicsQueue()) -> void;

        // [General getters]
        MKT_NODISCARD static inline auto GetSurface() -> VkSurfaceKHR& { return s_ContextData.Surface; }
        MKT_NODISCARD static inline auto GetInstance() -> VkInstance& { return s_ContextData.Instance; }
        MKT_NODISCARD static inline auto GetDefaultAllocator() -> VmaAllocator& { return s_DefaultAllocator; }
        MKT_NODISCARD static inline auto GetCurrentImageIndex() -> UInt32_T { return s_CurrentImageIndex; }
        MKT_NODISCARD static inline auto GetDrawCommandBuffersHandles() -> std::vector<VkCommandBuffer> & { return s_RenderCommandBufferHandles; }


        // [Physical device data getters]
        MKT_NODISCARD static inline auto GetPrimaryPhysicalDeviceFeatures()         -> VkPhysicalDeviceFeatures         { return s_ContextData.PhysicalDeviceFeatures[s_ContextData.PrimaryPhysicalDeviceIndex]; }
        MKT_NODISCARD static inline auto GetPrimaryPhysicalDeviceProperties()       -> VkPhysicalDeviceProperties       { return s_ContextData.PhysicalDeviceProperties[s_ContextData.PrimaryPhysicalDeviceIndex]; }
        MKT_NODISCARD static inline auto GetPrimaryPhysicalDevice()                 -> VkPhysicalDevice                 { return s_ContextData.PhysicalDevices[s_ContextData.PrimaryPhysicalDeviceIndex]; };
        MKT_NODISCARD static inline auto GetPrimaryPhysicalDeviceMemoryProperties() -> VkPhysicalDeviceMemoryProperties { return s_ContextData.PhysicalDeviceMemoryProperties[s_ContextData.PrimaryPhysicalDeviceIndex]; }


        // [Logical device data getters]
        MKT_NODISCARD static inline auto GetPrimaryLogicalDevice() -> VkDevice { return s_ContextData.LogicalDevices[s_ContextData.PrimaryLogicalDeviceIndex]; }
        MKT_NODISCARD static inline auto GetPrimaryLogicalDeviceFeatures() -> VkPhysicalDeviceFeatures { return s_ContextData.PhysicalDeviceFeatures[s_ContextData.PrimaryLogicalDeviceIndex]; }
        MKT_NODISCARD static inline auto GetPrimaryLogicalDeviceProperties() -> VkPhysicalDeviceProperties { return s_ContextData.PhysicalDeviceProperties[s_ContextData.PrimaryLogicalDeviceIndex]; }
        MKT_NODISCARD static inline auto GetPrimaryLogicalDeviceQueuesData() -> QueuesData { return s_QueueFamiliesData[s_ContextData.PrimaryLogicalDeviceIndex]; }
        MKT_NODISCARD static inline auto GetPrimaryLogicalDevicePresentQueue() -> VkQueue { return s_QueueFamiliesData[s_ContextData.PrimaryLogicalDeviceIndex].PresentQueue; }
        MKT_NODISCARD static inline auto GetPrimaryLogicalDeviceGraphicsQueue() -> VkQueue { return s_QueueFamiliesData[s_ContextData.PrimaryLogicalDeviceIndex].GraphicsQueue; }
        MKT_NODISCARD static inline auto GetPrimaryLogicalDeviceMemoryProperties() -> VkPhysicalDeviceMemoryProperties { return s_ContextData.PhysicalDeviceMemoryProperties[s_ContextData.PrimaryLogicalDeviceIndex]; }

        static auto RecreateSwapChain(VulkanSwapChainCreateInfo &&info) -> void;
        MKT_NODISCARD static inline auto GetSwapChain() -> std::shared_ptr<VulkanSwapChain> { return s_SwapChain; }

        MKT_NODISCARD static auto FindMemoryType(UInt32_T typeFilter, VkMemoryPropertyFlags properties, VkPhysicalDevice device) -> UInt32_T;
        MKT_NODISCARD static auto FindSupportedFormat(VkPhysicalDevice device, const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features) -> VkFormat;

    private:
        // [Internal usage]
        static auto InitSwapChain() -> void;
        static auto InitContext( const std::shared_ptr<Window>& ptr ) -> void;
        static auto SubmitToQueue(const QueueSubmitInfo &submitInfo) -> void;

        MKT_NODISCARD static auto IsDeviceSuitable(VkPhysicalDevice device) -> bool;
        MKT_NODISCARD static auto IsExtensionAvailable(std::string_view targetExtensionName, VkPhysicalDevice device) -> bool;
        MKT_NODISCARD static auto DeviceSupportsRequiredExtensions(VkPhysicalDevice device) -> bool;

        // Debug
        static auto OutputDebugInfo() -> void;

    private:
        static auto SwitchVSync_H(bool value) -> void;

    private:
        static auto CreateSurface() -> void;
        static auto CreateInstance() -> void;
        static auto CreateDebugMessenger() -> void;
        static auto InitMemoryAllocator() -> void;
        static auto FetchPhysicalDeviceSpec() -> void;
        static auto PickPrimaryPhysicalDevice() -> void;
        static auto CreatePrimaryLogicalDevice() -> void;
        static auto CheckValidationLayerSupport() -> bool;

    private:
        // Context main data, such as device, device properties, etc
        inline static ContextData s_ContextData {
            .PrimaryLogicalDeviceIndex{},
            .PrimaryPhysicalDeviceIndex{},

            .LogicalDevices{},
            .PhysicalDevices{},

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

        // Index to a valid swap chain image we can render to
        inline static UInt32_T s_CurrentImageIndex{};

        // Main command pool
        inline static Ref_T<VulkanCommandPool> s_MainCommandPool{};

        // Single swap chain
        inline static Ref_T<VulkanSwapChain> s_SwapChain{};

        // Synchronization primitives to synchronize rendering and presentation operations
        inline static FrameSynchronizationPrimitives s_SwapChainSyncObjects{ };

        // Contains the actual VkCommandBuffer handles, easier to pass to Vulkan structures such as VkSubmitInfo
        inline static std::vector<VkCommandBuffer> s_RenderCommandBufferHandles{};

        inline static ImmediateSubmitContext s_ImmediateSubmitContext{  };

        // Target validation layers
        inline static const std::vector<const char *> s_ValidationLayers{ "VK_LAYER_KHRONOS_validation" };

        // Required application extensions
        inline static const std::vector<const char *> s_DeviceRequiredExtensions{
                VK_KHR_SWAPCHAIN_EXTENSION_NAME,

                // Passing your vertex data just like in OpenGL, using the same state (as the pipeline setup)
                // and Shaders as in OpenGL, your scene will likely not display as you’d expect.
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