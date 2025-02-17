/**
 * VulkanContext.cc
 * Created by kate on 7/3/23.
 * */

// C++ Standard Library
#include <any>
#include <memory>
#include <numeric>
#include <set>
#include <stdexcept>
#include <unordered_set>
#include <algorithm>
#include <vector>

// Third-Party Libraries
#include <fmt/format.h>
#include <vk_mem_alloc.h>
#include <volk.h>
#include <GLFW/glfw3.h>

// Project Headers
#include <Common/Common.hh>
#include <Core/Logging/Assert.hh>
#include <Core/Logging/Logger.hh>
#include <Library/String/String.hh>
#include <Platform/Window/MainWindow.hh>
#include <Platform/Window/Window.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanDeletionQueue.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>
#include <Renderer/Vulkan/VulkanRenderer.hh>
#include <ranges>

namespace Mikoto {

    static auto GetGlfwRequiredExtensions() -> std::vector<const char*> {
        UInt32_T glfwExtensionCount{};
        const char** glfwExtensions{ glfwGetRequiredInstanceExtensions( std::addressof(glfwExtensionCount) ) };

        if ( glfwExtensions == nullptr ) {
            MKT_THROW_RUNTIME_ERROR( "Vulkan is not available on this platform!" );
        }

        return std::vector<const char*>{ glfwExtensions, glfwExtensions + glfwExtensionCount };
    }

    static auto DisplayGflwRequiredInstanceExtensions() -> void {
        UInt32_T extensionCount{};
        vkEnumerateInstanceExtensionProperties( nullptr, std::addressof(extensionCount), nullptr );

        std::vector<VkExtensionProperties> extensions( extensionCount );
        vkEnumerateInstanceExtensionProperties( nullptr, std::addressof(extensionCount), extensions.data() );

        // Log System available instance extension
        MKT_COLOR_PRINT_FORMATTED( MKT_FMT_COLOR_AQUA, "Instance Available extensions:\n" );
        for ( const auto& [extensionName, specVersion]: extensions ) {
            MKT_COLOR_PRINT_FORMATTED( MKT_FMT_COLOR_GREEN_YELLOW, "\t{}\n", extensionName );
        }

        // Log GLFW required extensions
        MKT_COLOR_PRINT_FORMATTED( MKT_FMT_COLOR_AQUA, "GLFW required extensions:\n" );
        const auto requiredExtensions{ GetGlfwRequiredExtensions() };
        for ( const auto& required: requiredExtensions ) {
            MKT_COLOR_PRINT_FORMATTED( MKT_FMT_COLOR_ORANGE_RED, "\t{}\n", required );
        }
    }

    static auto SetupDebugMessengerCreateInfo( VkDebugUtilsMessengerCreateInfoEXT& createInfo ) -> void {
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

        createInfo.pfnUserCallback =
                []( VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                    VkDebugUtilsMessageTypeFlagsEXT messageType,
                    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                    void* pUserData ) -> VKAPI_ATTR VkBool32 {
            // Unused
            ( void )messageSeverity;
            ( void )messageType;
            ( void )pUserData;

            MKT_CORE_LOGGER_ERROR( "Validation Error: {}", pCallbackData->pMessage );
            return VK_FALSE;
        };
    }

    auto VulkanContext::Init() -> bool {
        bool success{ true };

        try {
            InitVolk();

            CreateInstance();
            CreateDebugMessenger();
            CreateSurface();

            LoadVmaRequiredFunctions();

            CreateDevice();
            RecreateSwapChain();
            CreateSynchronizationPrimitives();

            CreateDefaultDescriptorLayouts();

            InitDefaultRenderer();

            InitDescriptorAllocator();

        } catch (MKT_UNUSED_VAR const std::exception& exception) {
            MKT_THROW_RUNTIME_ERROR( fmt::format( "VulkanContext::Init - Error initializing Vulkan Context. {}", exception.what() ) );
            success = false;
        }

        return success;
    }

    auto VulkanContext::CreateDevice() -> void {
        VulkanDeviceCreateInfo createInfo{
            .Instance{ std::addressof( m_VulkanData.Instance ) },
            .Surface{ std::addressof( m_VulkanData.Surface ) },
            .RequiredExtensionsCount{ m_DeviceRequestedExtensions.size() },
            .DeviceRequestedExtensions{ m_DeviceRequestedExtensions.data()},
            .ValidationsLayersCount{ GetValidationLayers().size() },
            .ValidationsLayers{ GetValidationLayers().data() },
            .VmaCallbacks{ std::addressof( GetVmaFunctions() ) }
        };

        m_VulkanData.Device = CreateScope<VulkanDevice>(createInfo);
        m_VulkanData.Device->Init();
    }

    auto VulkanContext::Shutdown() -> void {
        // Wait for remaining operations to finish
        m_VulkanData.Device->WaitIdle();

        // Clean remaining objects
        VulkanDeletionQueue::Flush();

        if ( m_VulkanData.EnableValidationLayers && vkDestroyDebugUtilsMessengerEXT != nullptr ) {
            vkDestroyDebugUtilsMessengerEXT( GetInstance(), m_VulkanData.DebugMessenger, nullptr );
        }

        for ( const auto& descLayout: m_DescriptorSetLayouts | std::views::values ) {
            vkDestroyDescriptorSetLayout( m_VulkanData.Device->GetLogicalDevice(), descLayout, nullptr );
        }

        m_SwapChain->Release();
        vkDestroySurfaceKHR( GetInstance(), GetSurface(), nullptr );

        m_VulkanData.Device = nullptr;

        vkDestroyInstance( GetInstance(), nullptr );
    }

    auto VulkanContext::SubmitFrame() -> void {
        SubmitCommands();

        PresentToSwapchain();
    }

    static auto CreateDebugUtilsMessengerEXT(
            VkInstance instance,
            const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
            const VkAllocationCallbacks* pAllocator,
            VkDebugUtilsMessengerEXT* pDebugMessenger ) -> VkResult
    {
        if ( vkCreateDebugUtilsMessengerEXT == nullptr ) {
            MKT_CORE_LOGGER_ERROR( "VulkanContext - Could not create DebugUtilsMessengerEXT because extension function 'vkCreateDebugUtilsMessengerEXT' is not available." );
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        return vkCreateDebugUtilsMessengerEXT( instance, pCreateInfo, pAllocator, pDebugMessenger );
    }

    auto VulkanContext::CreateInstance() -> void {
        if ( m_VulkanData.EnableValidationLayers && !CheckValidationLayerSupport() ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanContext::CreateInstance - Validation layers requested, but not available." );
        }

        VkApplicationInfo appInfo{ VulkanHelpers::Initializers::ApplicationInfo() };
        appInfo.pApplicationName = "Mikoto Application";
        appInfo.pEngineName = "Mikoto";
        appInfo.applicationVersion = VK_MAKE_API_VERSION( 0, MKT_ENGINE_VERSION_MAJOR, MKT_ENGINE_VERSION_MINOR, MKT_ENGINE_VERSION_PATCH );
        appInfo.engineVersion = VK_MAKE_API_VERSION( 0, MKT_ENGINE_VERSION_MAJOR, MKT_ENGINE_VERSION_MINOR, MKT_ENGINE_VERSION_PATCH );
        appInfo.apiVersion = VK_MAKE_API_VERSION( MKT_VULKAN_VERSION_VARIANT, MKT_VULKAN_VERSION_MAJOR, MKT_VULKAN_VERSION_MINOR, MKT_VULKAN_VERSION_PATCH );

        // Setup required extensions
        auto extensions{ GetGlfwRequiredExtensions() };
        if ( m_VulkanData.EnableValidationLayers ) {
            extensions.push_back( VK_EXT_DEBUG_UTILS_EXTENSION_NAME );
        }

#ifndef NDEBUG
        DisplayGflwRequiredInstanceExtensions();
#endif

        // Setup debug messenger utility for instance errors
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{ VulkanHelpers::Initializers::DebugUtilsMessengerCreateInfoEXT() };
        if ( m_VulkanData.EnableValidationLayers ) {
            SetupDebugMessengerCreateInfo( debugCreateInfo );
        }

        VkInstanceCreateInfo createInfo{ VulkanHelpers::Initializers::InstanceCreateInfo() };
        createInfo.pNext = std::addressof( debugCreateInfo );
        createInfo.pApplicationInfo = std::addressof(appInfo);

        createInfo.enabledLayerCount = static_cast<UInt32_T>( m_VulkanData.ValidationLayers.size() );
        createInfo.ppEnabledLayerNames = m_VulkanData.ValidationLayers.data();

        createInfo.enabledExtensionCount = static_cast<UInt32_T>( extensions.size() );
        createInfo.ppEnabledExtensionNames = extensions.data();

        if ( vkCreateInstance( std::addressof(createInfo), nullptr, std::addressof(m_VulkanData.Instance) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanContext::CreateInstance - Failed to create Vulkan instance." );
        }

        // load all required Vulkan entry-points, including all extensions
        volkLoadInstance( m_VulkanData.Instance );
    }

    auto VulkanContext::CreateDebugMessenger() -> void {
        if ( !m_VulkanData.EnableValidationLayers ) {
            return;
        }

        VkDebugUtilsMessengerCreateInfoEXT createInfo{ VulkanHelpers::Initializers::DebugUtilsMessengerCreateInfoEXT() };
        SetupDebugMessengerCreateInfo( createInfo );
        if ( CreateDebugUtilsMessengerEXT( m_VulkanData.Instance, std::addressof(createInfo), nullptr, std::addressof(m_VulkanData.DebugMessenger) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanContext::CreateDebugMessenger - Failed to set up debug messenger." );
        }
    }

    auto VulkanContext::CreateSurface() -> void {
        if (m_ContextData.TargetWindow != nullptr) {
            // Caller controls exception
            const auto window{ std::any_cast<GLFWwindow*>(m_ContextData.TargetWindow->GetNativeWindow()) };

            // For now, this is delegated to GLFW as the swap chain images are presented to a window generated by GLFW
            // Otherwise we would need to manually set up ourselves the surfaces according to the windowing system depending on the OS
            if (glfwCreateWindowSurface(m_VulkanData.Instance, window, nullptr, std::addressof(m_VulkanData.Surface)) != VK_SUCCESS) {
                MKT_THROW_RUNTIME_ERROR("VulkanContext::CreateSurface - Error failed to create Vulkan Surface.");
            }
        }
    }

    auto VulkanContext::CheckValidationLayerSupport() const -> bool {
        UInt32_T layerCount{};
        vkEnumerateInstanceLayerProperties( std::addressof( layerCount ), nullptr );

        std::vector<VkLayerProperties> availableLayers( layerCount );
        vkEnumerateInstanceLayerProperties( std::addressof( layerCount ), availableLayers.data() );

        const std::string validationLayerTarget{ m_VulkanData.ValidationLayers[0] };

        const auto result{ std::ranges::find_if(availableLayers,
            [&](const VkLayerProperties& layerProperty) -> bool {
                return validationLayerTarget == layerProperty.layerName;
            }) };

        return result != availableLayers.end();
    }

    auto VulkanContext::SwitchSyncMode( const bool enable ) -> void {
        // Return if we want to enable and VSync is already enabled
        if (enable && m_SwapChain->IsVsyncEnabled()) {
            return;
        }

        RecreateSwapChain( enable );
    }

    auto VulkanContext::CreateSwapChain( const VulkanSwapChainCreateInfo& createInfo ) -> void {
        m_SwapChain = CreateScope<VulkanSwapChain>( createInfo );

        if (m_SwapChain != nullptr) {
            m_SwapChain->Init();
        }
    }

    auto VulkanContext::LoadVmaRequiredFunctions() -> void {
        // Setup Vulkan Functions
        m_VulkanData.VulkanVMAFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;// Required when using VMA_DYNAMIC_VULKAN_FUNCTIONS.
        m_VulkanData.VulkanVMAFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;    // Required when using VMA_DYNAMIC_VULKAN_FUNCTIONS.
        m_VulkanData.VulkanVMAFunctions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
        m_VulkanData.VulkanVMAFunctions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
        m_VulkanData.VulkanVMAFunctions.vkAllocateMemory = vkAllocateMemory;
        m_VulkanData.VulkanVMAFunctions.vkFreeMemory = vkFreeMemory;
        m_VulkanData.VulkanVMAFunctions.vkMapMemory = vkMapMemory;
        m_VulkanData.VulkanVMAFunctions.vkUnmapMemory = vkUnmapMemory;
        m_VulkanData.VulkanVMAFunctions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
        m_VulkanData.VulkanVMAFunctions.vkInvalidateMappedMemoryRanges = vkFlushMappedMemoryRanges;
        m_VulkanData.VulkanVMAFunctions.vkBindBufferMemory = vkBindBufferMemory;
        m_VulkanData.VulkanVMAFunctions.vkBindImageMemory = vkBindImageMemory;
        m_VulkanData.VulkanVMAFunctions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
        m_VulkanData.VulkanVMAFunctions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
        m_VulkanData.VulkanVMAFunctions.vkCreateBuffer = vkCreateBuffer;
        m_VulkanData.VulkanVMAFunctions.vkDestroyBuffer = vkDestroyBuffer;
        m_VulkanData.VulkanVMAFunctions.vkCreateImage = vkCreateImage;
        m_VulkanData.VulkanVMAFunctions.vkDestroyImage = vkDestroyImage;
        m_VulkanData.VulkanVMAFunctions.vkCmdCopyBuffer = vkCmdCopyBuffer;
        m_VulkanData.VulkanVMAFunctions.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2;// Fetch "vkGetBufferMemoryRequirements2" on Vulkan >= 1.1, fetch "vkGetBufferMemoryRequirements2KHR" when using VK_KHR_dedicated_allocation extension.
        m_VulkanData.VulkanVMAFunctions.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2;  // Fetch "vkGetImageMemoryRequirements2" on Vulkan >= 1.1, fetch "vkGetImageMemoryRequirements2KHR" when using VK_KHR_dedicated_allocation extension.
        m_VulkanData.VulkanVMAFunctions.vkBindBufferMemory2KHR = vkBindBufferMemory2;                      // Fetch "vkBindBufferMemory2" on Vulkan >= 1.1, fetch "vkBindBufferMemory2KHR" when using VK_KHR_bind_memory2 extension.
        m_VulkanData.VulkanVMAFunctions.vkBindImageMemory2KHR = vkBindImageMemory2;                        // Fetch "vkBindImageMemory2" on Vulkan >= 1.1, fetch "vkBindImageMemory2KHR" when using VK_KHR_bind_memory2 extension.
        m_VulkanData.VulkanVMAFunctions.vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2;
        m_VulkanData.VulkanVMAFunctions.vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements;// Fetch from "vkGetDeviceBufferMemoryRequirements" on Vulkan >= 1.3, but you can also fetch it from "vkGetDeviceBufferMemoryRequirementsKHR" if you enabled extension VK_KHR_maintenance4.
        m_VulkanData.VulkanVMAFunctions.vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements;  // Fetch from "vkGetDeviceImageMemoryRequirements" on Vulkan >= 1.3, but you can also fetch it from "vkGetDeviceImageMemoryRequirementsKHR" if you enabled extension VK_KHR_maintenance4.
    }

    auto VulkanContext::InitDefaultRenderer() -> void {
        const RendererCreateInfo createInfo{
            .ViewportWidth{ static_cast<UInt32_T>( m_ContextData.TargetWindow->GetWidth() ) },
            .ViewportHeight{ static_cast<UInt32_T>( m_ContextData.TargetWindow->GetHeight() ) },
            .Api{ m_ContextData.GraphicsAPI }
        };

        m_ContextData.DefaultRenderer = RendererBackend::Create( createInfo );

        MKT_ASSERT( m_ContextData.DefaultRenderer, "VulkanContext::InitDefaultRenderer - Failed to create the default renderer." );

        if ( !m_ContextData.DefaultRenderer->Init() ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanContext::InitDefaultRenderer - Failed to initialize the default renderer." );
        }
    }

    auto VulkanContext::CreateDefaultDescriptorLayouts() -> void {

        DescriptorLayoutBuilder baseShaderDescriptorLayoutBuilder{};
        VkDescriptorSetLayout descLayout{ baseShaderDescriptorLayoutBuilder
                                       .WithBinding( 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER )
                                       .WithBinding( 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT )
                                       .WithBinding( 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT )
                                       .WithBinding( 3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT )
                                       .Build( m_VulkanData.Device->GetLogicalDevice() ) };

        DescriptorLayoutBuilder baseShaderWireframeDescriptorLayoutBuilder{};
        m_DescriptorSetLayouts.try_emplace( DESCRIPTOR_SET_LAYOUT_BASE_SHADER, descLayout );

        VkDescriptorSetLayout descLayoutWireframe{ baseShaderWireframeDescriptorLayoutBuilder
                                               .WithBinding( 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER )
                                               .Build( m_VulkanData.Device->GetLogicalDevice() ) };
        m_DescriptorSetLayouts.try_emplace( DESCRIPTOR_SET_LAYOUT_BASE_SHADER_WIREFRAME, descLayoutWireframe );

    }

    auto VulkanContext::RecreateSwapChain( const bool enableVsync ) -> void {
        const auto [Width, Height]{
            std::make_pair( m_ContextData.TargetWindow->GetWidth(), m_ContextData.TargetWindow->GetHeight() )
        };

        const VulkanSwapChainCreateInfo createInfo{
            .Extent{ static_cast<UInt32_T>( Width ), static_cast<UInt32_T>( Height ) },
            .Surface{ std::addressof( m_VulkanData.Surface ) },
            .EnableVsync{ enableVsync },
            .OldSwapChain{ m_SwapChain != nullptr ? m_SwapChain->GetSwapChainKHR() : VK_NULL_HANDLE },
        };

        // Wait before destroying the swapchain and creating a new one
        m_VulkanData.Device->WaitIdle();

        CreateSwapChain(createInfo);
    }

    auto VulkanContext::PrepareFrame() -> void {
        const auto ret{ GetSwapChain().GetNextRenderableImage( m_CurrentRenderableSwapChainImage,
                                                     m_SwapChainSyncObjects.RenderFence,
                                                     m_SwapChainSyncObjects.PresentSemaphore ) };

        if ( ret == VK_ERROR_OUT_OF_DATE_KHR ) {
            RecreateSwapChain( GetSwapChain().IsVsyncEnabled() );
        }

        if ( ret != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanContext::PrepareFrame - Failed to acquire swap chain image!" );
        }
    }

    auto VulkanContext::SubmitCommands() const -> void {
        FrameSynchronizationPrimitives submitInfo{};

        // Specify present and render semaphores
        submitInfo.RenderFence = m_SwapChainSyncObjects.RenderFence;
        submitInfo.RenderSemaphore = m_SwapChainSyncObjects.RenderSemaphore;
        submitInfo.PresentSemaphore = m_SwapChainSyncObjects.PresentSemaphore;

        m_VulkanData.Device->SubmitCommands( submitInfo );
    }

    auto VulkanContext::PresentToSwapchain() -> void {
        const auto result{ m_SwapChain->Present( m_CurrentRenderableSwapChainImage, m_SwapChainSyncObjects.RenderSemaphore ) };

        if ( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ) {
            RecreateSwapChain( GetSwapChain().IsVsyncEnabled() );
            return;
        }

        if ( result != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanContext::Present - Error failed present images to swapchain." );
        }
    }

    auto VulkanContext::InitDescriptorAllocator() -> void {
        std::vector<VulkanDescriptorAllocator::PoolSizeRatio> sizes{
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
        };


        m_DescriptorAllocator.Init( m_VulkanData.Device->GetLogicalDevice(), 1000, sizes );
    }

    auto VulkanContext::CreateSynchronizationPrimitives() -> void {
        VkFenceCreateInfo fenceInfo{ VulkanHelpers::Initializers::FenceCreateInfo() };

        // We want to create the fence with the Create Signaled flag, so we
        // can wait on it before using it on a GPU command (for the first frame)
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        if ( vkCreateFence( m_VulkanData.Device->GetLogicalDevice(),
                            std::addressof( fenceInfo ),
                            nullptr,
                            std::addressof( m_SwapChainSyncObjects.RenderFence ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( fmt::format( "Failed to create swap chain render fence!" ) );
        }

        VulkanDeletionQueue::Push( [this]() -> void {
            vkDestroyFence( m_VulkanData.Device->GetLogicalDevice(), m_SwapChainSyncObjects.RenderFence, nullptr );
        } );

        VkSemaphoreCreateInfo semaphoreCreateInfo{ VulkanHelpers::Initializers::SemaphoreCreateInfo() };
        if ( vkCreateSemaphore( m_VulkanData.Device->GetLogicalDevice(),
                                std::addressof( semaphoreCreateInfo ),
                                nullptr,
                                std::addressof( m_SwapChainSyncObjects.PresentSemaphore ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( fmt::format( "Failed to create swap chain present semaphore" ) );
        }

        VulkanDeletionQueue::Push( [this]() -> void {
            vkDestroySemaphore( m_VulkanData.Device->GetLogicalDevice(), m_SwapChainSyncObjects.PresentSemaphore, nullptr );
        } );

        if ( vkCreateSemaphore( m_VulkanData.Device->GetLogicalDevice(),
                                std::addressof( semaphoreCreateInfo ),
                                nullptr,
                                std::addressof( m_SwapChainSyncObjects.RenderSemaphore ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( fmt::format( "Failed to create swap chain render semaphore" ) );
        }

        VulkanDeletionQueue::Push( [this]() -> void {
            vkDestroySemaphore( m_VulkanData.Device->GetLogicalDevice(), m_SwapChainSyncObjects.RenderSemaphore, nullptr );
        } );
    }

    auto VulkanContext::InitVolk() -> void {
        const VkResult ret{ volkInitialize() };
        m_VulkanData.VOLKInitSuccess = ret == VK_SUCCESS;

        MKT_ASSERT( m_VulkanData.VOLKInitSuccess, "VulkanContext::InitContext - Failed to initialize VOLK!" );
        MKT_ASSERT( m_ContextData.TargetWindow, "VulkanContext::InitContext - Window handle for Vulkan Context initialization is NULL" );

        // Assert only works in debug builds
        if (ret != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR( "VulkanContext::InitContext - No valid Vulkan Loader found in the system." );
        }
    }
}