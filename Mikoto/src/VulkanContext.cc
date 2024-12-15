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
#include <Common/StringUtils.hh>
#include <Common/VulkanUtils.hh>
#include <Core/Assert.hh>
#include <Core/Logger.hh>
#include <Platform/Window.hh>
#include <Platform/XPWindow.hh>
#include <Renderer/Renderer.hh>
#include <Renderer/Vulkan/DeletionQueue.hh>
#include <Renderer/Vulkan/VulkanContext.hh>

namespace Mikoto {

    static auto GetGlfwRequiredExtensions() -> std::vector<const char*> {
        UInt32_T glfwExtensionCount{};
        const char** glfwExtensions{ glfwGetRequiredInstanceExtensions( std::addressof(glfwExtensionCount) ) };

        if ( glfwExtensions == nullptr ) {
            MKT_THROW_RUNTIME_ERROR( "Vulkan is not available on this platform!" );
        }

        return std::vector<const char*>{ glfwExtensions, glfwExtensions + glfwExtensionCount };
    }


    static auto PopulateDebugMessengerCreateInfo( VkDebugUtilsMessengerCreateInfoEXT& createInfo ) -> void {
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

        createInfo.pfnUserCallback =
                []( VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                    VkDebugUtilsMessageTypeFlagsEXT messageType,
                    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData ) -> VKAPI_ATTR VkBool32 {
            // Unused
            ( void )messageSeverity;
            ( void )messageType;
            ( void )pUserData;

            MKT_CORE_LOGGER_ERROR( "{}", pCallbackData->pMessage );
            return VK_FALSE;
        };
    }


    static auto DisplayGflwRequiredInstanceExtensions() -> void {
        UInt32_T extensionCount{};
        vkEnumerateInstanceExtensionProperties( nullptr, std::addressof(extensionCount), nullptr );
        std::vector<VkExtensionProperties> extensions( extensionCount );
        vkEnumerateInstanceExtensionProperties( nullptr, std::addressof(extensionCount), extensions.data() );

        MKT_COLOR_PRINT_FORMATTED( MKT_FMT_COLOR_AQUA, "Available extensions: \n" );
        std::unordered_set<std::string> available{};
        for ( const auto& extension: extensions ) {
            MKT_COLOR_PRINT_FORMATTED( MKT_FMT_COLOR_GREEN_YELLOW, "\t{}\n", extension.extensionName );
            available.insert( extension.extensionName );
        }

        MKT_COLOR_PRINT_FORMATTED( MKT_FMT_COLOR_AQUA, "Required extensions: \n" );
        auto requiredExtensions{ GetGlfwRequiredExtensions() };
        for ( const auto& required: requiredExtensions ) {
            MKT_COLOR_PRINT_FORMATTED( MKT_FMT_COLOR_ORANGE_RED, "\t{}\n", required );
            if ( available.find( required ) == available.end() ) {
                throw std::runtime_error( "Missing required GLFW extensions" );
            }
        }
    }


    auto VulkanContext::Init( const std::shared_ptr<Window>& handle ) -> void {
        InitContext( handle );

        CreateInstance();
        CreateDebugMessenger();
        CreateSurface();
        PickPrimaryPhysicalDevice();
        CreatePrimaryLogicalDevice();

        InitSwapChain();
        InitMemoryAllocator();
        InitRenderer();

        CreatePrimaryLogicalDeviceCommandPools();
        CreatePrimaryLogicalDeviceCommandBuffers();
        CreateSynchronizationPrimitives();

#if !defined( NDEBUG )
        OutputDebugInfo();
#endif
    }


    auto VulkanContext::Shutdown() -> void {
        if ( s_ContextData.EnableValidationLayers && vkDestroyDebugUtilsMessengerEXT != nullptr ) {
            vkDestroyDebugUtilsMessengerEXT( GetInstance(), s_ContextData.DebugMessenger, nullptr );
        }

        s_SwapChain->OnRelease();
        vkDestroySurfaceKHR( GetInstance(), GetSurface(), nullptr );

        vmaDestroyAllocator( s_DefaultAllocator );

        vkDestroyDevice( GetPrimaryLogicalDevice(), nullptr );
        vkDestroyInstance( GetInstance(), nullptr );
    }

    static auto CreateDebugUtilsMessengerEXT(
            VkInstance instance,
            const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
            const VkAllocationCallbacks* pAllocator,
            VkDebugUtilsMessengerEXT* pDebugMessenger ) -> VkResult
    {
        if ( vkCreateDebugUtilsMessengerEXT == nullptr )
            return VK_ERROR_EXTENSION_NOT_PRESENT;

        return vkCreateDebugUtilsMessengerEXT( instance, pCreateInfo, pAllocator, pDebugMessenger );
    }

    auto VulkanContext::CreateInstance() -> void {
        if ( s_ContextData.EnableValidationLayers && !CheckValidationLayerSupport() ) {
            MKT_THROW_RUNTIME_ERROR( "Validation layers requested, but not available!" );
        }

        // Setup application data
        VkApplicationInfo appInfo{ VulkanUtils::Initializers::ApplicationInfo() };

        appInfo.pApplicationName = "Mikoto Application";
        appInfo.pEngineName = "Mikoto";
        appInfo.applicationVersion = VK_MAKE_API_VERSION( 0, MKT_ENGINE_VERSION_MAJOR, MKT_ENGINE_VERSION_MINOR, MKT_ENGINE_VERSION_PATCH );
        appInfo.engineVersion = VK_MAKE_API_VERSION( 0, MKT_ENGINE_VERSION_MAJOR, MKT_ENGINE_VERSION_MINOR, MKT_ENGINE_VERSION_PATCH );
        // Patch version should always be set to zero, see Vulkan Spec
        appInfo.apiVersion = VK_MAKE_API_VERSION( MKT_VULKAN_VERSION_VARIANT, MKT_VULKAN_VERSION_MAJOR, MKT_VULKAN_VERSION_MINOR, MKT_VULKAN_VERSION_PATCH );

        VkInstanceCreateInfo createInfo{ VulkanUtils::Initializers::InstanceCreateInfo() };
        createInfo.pApplicationInfo = std::addressof(appInfo);

        // Setup required extensions
        auto extensions{ GetGlfwRequiredExtensions() };

        if ( s_ContextData.EnableValidationLayers ) {
            extensions.push_back( VK_EXT_DEBUG_UTILS_EXTENSION_NAME );
        }

        createInfo.enabledExtensionCount = static_cast<UInt32_T>( extensions.size() );
        createInfo.ppEnabledExtensionNames = extensions.data();

        // Setup debug messenger utility
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{ VulkanUtils::Initializers::DebugUtilsMessengerCreateInfoEXT() };

        if ( s_ContextData.EnableValidationLayers ) {
            createInfo.enabledLayerCount = static_cast<UInt32_T>( s_ValidationLayers.size() );
            createInfo.ppEnabledLayerNames = s_ValidationLayers.data();

            PopulateDebugMessengerCreateInfo( debugCreateInfo );
            createInfo.pNext = reinterpret_cast<VkDebugUtilsMessengerCreateInfoEXT*>( std::addressof(debugCreateInfo) );
        }

        if ( vkCreateInstance( std::addressof(createInfo), nullptr, std::addressof(s_ContextData.Instance) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "Failed to create instance!" );
        }

        // load all required Vulkan entry-points, including all extensions
        volkLoadInstance( s_ContextData.Instance );
    }


    auto VulkanContext::CreateDebugMessenger() -> void {
        if ( !s_ContextData.EnableValidationLayers ) {
            return;
        }

        VkDebugUtilsMessengerCreateInfoEXT createInfo{ VulkanUtils::Initializers::DebugUtilsMessengerCreateInfoEXT() };
        PopulateDebugMessengerCreateInfo( createInfo );
        if ( CreateDebugUtilsMessengerEXT( s_ContextData.Instance, std::addressof(createInfo), nullptr, std::addressof(s_ContextData.DebugMessenger) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "Failed to set up debug messenger!" );
        }
    }


    auto VulkanContext::CreateSurface() -> void {
        try {
            GLFWwindow* window{ std::any_cast<GLFWwindow*>(s_ContextData.WindowHandle->GetNativeWindow()) };

            if (glfwCreateWindowSurface(s_ContextData.Instance, window, nullptr, std::addressof(s_ContextData.Surface)) != VK_SUCCESS) {
                MKT_THROW_RUNTIME_ERROR("Failed to create Vulkan Surface");
            }

        } catch (const std::exception& exc) {
            MKT_THROW_RUNTIME_ERROR(exc.what());
        }
    }


    auto VulkanContext::CheckValidationLayerSupport() -> bool {
        UInt32_T layerCount{};
        vkEnumerateInstanceLayerProperties( &layerCount, nullptr );

        std::vector<VkLayerProperties> availableLayers( layerCount );
        vkEnumerateInstanceLayerProperties( &layerCount, availableLayers.data() );

        bool validationLayerFound{ false };
        std::vector<VkLayerProperties>::size_type index{};
        const std::string validationLayerTarget{ s_ValidationLayers[0] };

        while ( index < availableLayers.size() && !validationLayerFound ) {
            validationLayerFound = validationLayerTarget == availableLayers[index].layerName;
            ++index;
        }

        return validationLayerFound;
    }

    auto VulkanContext::FetchPhysicalDeviceSpec() -> void {
        UInt32_T deviceCount{};
        vkEnumeratePhysicalDevices( s_ContextData.Instance, std::addressof(deviceCount), nullptr );

        if ( deviceCount == 0 ) {
            MKT_THROW_RUNTIME_ERROR( "Failed to find GPUs with Vulkan support!" );
        }

        s_ContextData.PhysicalDevices = std::vector<VkPhysicalDevice>( deviceCount, VK_NULL_HANDLE );
        s_ContextData.PhysicalDeviceFeatures = std::vector<VkPhysicalDeviceFeatures>( deviceCount );
        s_ContextData.PhysicalDeviceProperties = std::vector<VkPhysicalDeviceProperties>( deviceCount );
        s_ContextData.PhysicalDeviceMemoryProperties = std::vector<VkPhysicalDeviceMemoryProperties>( deviceCount );

        s_QueueFamiliesData = std::vector<QueuesData>( deviceCount );

        vkEnumeratePhysicalDevices( s_ContextData.Instance, std::addressof(deviceCount), s_ContextData.PhysicalDevices.data() );

        UInt32_T deviceIndex{};
        for ( auto& physicalDevice: s_ContextData.PhysicalDevices ) {
            vkGetPhysicalDeviceFeatures( physicalDevice, std::addressof( s_ContextData.PhysicalDeviceFeatures[deviceIndex] ) );
            vkGetPhysicalDeviceProperties( physicalDevice, std::addressof( s_ContextData.PhysicalDeviceProperties[deviceIndex] ) );
            vkGetPhysicalDeviceMemoryProperties( physicalDevice, std::addressof( s_ContextData.PhysicalDeviceMemoryProperties[deviceIndex] ) );

            ++deviceIndex;
        }
    }


    auto VulkanContext::PickPrimaryPhysicalDevice() -> void {
        FetchPhysicalDeviceSpec();

        Size_T index{};
        bool foundSuitablePrimaryGPU{ false };

#if false // NEEDS TO BE TESTED
        auto it{
            std::find_if(s_ContextData.PhysicalDevices.begin(),
                          s_ContextData.PhysicalDevices.end(),
                          [&index](const auto& device) -> bool {
                              if ( IsDeviceSuitable( s_ContextData.PhysicalDevices[index] ) ) {
                                  s_ContextData.PrimaryPhysicalDeviceIndex = index;
                                  return true;
                              }

                              return false;
                          })
        };

        if (it == s_ContextData.PhysicalDevices.end()) {
            MKT_THROW_RUNTIME_ERROR( "Could not find a suitable GPU to use as primary physical device!" );
        }
#endif

        while ( index < s_ContextData.PhysicalDevices.size() && !foundSuitablePrimaryGPU ) {
            if ( IsDeviceSuitable( s_ContextData.PhysicalDevices[index] ) ) {
                s_ContextData.PrimaryPhysicalDeviceIndex = index;
                foundSuitablePrimaryGPU = true;
            }

            ++index;
        }

        if ( s_ContextData.PhysicalDevices[s_ContextData.PrimaryPhysicalDeviceIndex] == VK_NULL_HANDLE ) {
            MKT_THROW_RUNTIME_ERROR( "Could not find a suitable GPU to use as primary physical device!" );
        }
    }


    auto VulkanContext::DeviceSupportsRequiredExtensions( VkPhysicalDevice device ) -> bool {
        UInt32_T extensionCount{};
        vkEnumerateDeviceExtensionProperties( device, nullptr, std::addressof(extensionCount), nullptr );

        std::vector<VkExtensionProperties> availableExtensions( extensionCount );
        vkEnumerateDeviceExtensionProperties( device, nullptr, std::addressof(extensionCount), availableExtensions.data() );

        UInt32_T matchedExtensions{};

        std::string requiredExtension{};
        std::string supportedExtension{};

        for ( const auto& reqExtension: s_DeviceRequiredExtensions ) {
            requiredExtension = reqExtension;
            for ( const auto& extension: availableExtensions ) {
                matchedExtensions += StringUtils::Equal( requiredExtension, extension.extensionName ) ? 1 : 0;
            }
        }

        return matchedExtensions == s_DeviceRequiredExtensions.size();
    }

    auto VulkanContext::QuerySwapChainSupport( VkPhysicalDevice device ) -> SwapChainSupportDetails {
        SwapChainSupportDetails details{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR( device, s_ContextData.Surface, std::addressof(details.Capabilities) );

        UInt32_T formatCount{};
        vkGetPhysicalDeviceSurfaceFormatsKHR( device, s_ContextData.Surface, std::addressof(formatCount), nullptr );
        if ( formatCount != 0 ) {
            details.Formats.resize( formatCount );
            vkGetPhysicalDeviceSurfaceFormatsKHR( device, s_ContextData.Surface, std::addressof(formatCount), details.Formats.data() );
        }

        UInt32_T presentModeCount{};
        vkGetPhysicalDeviceSurfacePresentModesKHR( device, s_ContextData.Surface, std::addressof(presentModeCount), nullptr );
        if ( presentModeCount != 0 ) {
            details.PresentModes.resize( presentModeCount );
            vkGetPhysicalDeviceSurfacePresentModesKHR( device, s_ContextData.Surface, std::addressof(presentModeCount), details.PresentModes.data() );
        }

        return details;
    }


    auto VulkanContext::IsDeviceSuitable( VkPhysicalDevice device ) -> bool {
        const QueuesData indices{ VulkanUtils::FindQueueFamilies( device, s_ContextData.Surface ) };
        const bool extensionsSupported{ DeviceSupportsRequiredExtensions( device ) };
        bool swapChainAdequate{};
        ( void )swapChainAdequate;

        if ( extensionsSupported ) {
            SwapChainSupportDetails swapChainSupport{ QuerySwapChainSupport( device ) };
            swapChainAdequate = !swapChainSupport.Formats.empty() && !swapChainSupport.PresentModes.empty();
        }

        VkPhysicalDeviceFeatures supportedFeatures{};
        vkGetPhysicalDeviceFeatures( device, std::addressof(supportedFeatures) );

        return indices.IsComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
    }


    static auto SetupDeviceQueueCreateInfo(const std::set<UInt32_T>& uniqueQueueFamilies) -> std::vector<VkDeviceQueueCreateInfo> {
        std::vector<VkDeviceQueueCreateInfo> result{};

        constexpr float queuePriority{ 1.0f };
        for ( UInt32_T queueFamily : uniqueQueueFamilies ) {
            VkDeviceQueueCreateInfo queueCreateInfo{ VulkanUtils::Initializers::DeviceQueueCreateInfo() };

            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = std::addressof(queuePriority);

            result.push_back( queueCreateInfo );
        }

        return result;
    }

    auto VulkanContext::CreatePrimaryLogicalDevice() -> void {
        auto& primaryPhysicalDevice{ s_ContextData.PhysicalDevices[s_ContextData.PrimaryPhysicalDeviceIndex] };
        s_QueueFamiliesData[s_ContextData.PrimaryPhysicalDeviceIndex] = VulkanUtils::FindQueueFamilies( primaryPhysicalDevice, s_ContextData.Surface );

        const auto graphicsQueueFamilyIndex{ s_QueueFamiliesData[s_ContextData.PrimaryPhysicalDeviceIndex].GraphicsFamilyIndex };
        const auto presentQueueFamilyIndex{ s_QueueFamiliesData[s_ContextData.PrimaryPhysicalDeviceIndex].PresentFamilyIndex };

        const auto queueCreateInfos{ SetupDeviceQueueCreateInfo({ graphicsQueueFamilyIndex, presentQueueFamilyIndex}) };

        // Requested device features
        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.samplerAnisotropy = VK_TRUE;
        deviceFeatures.fillModeNonSolid = VK_TRUE;// required for wireframe mode

        // TODO: Required Vulkan 1.3 features
        VkPhysicalDeviceVulkan13Features vulkan13Features{ VulkanUtils::Initializers::PhysicalDeviceVulkan13Features() };
        vulkan13Features.synchronization2 = VK_TRUE;

        VkPhysicalDeviceFeatures2 physicalDeviceFeatures2{ VulkanUtils::Initializers::PhysicalDeviceFeatures2() };
        physicalDeviceFeatures2.features = deviceFeatures;
        physicalDeviceFeatures2.pNext = std::addressof( vulkan13Features );

        VkDeviceCreateInfo createInfo{ VulkanUtils::Initializers::DeviceCreateInfo() };
        createInfo.queueCreateInfoCount = static_cast<UInt32_T>( queueCreateInfos.size() );
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pEnabledFeatures = nullptr;
        createInfo.enabledExtensionCount = static_cast<UInt32_T>( s_DeviceRequiredExtensions.size() );
        createInfo.ppEnabledExtensionNames = s_DeviceRequiredExtensions.data();
        createInfo.pNext = std::addressof( physicalDeviceFeatures2 );

        // might not be necessary anymore because device-specific validation layers have been deprecated
        // even tho recommended for some backwards compatibility as they are required for some Vulkan implementations
        if ( s_ContextData.EnableValidationLayers ) {
            // These two fields are ignored by up-to-date Vulkan implementations
            createInfo.enabledLayerCount = static_cast<UInt32_T>( s_ValidationLayers.size() );
            createInfo.ppEnabledLayerNames = s_ValidationLayers.data();
        } else {
            createInfo.enabledLayerCount = 0;
        }

        // Save position of primary logical device
        s_ContextData.PrimaryLogicalDeviceIndex = 0;
        s_ContextData.LogicalDevices = std::vector<VkDevice>( 1 );

        auto& data{ s_ContextData };

        auto result { std::addressof( s_ContextData.LogicalDevices[s_ContextData.PrimaryLogicalDeviceIndex] ) };

        if ( vkCreateDevice( GetPrimaryPhysicalDevice(),
                             std::addressof( createInfo ), nullptr,
                             std::addressof( s_ContextData.LogicalDevices[s_ContextData.PrimaryLogicalDeviceIndex] ) ) != VK_SUCCESS )
        {
            MKT_THROW_RUNTIME_ERROR( "Failed to create primary logical device!" );
        }

        /**
         * [...] all device-related function calls, such as vkCmdDraw, will go through Vulkan loader dispatch code.
         * This allows you to transparently support multiple VkDevice objects in the same application, but comes at
         * a price of dispatch overhead which can be as high as 7% depending on the driver and application.
         *
         * To avoid this, For applications that use just one VkDevice object, load device-related
         * Vulkan entry-points directly from the driver with void volkLoadDevice(VkDevice device);
         * See: https://github.com/zeux/volk
         * */
        volkLoadDevice( GetPrimaryLogicalDevice() );

        vkGetDeviceQueue( s_ContextData.LogicalDevices[s_ContextData.PrimaryLogicalDeviceIndex],
                          s_QueueFamiliesData[s_ContextData.PrimaryPhysicalDeviceIndex].GraphicsFamilyIndex, 0,
                          std::addressof(s_QueueFamiliesData[s_ContextData.PrimaryPhysicalDeviceIndex].GraphicsQueue) );

        vkGetDeviceQueue( s_ContextData.LogicalDevices[s_ContextData.PrimaryLogicalDeviceIndex],
                          s_QueueFamiliesData[s_ContextData.PrimaryPhysicalDeviceIndex].PresentFamilyIndex, 0,
                          std::addressof(s_QueueFamiliesData[s_ContextData.PrimaryPhysicalDeviceIndex].PresentQueue) );
    }


    auto VulkanContext::FindMemoryType( UInt32_T typeFilter, VkMemoryPropertyFlags properties, VkPhysicalDevice device ) -> UInt32_T {
        VkPhysicalDeviceMemoryProperties memProperties{};
        vkGetPhysicalDeviceMemoryProperties( device, std::addressof( memProperties ) );

        for ( UInt32_T index{}; index < memProperties.memoryTypeCount; index++ ) {
            if ( ( typeFilter & ( 1 << index ) ) && ( memProperties.memoryTypes[index].propertyFlags & properties ) == properties ) {
                return index;
            }
        }

        MKT_THROW_RUNTIME_ERROR( "failed to find suitable memory type!" );
    }


    MKT_UNUSED_FUNC auto VulkanContext::IsExtensionAvailable( std::string_view targetExtensionName, VkPhysicalDevice device ) -> bool {
        UInt32_T extensionCount{};
        vkEnumerateDeviceExtensionProperties( device, nullptr, &extensionCount, nullptr );
        std::vector<VkExtensionProperties> availableExtensions( extensionCount );
        vkEnumerateDeviceExtensionProperties( device, nullptr, &extensionCount, availableExtensions.data() );

        for ( const auto& extensionProperties: availableExtensions ) {
            if ( StringUtils::Equal( extensionProperties.extensionName, targetExtensionName ) ) {
                return true;
            }
        }

        return false;
    }

    auto VulkanContext::FindSupportedFormat( VkPhysicalDevice device, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features ) -> VkFormat {
        for ( VkFormat format: candidates ) {
            VkFormatProperties props{};
            vkGetPhysicalDeviceFormatProperties( device, format, &props );

            if ( ( tiling == VK_IMAGE_TILING_LINEAR && ( props.linearTilingFeatures & features ) == features ) ||
                 ( tiling == VK_IMAGE_TILING_OPTIMAL && ( props.optimalTilingFeatures & features ) == features ) ) {
                return format;
            }
        }

        MKT_THROW_RUNTIME_ERROR( "Failed to find supported format!" );
    }

    auto VulkanContext::RecreateSwapChain( VulkanSwapChainCreateInfo&& info ) -> void {
        if ( s_SwapChain )
            s_SwapChain->OnRelease();

        s_SwapChain->OnCreate( std::move( info ) );
    }


    auto VulkanContext::SwitchVSync_H( bool value ) -> void {
        const auto windowExtent{ std::make_pair( 1920, 1080 ) };

        VulkanSwapChainCreateInfo createInfo{
            .OldSwapChain = s_SwapChain->GetSwapChainKHR(),
            .Extent = { ( UInt32_T )windowExtent.first, ( UInt32_T )windowExtent.second },
            .VSyncEnable = value,
        };

        RecreateSwapChain( std::move( createInfo ) );
    }


    auto VulkanContext::InitSwapChain() -> void {
        s_SwapChain = std::make_shared<VulkanSwapChain>();
        s_SwapChain->OnCreate( VulkanSwapChain::GetDefaultCreateInfo() );
    }


    auto VulkanContext::InitMemoryAllocator() -> void {
        // Setup Vulkan Functions
        VmaVulkanFunctions vulkanFunctions{};
        vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;// Required when using VMA_DYNAMIC_VULKAN_FUNCTIONS.
        vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;    // Required when using VMA_DYNAMIC_VULKAN_FUNCTIONS.
        vulkanFunctions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
        vulkanFunctions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
        vulkanFunctions.vkAllocateMemory = vkAllocateMemory;
        vulkanFunctions.vkFreeMemory = vkFreeMemory;
        vulkanFunctions.vkMapMemory = vkMapMemory;
        vulkanFunctions.vkUnmapMemory = vkUnmapMemory;
        vulkanFunctions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
        vulkanFunctions.vkInvalidateMappedMemoryRanges = vkFlushMappedMemoryRanges;
        vulkanFunctions.vkBindBufferMemory = vkBindBufferMemory;
        vulkanFunctions.vkBindImageMemory = vkBindImageMemory;
        vulkanFunctions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
        vulkanFunctions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
        vulkanFunctions.vkCreateBuffer = vkCreateBuffer;
        vulkanFunctions.vkDestroyBuffer = vkDestroyBuffer;
        vulkanFunctions.vkCreateImage = vkCreateImage;
        vulkanFunctions.vkDestroyImage = vkDestroyImage;
        vulkanFunctions.vkCmdCopyBuffer = vkCmdCopyBuffer;
        vulkanFunctions.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2;// Fetch "vkGetBufferMemoryRequirements2" on Vulkan >= 1.1, fetch "vkGetBufferMemoryRequirements2KHR" when using VK_KHR_dedicated_allocation extension.
        vulkanFunctions.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2;  // Fetch "vkGetImageMemoryRequirements2" on Vulkan >= 1.1, fetch "vkGetImageMemoryRequirements2KHR" when using VK_KHR_dedicated_allocation extension.
        vulkanFunctions.vkBindBufferMemory2KHR = vkBindBufferMemory2;                      // Fetch "vkBindBufferMemory2" on Vulkan >= 1.1, fetch "vkBindBufferMemory2KHR" when using VK_KHR_bind_memory2 extension.
        vulkanFunctions.vkBindImageMemory2KHR = vkBindImageMemory2;                        // Fetch "vkBindImageMemory2" on Vulkan >= 1.1, fetch "vkBindImageMemory2KHR" when using VK_KHR_bind_memory2 extension.
        vulkanFunctions.vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2;
        vulkanFunctions.vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements;// Fetch from "vkGetDeviceBufferMemoryRequirements" on Vulkan >= 1.3, but you can also fetch it from "vkGetDeviceBufferMemoryRequirementsKHR" if you enabled extension VK_KHR_maintenance4.
        vulkanFunctions.vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements;  // Fetch from "vkGetDeviceImageMemoryRequirements" on Vulkan >= 1.3, but you can also fetch it from "vkGetDeviceImageMemoryRequirementsKHR" if you enabled extension VK_KHR_maintenance4.

        // Setup VmaAllocator
        VmaAllocatorCreateInfo allocatorInfo{};
        allocatorInfo.vulkanApiVersion = VK_MAKE_API_VERSION( MKT_VULKAN_VERSION_VARIANT, MKT_VULKAN_VERSION_MAJOR, MKT_VULKAN_VERSION_MINOR, MKT_VULKAN_VERSION_PATCH );
        allocatorInfo.physicalDevice = GetPrimaryPhysicalDevice();
        allocatorInfo.device = GetPrimaryLogicalDevice();
        allocatorInfo.instance = GetInstance();
        allocatorInfo.pVulkanFunctions = &vulkanFunctions;

        vmaCreateAllocator( std::addressof( allocatorInfo ), std::addressof( s_DefaultAllocator ) );
    }

    auto VulkanContext::GetDetailedStatistics() -> const VmaTotalStatistics& {
        vmaCalculateStatistics( s_DefaultAllocator, std::addressof( s_TotalStatistics ) );

        return s_TotalStatistics;
    }

    auto VulkanContext::CreatePrimaryLogicalDeviceCommandPools() -> void {
        VkCommandPoolCreateInfo createInfo{ VulkanUtils::Initializers::CommandPoolCreateInfo() };
        auto queueFamilyData{ VulkanUtils::FindQueueFamilies( GetPrimaryPhysicalDevice(), s_ContextData.Surface ) };

        createInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        createInfo.queueFamilyIndex = queueFamilyData.GraphicsFamilyIndex;

        s_MainCommandPool.Create( createInfo );

        // Immediate submit command pull
        // default command pool we will use for immediate submission
        VkCommandPoolCreateInfo immediateSubmitCreateInfo{ VulkanUtils::Initializers::CommandPoolCreateInfo() };
        immediateSubmitCreateInfo.queueFamilyIndex = queueFamilyData.GraphicsFamilyIndex;
        immediateSubmitCreateInfo.flags = 0;

        s_ImmediateSubmitContext.CommandPool.Create( immediateSubmitCreateInfo );
    }

    auto VulkanContext::CreatePrimaryLogicalDeviceCommandBuffers() -> void {
        // create one command buffer for each swapchain image and reuse during rendering
        const UInt32_T commandBuffersCount{ static_cast<UInt32_T>( GetSwapChain()->GetImageCount() ) };

        VkCommandBufferAllocateInfo allocInfo{ VulkanUtils::Initializers::CommandBufferAllocateInfo() };
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = s_MainCommandPool.Get();
        allocInfo.commandBufferCount = 1;

        // Allocation ----------------------

        for ( Size_T count{}; count < commandBuffersCount; ++count ) {
            VkCommandBuffer cmd{};

            if ( vkAllocateCommandBuffers( VulkanContext::GetPrimaryLogicalDevice(), &allocInfo, std::addressof( cmd ) ) != VK_SUCCESS ) {
                MKT_THROW_RUNTIME_ERROR( "Failed to allocate command buffer" );
            }

            s_RenderCommandBufferHandles.emplace_back( cmd );
        }

        // Immediate submit command buffers
        VkCommandBufferAllocateInfo immediateSubmitAllocInfo{ VulkanUtils::Initializers::CommandBufferAllocateInfo() };
        immediateSubmitAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        immediateSubmitAllocInfo.commandPool = s_ImmediateSubmitContext.CommandPool.Get();
        immediateSubmitAllocInfo.commandBufferCount = 1;

        // default command buffer we will use for immediate submission
        vkAllocateCommandBuffers( VulkanContext::GetPrimaryLogicalDevice(),
                                  std::addressof( immediateSubmitAllocInfo ),
                                  std::addressof( s_ImmediateSubmitContext.CommandBuffer ) );


        // Cleanup via delete queue ----------------------

        DeletionQueue::Push( [=]() -> void {
            // Clear main draw command buffers
            vkFreeCommandBuffers( GetPrimaryLogicalDevice(),
                                  s_MainCommandPool.Get(),
                                  static_cast<UInt32_T>( s_RenderCommandBufferHandles.size() ),
                                  s_RenderCommandBufferHandles.data() );

            // Immediate submit context objects release
            vkFreeCommandBuffers( GetPrimaryLogicalDevice(),
                                  s_ImmediateSubmitContext.CommandPool.Get(),
                                  1,
                                  std::addressof( s_ImmediateSubmitContext.CommandBuffer ) );
        } );
    }

    auto VulkanContext::PrepareFrame() -> void {
        auto ret{ GetSwapChain()->GetNextImageIndex( s_CurrentImageIndex,
                                                     s_SwapChainSyncObjects.RenderFence,
                                                     s_SwapChainSyncObjects.PresentSemaphore ) };

        if ( ret == VK_ERROR_OUT_OF_DATE_KHR ) {
            VulkanSwapChainCreateInfo createInfo{ VulkanContext::GetSwapChain()->GetSwapChainCreateInfo() };
            VulkanContext::RecreateSwapChain( std::move( createInfo ) );
        }

        if ( ret != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "Failed to acquire swap chain image!" );
        }
    }

    auto VulkanContext::SubmitFrame() -> void {
        QueueSubmitInfo submitInfo{};

        // Specify present and render semaphores
        submitInfo.SyncObjects.RenderFence = s_SwapChainSyncObjects.RenderFence;
        submitInfo.SyncObjects.RenderSemaphore = s_SwapChainSyncObjects.RenderSemaphore;
        submitInfo.SyncObjects.PresentSemaphore = s_SwapChainSyncObjects.PresentSemaphore;

        // Setup commands and specify queue
        submitInfo.CommandsCount = static_cast<UInt32_T>( s_BatchedGraphicQueueCommands.size() );
        submitInfo.Commands = s_BatchedGraphicQueueCommands.data();
        submitInfo.Queue = GetPrimaryLogicalDeviceGraphicsQueue();

        SubmitToQueue( submitInfo );

        // We are not responsible for freeing the command buffers held by this array
        s_BatchedGraphicQueueCommands.clear();
    }

    // TODO: delete if not needed
    auto VulkanContext::SubmitCommands( const VkCommandBuffer* commands, const UInt32_T count ) -> void {
        QueueSubmitInfo submitInfo{};

        // Specify present and render semaphores
        submitInfo.SyncObjects.RenderFence = s_SwapChainSyncObjects.RenderFence;
        submitInfo.SyncObjects.RenderSemaphore = s_SwapChainSyncObjects.RenderSemaphore;
        submitInfo.SyncObjects.PresentSemaphore = s_SwapChainSyncObjects.PresentSemaphore;

        // Setup commands and specify queue
        submitInfo.CommandsCount = count;
        submitInfo.Commands = commands;
        submitInfo.Queue = GetPrimaryLogicalDeviceGraphicsQueue();

        SubmitToQueue( submitInfo );
    }

    auto VulkanContext::Present() -> void {
        auto& swapChain{ *GetSwapChain() };
        auto& syncObjects{ VulkanContext::s_SwapChainSyncObjects };

        auto result{ swapChain.Present( s_CurrentImageIndex, syncObjects.RenderSemaphore ) };

        if ( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ) {
            VulkanSwapChainCreateInfo createInfo{ swapChain.GetSwapChainCreateInfo() };
            VulkanContext::RecreateSwapChain( std::move( createInfo ) );
            return;
        }

        if ( result != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "Failed to submit imgui command buffers!" );
        }
    }

    auto VulkanContext::CreateSynchronizationPrimitives() -> void {
        // Allocation fence ----------------------

        VkFenceCreateInfo fenceInfo{ VulkanUtils::Initializers::FenceCreateInfo() };

        // We want to create the fence with the Create Signaled flag, so we
        // can wait on it before using it on a GPU command (for the first frame)
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        if ( vkCreateFence( VulkanContext::GetPrimaryLogicalDevice(),
                            std::addressof( fenceInfo ),
                            nullptr,
                            std::addressof( s_SwapChainSyncObjects.RenderFence ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( fmt::format( "Failed to create swap chain render fence!" ) );
        }

        // Deletion fence via delete queue ----------------------

        DeletionQueue::Push( []() -> void {
            vkDestroyFence( GetPrimaryLogicalDevice(), s_SwapChainSyncObjects.RenderFence, nullptr );
        } );


        // Allocation semaphore ----------------------

        VkSemaphoreCreateInfo semaphoreCreateInfo{ VulkanUtils::Initializers::SemaphoreCreateInfo() };
        if ( vkCreateSemaphore( VulkanContext::GetPrimaryLogicalDevice(),
                                std::addressof( semaphoreCreateInfo ),
                                nullptr,
                                std::addressof( s_SwapChainSyncObjects.PresentSemaphore ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( fmt::format( "Failed to create swap chain present semaphore" ) );
        }

        // Deletion via delete queue ----------------------

        DeletionQueue::Push( []() -> void {
            vkDestroySemaphore( GetPrimaryLogicalDevice(), s_SwapChainSyncObjects.PresentSemaphore, nullptr );
        } );


        // Allocation semaphore ----------------------

        if ( vkCreateSemaphore( VulkanContext::GetPrimaryLogicalDevice(),
                                std::addressof( semaphoreCreateInfo ),
                                nullptr,
                                std::addressof( s_SwapChainSyncObjects.RenderSemaphore ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( fmt::format( "Failed to create swap chain render semaphore" ) );
        }

        // Deletion via delete queue ----------------------

        DeletionQueue::Push( []() -> void {
            vkDestroySemaphore( GetPrimaryLogicalDevice(), s_SwapChainSyncObjects.RenderSemaphore, nullptr );
        } );


        // Immediate submit synchronization primitives

        // Allocation ----------------------

        VkFenceCreateInfo immediateSubmitFenceInfo{ VulkanUtils::Initializers::FenceCreateInfo() };

        if ( vkCreateFence( VulkanContext::GetPrimaryLogicalDevice(),
                            std::addressof( immediateSubmitFenceInfo ),
                            nullptr,
                            std::addressof( s_ImmediateSubmitContext.UploadFence ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( fmt::format( "Failed to create Vulkan immediate submit fence!" ) );
        }

        // Deletion via delete queue ----------------------

        DeletionQueue::Push( []() -> void {
            vkDestroyFence( GetPrimaryLogicalDevice(), s_ImmediateSubmitContext.UploadFence, nullptr );
        } );
    }

    auto VulkanContext::SubmitToQueue( const QueueSubmitInfo& submitInfo ) -> void {
        // Prepare the submission to the queue. We want to wait on
        // the present semaphore, which is signaled when the swapchain
        // is ready (there's image available to render to). We will
        // signal the render semaphore, to signal that rendering has finished

        VkSubmitInfo submit{ VulkanUtils::Initializers::SubmitInfo() };
        submit.pNext = nullptr;

        VkPipelineStageFlags waitStage{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

        submit.pWaitDstStageMask = std::addressof( waitStage );

        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = std::addressof( submitInfo.SyncObjects.PresentSemaphore );

        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = std::addressof( submitInfo.SyncObjects.RenderSemaphore );

        submit.commandBufferCount = submitInfo.CommandsCount;
        submit.pCommandBuffers = submitInfo.Commands;

        // Submit command buffer to the queue for execution
        // Render fence will now block until the graphic commands finish execution
        if ( vkQueueSubmit( submitInfo.Queue, 1, std::addressof( submit ), submitInfo.SyncObjects.RenderFence ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "Error in Vulkan context on attempt to queue submit!" );
        }
    }

    auto VulkanContext::BatchCommandBuffer( VkCommandBuffer cmd ) -> void {
        // TODO: Thread safety
        // Senders are responsible of freeing the command buffers packed into
        // this array once these command buffers are no longer needed.
        s_BatchedGraphicQueueCommands.emplace_back( cmd );
    }

    auto VulkanContext::ImmediateSubmit( const std::function<void( VkCommandBuffer )>& task, VkQueue queue ) -> void {
        VkCommandBuffer cmd{ s_ImmediateSubmitContext.CommandBuffer };

        // Begin the command buffer recording. We will use this command buffer exactly
        // once before resetting, so we tell vulkan that
        VkCommandBufferBeginInfo cmdBeginInfo{ VulkanUtils::CommandBufferBeginInfo( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT ) };

        if ( vkBeginCommandBuffer( cmd, &cmdBeginInfo ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "Error on vkBeginCommandBuffer on ImmediateSubmit" );
        }

        // execute the function
        task( cmd );

        if ( vkEndCommandBuffer( cmd ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "Error on vkBeginCommandBuffer on ImmediateSubmit" );
        }

        VkSubmitInfo submitInfo{ VulkanUtils::SubmitInfo( cmd ) };


        // Submit command buffer to the queue and execute it.
        // UploadFence will now block until the graphic commands finish execution
        if ( vkQueueSubmit( queue, 1, std::addressof( submitInfo ), s_ImmediateSubmitContext.UploadFence ) ) {
            MKT_THROW_RUNTIME_ERROR( "Error on vkQueueSubmit on ImmediateSubmit" );
        }

        vkWaitForFences( GetPrimaryLogicalDevice(), 1, std::addressof( s_ImmediateSubmitContext.UploadFence ), true, 9999999999 );
        vkResetFences( GetPrimaryLogicalDevice(), 1, std::addressof( s_ImmediateSubmitContext.UploadFence ) );

        // reset the command buffers inside the command pool
        vkResetCommandPool( GetPrimaryLogicalDevice(), s_ImmediateSubmitContext.CommandPool.Get(), 0 );
    }


    auto VulkanContext::OutputDebugInfo() -> void {
        DisplayGflwRequiredInstanceExtensions();
        MKT_CORE_LOGGER_DEBUG( "Physical device: {}", GetPrimaryPhysicalDeviceProperties().deviceName );
        MKT_CORE_LOGGER_INFO( "maxUniformBufferRange for primary device is {} bytes", VulkanContext::GetPrimaryLogicalDeviceProperties().limits.maxUniformBufferRange );
    }


    auto VulkanContext::InitRenderer() -> void {
        auto& rendererInfo{ Renderer::GetRendererData() };
        rendererInfo.GPUName = GetPrimaryPhysicalDeviceProperties().deviceName;
        rendererInfo.CPUName = StringUtils::Trim( GetCPUName() );
        rendererInfo.DriverVersion = std::to_string( GetPrimaryPhysicalDeviceProperties().driverVersion );
        rendererInfo.VRAMSize = static_cast<double>(GetPrimaryPhysicalDeviceMemoryProperties().memoryHeaps[0].size) / 1'000'000;
    }


    auto VulkanContext::InitContext( const std::shared_ptr<Window>& ptr ) -> void {
        const VkResult ret{ volkInitialize() };
        s_ContextData.VOLKInitSuccess = ret == VK_SUCCESS;
        MKT_ASSERT( s_ContextData.VOLKInitSuccess, "Failed to initialize VOLK!" );

        s_ContextData.WindowHandle = std::dynamic_pointer_cast<XPWindow>( ptr );
        MKT_ASSERT( s_ContextData.WindowHandle, "Window handle for Vulkan Context initialization is NULL" );
    }
}