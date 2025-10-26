/**
 * VulkanContext.cc
 * Created by kate on 7/3/23.
 * */

// C++ Standard Library
#include <algorithm>
#include <any>
#include <memory>
#include <numeric>
#include <ranges>
#include <set>
#include <stdexcept>
#include <unordered_set>
#include <vector>

// Third-Party Libraries
#include <volk.h>
#include <GLFW/glfw3.h>
#include <fmt/format.h>

// Project Headers
#include <Common/Common.hh>
#include <Library/String/String.hh>
#include <Logging/Assert.hh>
#include <Logging/Logger.hh>
#include <Platform/Window.hh>
#include <Renderer/RenderService.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>

namespace Mikoto {

    static auto GetGlfwRequiredExtensions() -> std::vector<const char*> {
        UInt32 glfwExtensionCount{};
        const char** glfwExtensions{ glfwGetRequiredInstanceExtensions( std::addressof( glfwExtensionCount ) ) };

        if ( glfwExtensions == nullptr ) {
            MKT_THROW_RUNTIME_ERROR( "Vulkan is not available on this platform!" );
        }

        return std::vector<const char*>{ glfwExtensions, glfwExtensions + glfwExtensionCount };
    }

    static auto DisplayGflwRequiredInstanceExtensions() -> void {
        UInt32 extensionCount{};
        vkEnumerateInstanceExtensionProperties( nullptr, std::addressof( extensionCount ), nullptr );

        std::vector<VkExtensionProperties> extensions( extensionCount );
        vkEnumerateInstanceExtensionProperties( nullptr, std::addressof( extensionCount ), extensions.data() );

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

                    switch (messageSeverity) {
                        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
                            MKT_CORE_LOGGER_ERROR( "Validation Error: {}", pCallbackData->pMessage );
                            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
                                MKT_CORE_LOGGER_WARN( "Validation Warn: {}", pCallbackData->pMessage );
                        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
                            MKT_CORE_LOGGER_INFO( "Validation Info: {}", pCallbackData->pMessage );
                        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
                            MKT_CORE_LOGGER_DEBUG( "Validation Debug: {}", pCallbackData->pMessage );
                        default:
                            MKT_CORE_LOGGER_ERROR( "Validation Unhandled Severity: {}", pCallbackData->pMessage );
                    }
            return VK_FALSE;
        };
    }

    static auto CreateDebugUtilsMessengerEXT(
            VkInstance instance,
            const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
            const VkAllocationCallbacks* pAllocator,
            VkDebugUtilsMessengerEXT* pDebugMessenger ) -> VkResult {
        if ( vkCreateDebugUtilsMessengerEXT == nullptr ) {
            MKT_CORE_LOGGER_ERROR( "VulkanContext - Could not create DebugUtilsMessengerEXT because extension function 'vkCreateDebugUtilsMessengerEXT' is not available." );
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        return vkCreateDebugUtilsMessengerEXT( instance, pCreateInfo, pAllocator, pDebugMessenger );
    }

    auto VulkanContext::Init() -> bool {
        MKT_CORE_LOGGER_INFO( "Initializing VulkanContext." );

        InitVolk();
        if ( !m_VulkanData.VOLKInitSuccess ) {
            return false;
        }

        CreateInstance();

        CreateDebugMessenger();

        CreateSurface();

        // Create the swap chain. The manager is an texture/image manager that
        // provides the images we render to and present to the screen.
        // If we pass a window while creating the context, we assume we want to present
        // So we check for swap chain support and create it if possible

        // Init the device when the context is ready
        m_Device = GpuDevice::Create({ .Api = GraphicsAPI::VULKAN_API });
        if (!m_Device) {
            MKT_THROW_RUNTIME_ERROR( "RenderContext::Create - Could not initialize GPU Device." );
        }
        m_Device->Init();

        // The current flow assumes we will present to a surface
        // so we create a swap chain to get ready for that
        CreateSwapchain();

        CreateSynchronizationPrimitives();

        return true;
    }

    auto VulkanContext::Shutdown() -> void {

        m_Swapchain.Disable();

        for (const auto& framePrimitives : m_FrameSyncPrimitives) {
            // Destroy frame sync stuff
            vkDestroySemaphore(  VK_DEVICE(m_Device.get()), framePrimitives.RenderFinishedSemaphore, nullptr );
            vkDestroySemaphore( VK_DEVICE(m_Device.get()), framePrimitives.ImageAvailableSemaphore, nullptr );
            vkDestroyFence(  VK_DEVICE(m_Device.get()), framePrimitives.RenderFence, nullptr );
        }

        if ( m_VulkanData.EnableValidationLayers && vkDestroyDebugUtilsMessengerEXT != nullptr ) {
            vkDestroyDebugUtilsMessengerEXT( GetInstance(), m_VulkanData.DebugMessenger, nullptr );
        }

        // Device needs a valid context
        m_Device->Shutdown();
        m_Device = nullptr;

        vkDestroySurfaceKHR( GetInstance(), GetSurface(), nullptr );

        vkDestroyInstance( GetInstance(), nullptr );
    }

    auto VulkanContext::SubmitFrame() -> void {
        // Prepare the submission to the queue. We want to wait on
        // the present semaphore, which is signaled when the swapchain
        // is ready (there's image available to render to). We will
        // signal the render semaphore to signal that rendering has finished

        VulkanDevice* device{ TO_VK_DEVICE( RenderService::Get()->GetGpuDevice() ) };

        device->FlushPendingCommands( m_FrameSyncPrimitives[m_CurrentFrameIndex] );
        device->PresentToSwapChain( m_FrameSyncPrimitives[m_CurrentFrameIndex], m_Swapchain );

        m_Device->RunGarbageCollection();
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

        // Store the API version for future reference
        m_VulkanData.ApiVersion = appInfo.apiVersion;

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
        createInfo.pApplicationInfo = std::addressof( appInfo );

        createInfo.enabledLayerCount = static_cast<UInt32>( m_VulkanData.ValidationLayers.size() );
        createInfo.ppEnabledLayerNames = m_VulkanData.ValidationLayers.data();

        createInfo.enabledExtensionCount = static_cast<UInt32>( extensions.size() );
        createInfo.ppEnabledExtensionNames = extensions.data();

        if ( vkCreateInstance( std::addressof( createInfo ), nullptr, std::addressof( m_VulkanData.Instance ) ) != VK_SUCCESS ) {
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
        if ( CreateDebugUtilsMessengerEXT( m_VulkanData.Instance, std::addressof( createInfo ), nullptr, std::addressof( m_VulkanData.DebugMessenger ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanContext::CreateDebugMessenger - Failed to set up debug messenger." );
        }
    }

    auto VulkanContext::CreateSurface() -> void {
        if ( m_TargetWindow != nullptr ) {
            const auto window{ std::any_cast<GLFWwindow*>( m_TargetWindow->GetNativeWindow() ) };

            // For now, this is delegated to GLFW as the swap chain images are presented to a window generated by GLFW
            // Otherwise we would need to manually set up ourselves the surfaces according to the windowing system depending on the OS
            if ( glfwCreateWindowSurface( m_VulkanData.Instance, window, nullptr, std::addressof( m_VulkanData.Surface ) ) != VK_SUCCESS ) {
                MKT_THROW_RUNTIME_ERROR( "VulkanContext::CreateSurface - Error failed to create Vulkan Surface." );
            }
        }
    }

    auto VulkanContext::CheckValidationLayerSupport() const -> bool {
        UInt32 layerCount{};
        vkEnumerateInstanceLayerProperties( std::addressof( layerCount ), nullptr );

        std::vector<VkLayerProperties> availableLayers( layerCount );
        vkEnumerateInstanceLayerProperties( std::addressof( layerCount ), availableLayers.data() );

        const std::string validationLayerTarget{ m_VulkanData.ValidationLayers[0] };

        const auto result{ std::ranges::find_if( availableLayers,
                                                 [&]( const VkLayerProperties& layerProperty ) -> bool {
                                                     return validationLayerTarget == layerProperty.layerName;
                                                 } ) };

        return result != availableLayers.end();
    }

    auto VulkanContext::SwitchSyncMode( const bool enable ) -> void {

    }

    auto VulkanContext::PrepareFrame() -> void {

        m_CurrentFrameIndex = m_Swapchain->GetCurrentFrameIndex();
        const auto ret{ m_Swapchain->GetNextRenderableImage( m_CurrentImageIndex,
                                                     m_FrameSyncPrimitives[m_CurrentFrameIndex].RenderFence,
                                                     m_FrameSyncPrimitives[m_CurrentFrameIndex].ImageAvailableSemaphore ) };

        if ( ret == VK_ERROR_OUT_OF_DATE_KHR ) {
            MKT_THROW_RUNTIME_ERROR( "Error getting swap chain image index" );
        }

        if ( ret != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanContext::PrepareFrame - Failed to acquire swap chain Screenshots!" );
        }
    }

    auto VulkanContext::InitVolk() -> void {
        const VkResult ret{ volkInitialize() };
        m_VulkanData.VOLKInitSuccess = ret == VK_SUCCESS;

        MKT_ASSERT( m_VulkanData.VOLKInitSuccess, "VulkanContext::InitContext - Failed to initialize VOLK!" );
        MKT_ASSERT( m_TargetWindow, "VulkanContext::InitContext - Window handle for Vulkan Context initialization is NULL" );
    }

    auto VulkanContext::CreateSwapchain() -> void {
        const VulkanSwapChainCreateInfo createInfo{
            .Extent{
                .width { static_cast<UInt32>( m_TargetWindow->GetWidth() ) },
                .height{ static_cast<UInt32>( m_TargetWindow->GetHeight() ) } },
            .Surface{ std::addressof( m_VulkanData.Surface ) },
            .EnableVsync{ false },
        };

        m_Swapchain = TO_VK_DEVICE( m_Device.get() )->CreateSwapChain(createInfo);
    }

    auto VulkanContext::CreateSynchronizationPrimitives() -> void {

        const Size frameCount{ m_Swapchain->GetImageCount() };
        m_FrameSyncPrimitives.resize( frameCount );

        for (auto& [ImageAvailableSemaphore, RenderFinishedSemaphore, RenderFence] : m_FrameSyncPrimitives) {
            VkFenceCreateInfo fenceInfo{ VulkanHelpers::Initializers::FenceCreateInfo() };

            // We want to create the fence with the Create Signaled flag, so we
            // can wait on it before using it on a GPU command (for the first frame)
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

            if ( vkCreateFence( VK_DEVICE(RenderService::Get()->GetGpuDevice()),
                                std::addressof( fenceInfo ),
                                nullptr,
                                std::addressof( RenderFence ) ) != VK_SUCCESS ) {
                MKT_THROW_RUNTIME_ERROR( fmt::format( "Failed to create frame render fence!" ) );
                                }

            VkSemaphoreCreateInfo semaphoreCreateInfo{ VulkanHelpers::Initializers::SemaphoreCreateInfo() };
            if ( vkCreateSemaphore( VK_DEVICE(RenderService::Get()->GetGpuDevice()),
                                    std::addressof( semaphoreCreateInfo ),
                                    nullptr,
                                    std::addressof( ImageAvailableSemaphore ) ) != VK_SUCCESS ) {
                MKT_THROW_RUNTIME_ERROR( fmt::format( "Failed to create frame present semaphore" ) );
                                    }

            if ( vkCreateSemaphore( VK_DEVICE(RenderService::Get()->GetGpuDevice()),
                                    std::addressof( semaphoreCreateInfo ),
                                    nullptr,
                                    std::addressof( RenderFinishedSemaphore ) ) != VK_SUCCESS ) {
                MKT_THROW_RUNTIME_ERROR( fmt::format( "Failed to create frame render semaphore" ) );
                                    }
        }
    }

}// namespace Mikoto