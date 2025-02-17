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
#include <Renderer/Core/RenderContext.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>
#include <Renderer/Vulkan/VulkanSwapChain.hh>

#include <Renderer/Vulkan/VulkanDescriptorManager.hh>


namespace Mikoto {
    enum DescriptorSetLayoutType {
        DESCRIPTOR_SET_LAYOUT_BASE_SHADER,
        DESCRIPTOR_SET_LAYOUT_BASE_SHADER_WIREFRAME,
    };

    struct VulkanContextData {
        VkInstance Instance{};
        VkSurfaceKHR Surface{};
        VmaVulkanFunctions VulkanVMAFunctions{};
        VkDebugUtilsMessengerEXT DebugMessenger{};

        Scope_T<VulkanDevice> Device{};

        bool VOLKInitSuccess{};

        const bool EnableValidationLayers{};
        std::vector<const char*> ValidationLayers{};
    };

    class VulkanContext final : public RenderContext, public Singleton<VulkanContext> {
    public:
        explicit VulkanContext() = default;

        explicit VulkanContext(const RenderContextCreateInfo& createInfo)
            :  RenderContext{ createInfo }
        { }

        auto Init() -> bool override;
        auto Shutdown() -> void override;

        auto SubmitFrame() -> void override;
        auto PrepareFrame() -> void override;

        // [Swapchain manipulation]
        auto EnableVSync() -> void override { SwitchSyncMode( true ); }
        auto DisableVSync() -> void { SwitchSyncMode( false ); }
        MKT_NODISCARD auto IsVSyncActive() const -> bool { return m_SwapChain->IsVsyncEnabled(); }
        MKT_NODISCARD auto GetSwapChain() const -> VulkanSwapChain& { return *m_SwapChain; }

        // [General getters]
        MKT_NODISCARD auto GetSurface() const -> const VkSurfaceKHR& { return m_VulkanData.Surface; }
        MKT_NODISCARD auto GetDescriptorAllocator() -> VulkanDescriptorAllocator& { return m_DescriptorAllocator; }
        MKT_NODISCARD auto GetInstance() const -> const VkInstance& { return m_VulkanData.Instance; }
        MKT_NODISCARD auto GetInstance() -> VkInstance& { return m_VulkanData.Instance; }
        MKT_NODISCARD auto GetDevice() -> VulkanDevice& { return *m_VulkanData.Device; }
        MKT_NODISCARD auto GetDevice() const -> const VulkanDevice& { return *m_VulkanData.Device; }
        MKT_NODISCARD auto IsValidationEnabled() const -> bool { return m_VulkanData.EnableValidationLayers; }
        MKT_NODISCARD auto GetValidationLayers() -> std::vector<const char*>& { return m_VulkanData.ValidationLayers; }
        MKT_NODISCARD auto GetVmaFunctions() const -> const VmaVulkanFunctions& { return m_VulkanData.VulkanVMAFunctions; }
        MKT_NODISCARD auto GetCurrentRenderableImageIndex() const -> UInt32_T { return m_CurrentRenderableSwapChainImage; }

        MKT_NODISCARD auto GetDescriptorSetLayouts( const DescriptorSetLayoutType type ) -> const VkDescriptorSetLayout& { return m_DescriptorSetLayouts[type]; }

    private:
        // [Internal usage]
        auto SubmitCommands() const -> void;
        auto PresentToSwapchain() -> void;

        auto InitVolk() -> void;
        auto LoadVmaRequiredFunctions() -> void;
        auto InitDefaultRenderer() -> void;

        auto CreateDefaultDescriptorLayouts() -> void;
        auto InitDescriptorAllocator() -> void;

        auto RecreateSwapChain( bool enableVsync = false ) -> void;

        auto CreateDevice() -> void;
        auto CreateSurface() -> void;
        auto CreateInstance() -> void;
        auto CreateDebugMessenger() -> void;
        auto CreateSynchronizationPrimitives() -> void;
        auto CreateSwapChain( const VulkanSwapChainCreateInfo& createInfo ) -> void;

        auto SwitchSyncMode( bool enable ) -> void;
        MKT_NODISCARD auto CheckValidationLayerSupport() const -> bool;

    private:
        std::unordered_map<DescriptorSetLayoutType, VkDescriptorSetLayout> m_DescriptorSetLayouts{};

        VulkanDescriptorAllocator m_DescriptorAllocator{};

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
        Scope_T<VulkanSwapChain> m_SwapChain{};
        UInt32_T m_CurrentRenderableSwapChainImage{};
        FrameSynchronizationPrimitives m_SwapChainSyncObjects{};

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