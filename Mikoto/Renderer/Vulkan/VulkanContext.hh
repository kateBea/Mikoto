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
#include <Common/Common.hh>
#include <Library/Utility/Types.hh>
#include <Renderer/RenderService.hh>
#include "VulkanHelpers.hh"
#include "VulkanTexture.hh"

namespace Mikoto {

    struct VulkanContextData {
        VkInstance Instance{};
        VkSurfaceKHR Surface{};
        VmaVulkanFunctions VulkanVMAFunctions{};
        VkDebugUtilsMessengerEXT DebugMessenger{};

        UInt32 ApiVersion{};

        bool VOLKInitSuccess{};

        const bool EnableValidationLayers{};
        std::vector<const char*> ValidationLayers{};
        std::vector<const char*> InstanceExtensions{};
    };

    class VulkanContext final : public RenderContext, public Singleton<VulkanContext> {
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

        MKT_NODISCARD auto GetCurrentImageIndex() const -> UInt32 { return m_CurrentImageIndex; }
        MKT_NODISCARD auto GetCurrentFrameIndex() const -> UInt32 { return m_CurrentFrameIndex; }

        // [General getters]
        MKT_NODISCARD auto GetSurface() const -> const VkSurfaceKHR& { return m_VulkanData.Surface; }

        MKT_NODISCARD auto GetInstance() const -> const VkInstance& { return m_VulkanData.Instance; }
        MKT_NODISCARD auto GetInstance() -> VkInstance& { return m_VulkanData.Instance; }

        MKT_NODISCARD auto GetSwapchain() -> SwapChainHandle { return m_Swapchain; }

        MKT_NODISCARD auto GetApiVersion() const -> UInt32 { return m_VulkanData.ApiVersion; }

        MKT_NODISCARD auto GetValidationLayers() const -> const std::vector<const char*>& { return m_VulkanData.ValidationLayers; }
        MKT_NODISCARD auto GetVMAFunctions() const -> const VmaVulkanFunctions& { return m_VulkanData.VulkanVMAFunctions; }

    private:
        auto InitVolk() -> void;

        auto CreateSwapchain() -> void;

        auto CreateSurface() -> void;
        auto CreateInstance() -> void;
        auto CreateDebugMessenger() -> void;
        auto CreateSynchronizationPrimitives() -> void;

        auto RecreateSwapchain( bool enableVsync = false) -> void;

        auto SwitchSyncMode( bool enable ) -> void;
        MKT_NODISCARD auto CheckValidationLayerSupport() const -> bool;

    private:

        SwapChainHandle m_Swapchain{};

        // Current frame
        UInt32 m_CurrentFrameIndex{};

        // Current swapchain image we can render to
        // which is set at the start of every frame via PrepareFrame
        UInt32 m_CurrentImageIndex{};
        std::vector<FrameSynchronizationPrimitives> m_FrameSyncPrimitives{};

        UInt32 m_MaxFramesInFlight{};

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
            .ValidationLayers{ "VK_LAYER_KHRONOS_validation" },
            .InstanceExtensions{ VK_EXT_DEBUG_UTILS_EXTENSION_NAME }
        };
    };
}

#endif // MIKOTO_VULKAN_CONTEXT_HH