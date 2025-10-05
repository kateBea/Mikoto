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
#include <vk_mem_alloc.h>

// Project Headers
#include <Common/Common.hh>
#include <Library/String/String.hh>
#include <Logging/Assert.hh>
#include <Logging/Logger.hh>
#include <Platform/MainWindow.hh>
#include <Platform/Window.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
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

            MKT_CORE_LOGGER_ERROR( "Validation Error: {}", pCallbackData->pMessage );
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

        LoadVmaRequiredFunctions();

        return true;
    }

    auto VulkanContext::Shutdown() -> void {

        if ( m_VulkanData.EnableValidationLayers && vkDestroyDebugUtilsMessengerEXT != nullptr ) {
            vkDestroyDebugUtilsMessengerEXT( GetInstance(), m_VulkanData.DebugMessenger, nullptr );
        }

        vkDestroySurfaceKHR( GetInstance(), GetSurface(), nullptr );

        vkDestroyInstance( GetInstance(), nullptr );
    }

    auto VulkanContext::SubmitFrame() -> void {
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
        if ( m_ContextData.TargetWindow != nullptr ) {
            const auto window{ std::any_cast<GLFWwindow*>( m_ContextData.TargetWindow->GetNativeWindow() ) };

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

    auto VulkanContext::PrepareFrame() -> void {
    }

    auto VulkanContext::InitVolk() -> void {
        const VkResult ret{ volkInitialize() };
        m_VulkanData.VOLKInitSuccess = ret == VK_SUCCESS;

        MKT_ASSERT( m_VulkanData.VOLKInitSuccess, "VulkanContext::InitContext - Failed to initialize VOLK!" );
        MKT_ASSERT( m_ContextData.TargetWindow, "VulkanContext::InitContext - Window handle for Vulkan Context initialization is NULL" );
    }
}// namespace Mikoto