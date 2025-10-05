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
#include <Library/Utility/Types.hh>
#include <Renderer/RenderContext.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>
#include <Renderer/Vulkan/VulkanTexture.hh>

#include <Renderer/Vulkan/VulkanDescriptorManager.hh>


namespace Mikoto {

    struct VulkanContextData {
        VkInstance Instance{};
        VkSurfaceKHR Surface{};
        VmaVulkanFunctions VulkanVMAFunctions{};
        VkDebugUtilsMessengerEXT DebugMessenger{};

        bool VOLKInitSuccess{};

        const bool EnableValidationLayers{};
        std::vector<const char*> ValidationLayers{};

        auto SetInstance(VkInstance instance) -> VulkanContextData&;
        auto SetSurface(VkSurfaceKHR surface) -> VulkanContextData&;
        auto SetDebugMessenger(VkDebugUtilsMessengerEXT debugUtil) -> VulkanContextData&;
        auto SetIsVolkReady(bool value) -> VulkanContextData&;
        auto SetValidationLayers(CStr* layers, UInt32 count) -> VulkanContextData&;
    };

    class VulkanContext final : public RenderContext {
    public:
        explicit VulkanContext(const RenderContextCreateInfo& createInfo)
            :  RenderContext{ createInfo }
        { }

        auto Init() -> bool override;
        auto Shutdown() -> void override;

        auto SubmitFrame() -> void override;
        auto PrepareFrame() -> void override;

        auto EnableVSync() -> void override { SwitchSyncMode( true ); }
        auto DisableVSync() -> void override { SwitchSyncMode( false ); }

        MKT_NODISCARD auto IsVSyncActive() const -> bool { return m_SwapChain->IsVsyncEnabled(); }
        MKT_NODISCARD auto GetSwapChain() const -> VulkanSwapChain& { return *m_SwapChain; }

        // [General getters]
        MKT_NODISCARD auto GetSurface() const -> const VkSurfaceKHR& { return m_VulkanData.Surface; }

        MKT_NODISCARD auto GetInstance() const -> const VkInstance& { return m_VulkanData.Instance; }
        MKT_NODISCARD auto GetInstance() -> VkInstance& { return m_VulkanData.Instance; }

        MKT_NODISCARD auto GetDevice() -> VulkanDevice* { return dynamic_cast<VulkanDevice*>(m_GraphicsDevice.get()); }
        MKT_NODISCARD auto GetDevice() const -> const VulkanDevice* { return dynamic_cast<VulkanDevice*>(m_GraphicsDevice.get());  }

        MKT_NODISCARD auto GetValidationLayers() -> std::vector<const char*>& { return m_VulkanData.ValidationLayers; }

        MKT_NODISCARD auto GetVmaFunctions() const -> const VmaVulkanFunctions& { return m_VulkanData.VulkanVMAFunctions; }

        MKT_NODISCARD auto GetCurrentRenderableImageIndex() const -> UInt32 { return m_CurrentRenderableSwapChainImage; }

    private:
        // [Internal usage]
        auto SubmitCommands() -> void;
        auto PresentToSwapchain() -> void;

        auto PrepareImmediateSubmit() -> void;

        auto InitVolk() -> void;
        auto LoadVmaRequiredFunctions() -> void;

        auto CreateDefaultDescriptorLayouts() -> void;
        auto InitDescriptorAllocator() -> void;

        auto RecreateSwapChain( bool enableVsync = false ) -> void;

        auto CreateDevice() -> void;
        auto CreateSurface() -> void;
        auto CreateInstance() -> void;
        auto CreateDebugMessenger() -> void;
        auto CreateSynchronizationPrimitives() -> void;
        auto CreateSwapChain( const VulkanSwapChainCreateInfo& createInfo ) -> void;

        auto FlushImmediateSubmitTasks() -> void;

        auto SwitchSyncMode( bool enable ) -> void;
        MKT_NODISCARD auto CheckValidationLayerSupport() const -> bool;

    private:

        VulkanContextData m_VulkanData{
            .Instance{},
            .Surface{},
            .DebugMessenger{},
            .VOLKInitSuccess{},
#if defined( NDEBUG )
            // Disable validation layers for non-debug builds
            .EnableValidationLayers{ false },
#else
            .EnableValidationLayers{ true },
#endif
            .ValidationLayers{ "VK_LAYER_KHRONOS_validation" }
        };

        // Swapchain manipulation data
        Unique<VulkanSwapChain> m_SwapChain{};
        UInt32 m_CurrentRenderableSwapChainImage{};
        GraphicsQueueSyncPrimitives m_SwapChainSyncObjects{};

        // Required application extensions
        std::vector<const char *> m_DeviceRequestedExtensions{
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