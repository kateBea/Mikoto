/**
 * VulkanContext.cc
 * Created by kate on 7/3/23.
 * */

// C++ Standard Library
#include <any>
#include <set>
#include <vector>
#include <memory>
#include <stdexcept>
#include <unordered_set>

// Third-Party Libraries
#include <volk.h>
#include <vk_mem_alloc.h>

// Project Headers
#include <Utility/Common.hh>
#include <Utility/VulkanUtils.hh>
#include <Core/Assert.hh>
#include <Core/Logger.hh>
#include <Core/Application.hh>
#include <Platform/Window/Window.hh>
#include <Platform/Window/MainWindow.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanSwapChain.hh>

namespace Mikoto {
    auto VulkanContext::Init(const std::shared_ptr<Window>& handle) -> void {
        // Initialize the Volk library
        VkResult ret{ volkInitialize() };
        s_ContextData.VOLKInitSuccess = ret == VK_SUCCESS;
        MKT_ASSERT(s_ContextData.VOLKInitSuccess, "Failed to initialize VOLK!");

        // At the moment, Vulkan works with GLFW windows on desktop
        s_ContextData.WindowHandle = std::dynamic_pointer_cast<MainWindow>(handle);
        MKT_ASSERT(s_ContextData.WindowHandle, "Window handle for Vulkan Context initialization is NULL");

        CreateInstance();
        SetupDebugMessenger();
        CreateSurface();
        PickPrimaryPhysicalDevice();
        CreatePrimaryLogicalDevice();

        InitSwapChain();
        InitMemoryAllocator();
    }

    auto CreateDebugUtilsMessengerEXT(
            VkInstance instance,
            const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
            const VkAllocationCallbacks* pAllocator,
            VkDebugUtilsMessengerEXT* pDebugMessenger) -> VkResult
    {
        if (vkCreateDebugUtilsMessengerEXT == nullptr)
            return VK_ERROR_EXTENSION_NOT_PRESENT;

        return vkCreateDebugUtilsMessengerEXT(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }

    auto VulkanContext::CreateInstance() -> void {
        if (s_ContextData.EnableValidationLayers && !CheckValidationLayerSupport())
            throw std::runtime_error("Validation layers requested, but not available!");

        // Setup application data
        VkApplicationInfo appInfo{ VulkanUtils::Initializers::ApplicationInfo() };

        appInfo.pApplicationName = "Mikoto Engine";
        appInfo.applicationVersion = VK_MAKE_API_VERSION(0,
                                                         MKT_ENGINE_VERSION_MAJOR,
                                                         MKT_ENGINE_VERSION_MINOR,
                                                         MKT_ENGINE_VERSION_PATCH);
        appInfo.pEngineName = "Mikoto";
        appInfo.engineVersion = VK_MAKE_API_VERSION(0,
                                                    MKT_ENGINE_VERSION_MAJOR,
                                                    MKT_ENGINE_VERSION_MINOR,
                                                    MKT_ENGINE_VERSION_PATCH);

        appInfo.apiVersion = VK_MAKE_API_VERSION(MKT_VULKAN_VERSION_VARIANT,
                                                 MKT_VULKAN_VERSION_MAJOR,
                                                 MKT_VULKAN_VERSION_MINOR,
                                                 MKT_VULKAN_VERSION_PATCH); // Patch version should always be set to zero, see Vulkan Spec


        VkInstanceCreateInfo createInfo{ VulkanUtils::Initializers::InstanceCreateInfo() };
        createInfo.pApplicationInfo = &appInfo;

        // Setup required extensions
        auto extensions{ GetGlfwRequiredExtensions() };

        if (s_ContextData.EnableValidationLayers)
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        createInfo.enabledExtensionCount = static_cast<UInt32_T>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        // Setup debug messenger utility
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{ VulkanUtils::Initializers::DebugUtilsMessengerCreateInfoEXT() };

        if (s_ContextData.EnableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<UInt32_T>(s_ValidationLayers.size());
            createInfo.ppEnabledLayerNames = s_ValidationLayers.data();

            PopulateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = reinterpret_cast<VkDebugUtilsMessengerCreateInfoEXT*>(&debugCreateInfo);
        }

        if (vkCreateInstance(&createInfo, nullptr, &s_ContextData.Instance) != VK_SUCCESS)
            throw std::runtime_error("failed to create instance!");

        // load all required Vulkan entry-points, including all extensions
        volkLoadInstance(s_ContextData.Instance);

#if !defined(NDEBUG)
        DisplayGflwRequiredInstanceExtensions();
#endif
    }

    auto VulkanContext::GetGlfwRequiredExtensions() -> std::vector<const char*> {
        UInt32_T glfwExtensionCount{};
        const char** glfwExtensions{ glfwGetRequiredInstanceExtensions(&glfwExtensionCount) };

        if (glfwExtensions == nullptr)
            throw std::runtime_error("Vulkan is not available on this machine");

        return { glfwExtensions, glfwExtensions + glfwExtensionCount };
    }

    auto VulkanContext::SetupDebugMessenger() -> void {
        if (!s_ContextData.EnableValidationLayers)
            return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo{ VulkanUtils::Initializers::DebugUtilsMessengerCreateInfoEXT() };
        PopulateDebugMessengerCreateInfo(createInfo);
        if (CreateDebugUtilsMessengerEXT(s_ContextData.Instance, &createInfo, nullptr, &s_ContextData.DebugMessenger) != VK_SUCCESS)
            throw std::runtime_error("Failed to set up debug messenger!");
    }

    auto VulkanContext::CreateSurface() -> void {
        s_ContextData.WindowHandle->CreateWindowSurface(s_ContextData.Instance, &s_ContextData.Surface);
    }

    auto VulkanContext::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) -> void {
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

        createInfo.pfnUserCallback =
            [](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
               VkDebugUtilsMessageTypeFlagsEXT messageType,
               const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) -> VKAPI_ATTR VkBool32
            {
                // Unused
                (void)messageSeverity;
                (void)messageType;
                (void)pUserData;

                MKT_CORE_LOGGER_ERROR("Validation layer: {}", pCallbackData->pMessage);
                return VK_FALSE;
            };
    }

    auto VulkanContext::DisplayGflwRequiredInstanceExtensions() -> void {
        UInt32_T extensionCount{};
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> extensions(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

        MKT_COLOR_PRINT_FORMATTED(MKT_FMT_COLOR_AQUA, "Available extensions: \n");
        std::unordered_set<std::string> available{};
        for (const auto& extension : extensions) {
            MKT_COLOR_PRINT_FORMATTED(MKT_FMT_COLOR_GREEN_YELLOW, "\t{}\n", extension.extensionName);
            available.insert(extension.extensionName);
        }

        MKT_COLOR_PRINT_FORMATTED(MKT_FMT_COLOR_AQUA, "Required extensions: \n");
        auto requiredExtensions{ GetGlfwRequiredExtensions() };
        for (const auto& required: requiredExtensions) {
            MKT_COLOR_PRINT_FORMATTED(MKT_FMT_COLOR_ORANGE_RED, "\t{}\n", required);
            if (available.find(required) == available.end()) {
                throw std::runtime_error("Missing required GLFW extensions");
            }
        }
    }

    auto VulkanContext::CheckValidationLayerSupport() -> bool {
        UInt32_T layerCount{};
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        bool validationLayerFound{ false };
        std::vector<VkLayerProperties>::size_type index{};
        const std::string validationLayerTarget{ s_ValidationLayers[0] };

        while (index < availableLayers.size() && !validationLayerFound) {
            validationLayerFound = validationLayerTarget == availableLayers[index].layerName;
            ++index;
        }

        return validationLayerFound;
    }

    auto VulkanContext::SetupPhysicalDevicesData() -> void {
        UInt32_T deviceCount {};
        vkEnumeratePhysicalDevices(s_ContextData.Instance, &deviceCount, nullptr);

        if (deviceCount == 0)
            throw std::runtime_error("Failed to find GPUs with Vulkan support!");

        s_ContextData.PhysicalDevices = std::vector<VkPhysicalDevice>(deviceCount);
        s_ContextData.PhysicalDeviceFeatures = std::vector<VkPhysicalDeviceFeatures>(deviceCount);
        s_ContextData.PhysicalDeviceProperties = std::vector<VkPhysicalDeviceProperties>(deviceCount);
        s_ContextData.PhysicalDeviceMemoryProperties = std::vector<VkPhysicalDeviceMemoryProperties>(deviceCount);

        s_QueueFamiliesData = std::vector<QueuesData>(deviceCount);

        for (auto& physicalDevice : s_ContextData.PhysicalDevices)
            physicalDevice = VK_NULL_HANDLE;

        // Collect all physical devices available in the machine
        vkEnumeratePhysicalDevices(s_ContextData.Instance, &deviceCount, s_ContextData.PhysicalDevices.data());
        MKT_CORE_LOGGER_DEBUG("Vulkan device count: {}", deviceCount);

        // Load physical device info.
        // This is a direct mapping between physical devices position in s_ContextData.PhysicalDevices with
        // the corresponding position in s_ContextData.PhysicalDeviceFeatures, s_ContextData.PhysicalDeviceProperties,
        // s_ContextData.PhysicalDeviceMemoryProperties (physical device at index 0 has device properties at index 0, and so on)
        UInt32_T deviceIndex{};
        for (auto& physicalDevice : s_ContextData.PhysicalDevices) {
            vkGetPhysicalDeviceFeatures(physicalDevice, std::addressof(s_ContextData.PhysicalDeviceFeatures[deviceIndex]));
            vkGetPhysicalDeviceProperties(physicalDevice, std::addressof(s_ContextData.PhysicalDeviceProperties[deviceIndex]));
            vkGetPhysicalDeviceMemoryProperties(physicalDevice, std::addressof(s_ContextData.PhysicalDeviceMemoryProperties[deviceIndex]));

            ++deviceIndex;
        }
    }

    auto VulkanContext::PickPrimaryPhysicalDevice() -> void {
        SetupPhysicalDevicesData();

        bool foundSuitablePrimaryGPU{ false };
        decltype(s_ContextData.PhysicalDevices)::size_type index{};

        while (index < s_ContextData.PhysicalDevices.size() && !foundSuitablePrimaryGPU) {
            if (IsDeviceSuitable(s_ContextData.PhysicalDevices[index])) {
                s_ContextData.PrimaryPhysicalDeviceIndex = index;
                foundSuitablePrimaryGPU = true;
            }

            ++index;
        }

        if (s_ContextData.PhysicalDevices[s_ContextData.PrimaryPhysicalDeviceIndex] == VK_NULL_HANDLE)
            throw std::runtime_error("Could not find a suitable GPU to use as primary physical device!");

#if !defined(NDEBUG)
        MKT_CORE_LOGGER_DEBUG("Physical device: {}", GetPrimaryPhysicalDeviceProperties().deviceName);
#endif
    }



    auto VulkanContext::FindQueueFamilies(VkPhysicalDevice device) -> QueuesData {
        QueuesData indices{};

        UInt32_T queueFamilyCount{};
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        Int32_T i{};
        for (const auto& queueFamily : queueFamilies) {
            if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.GraphicsFamilyIndex = i;
                indices.GraphicsFamilyHasValue = true;
            }

            VkBool32 presentSupport{ VK_FALSE };
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, s_ContextData.Surface, &presentSupport);
            if (queueFamily.queueCount > 0 && presentSupport) {
                indices.PresentFamilyIndex = i;
                indices.PresentFamilyHasValue = true;
            }

            if (indices.IsComplete())
                break;

            i++;
        }

        return indices;
    }

    auto VulkanContext::CheckForDeviceRequiredExtensionSupport(VkPhysicalDevice device) -> bool {
        UInt32_T extensionCount{};
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        UInt32_T matchedExtensions{};

        std::string required{};
        std::string supported{};

        for (const auto& reqExtension : s_DeviceRequiredExtensions) {
            required = reqExtension;
            for (const auto& extension : availableExtensions) {
                matchedExtensions += required == std::string(extension.extensionName) ? 1 : 0;
            }
        }

        return matchedExtensions == s_DeviceRequiredExtensions.size();
    }

    auto VulkanContext::QuerySwapChainSupport(VkPhysicalDevice device) -> SwapChainSupportDetails {
        SwapChainSupportDetails details{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, s_ContextData.Surface, &details.Capabilities);

        UInt32_T formatCount{};
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, s_ContextData.Surface, &formatCount, nullptr);
        if (formatCount != 0) {
            details.Formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, s_ContextData.Surface, &formatCount, details.Formats.data());
        }

        UInt32_T presentModeCount{};
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, s_ContextData.Surface, &presentModeCount, nullptr);
        if (presentModeCount != 0) {
            details.PresentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, s_ContextData.Surface, &presentModeCount, details.PresentModes.data());
        }

        return details;
    }

    auto VulkanContext::IsDeviceSuitable(VkPhysicalDevice device) -> bool {
        QueuesData indices{ FindQueueFamilies(device) };
        bool extensionsSupported{ CheckForDeviceRequiredExtensionSupport(device) };
        bool swapChainAdequate{};
        (void)swapChainAdequate;

        if (extensionsSupported) {
            SwapChainSupportDetails swapChainSupport{ QuerySwapChainSupport(device) };
            swapChainAdequate = !swapChainSupport.Formats.empty() && !swapChainSupport.PresentModes.empty();
        }

        VkPhysicalDeviceFeatures supportedFeatures{};
        vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

        return indices.IsComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
    }

    auto VulkanContext::CreatePrimaryLogicalDevice() -> void {
        s_QueueFamiliesData[s_ContextData.PrimaryPhysicalDeviceIndex] = FindQueueFamilies(s_ContextData.PhysicalDevices[s_ContextData.PrimaryPhysicalDeviceIndex]);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};
        std::set<UInt32_T> uniqueQueueFamilies{ s_QueueFamiliesData[s_ContextData.PrimaryPhysicalDeviceIndex].GraphicsFamilyIndex,
                                                s_QueueFamiliesData[s_ContextData.PrimaryPhysicalDeviceIndex].PresentFamilyIndex };

        constexpr float queuePriority{ 1.0f };
        for (UInt32_T queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{ VulkanUtils::Initializers::DeviceQueueCreateInfo() };

            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;

            queueCreateInfos.push_back(queueCreateInfo);
        }

        // Requested device features
        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.samplerAnisotropy = VK_TRUE;
        deviceFeatures.fillModeNonSolid = VK_TRUE; // required for wireframe mode

        VkDeviceCreateInfo createInfo{ VulkanUtils::Initializers::DeviceCreateInfo() };

        createInfo.queueCreateInfoCount = static_cast<UInt32_T>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount = static_cast<UInt32_T>(s_DeviceRequiredExtensions.size());
        createInfo.ppEnabledExtensionNames = s_DeviceRequiredExtensions.data();

        // might not be necessary anymore because device-specific validation layers have been deprecated
        // even tho recommended for some backwards compatibility as they are required for some Vulkan implementations
        if (s_ContextData.EnableValidationLayers) {
            // These two fields are ignored by up-to-date Vulkan implementations
            createInfo.enabledLayerCount = static_cast<UInt32_T>(s_ValidationLayers.size());
            createInfo.ppEnabledLayerNames = s_ValidationLayers.data();
        }
        else {
            createInfo.enabledLayerCount = 0;
        }

        // The app only uses one logical device which is
        // going to be the main logical device
        s_ContextData.PrimaryLogicalDeviceIndex = 0;
        s_ContextData.LogicalDevices = std::vector<VkDevice>(1);

        if (vkCreateDevice(GetPrimaryPhysicalDevice(), &createInfo, nullptr, &s_ContextData.LogicalDevices[s_ContextData.PrimaryLogicalDeviceIndex]) != VK_SUCCESS)
            throw std::runtime_error("Failed to create logical device!");

        /**
         * [...] all device-related function calls, such as vkCmdDraw, will go through Vulkan loader dispatch code.
         * This allows you to transparently support multiple VkDevice objects in the same application, but comes at
         * a price of dispatch overhead which can be as high as 7% depending on the driver and application.
         *
         * To avoid this, For applications that use just one VkDevice object, load device-related
         * Vulkan entry-points directly from the driver with void volkLoadDevice(VkDevice device);
         * See: https://github.com/zeux/volk
         * */
        volkLoadDevice(GetPrimaryLogicalDevice());

        vkGetDeviceQueue(s_ContextData.LogicalDevices[s_ContextData.PrimaryLogicalDeviceIndex],
                         s_QueueFamiliesData[s_ContextData.PrimaryPhysicalDeviceIndex].GraphicsFamilyIndex, 0, &s_QueueFamiliesData[s_ContextData.PrimaryPhysicalDeviceIndex].GraphicsQueue);

        vkGetDeviceQueue(s_ContextData.LogicalDevices[s_ContextData.PrimaryLogicalDeviceIndex],
                         s_QueueFamiliesData[s_ContextData.PrimaryPhysicalDeviceIndex].PresentFamilyIndex, 0, &s_QueueFamiliesData[s_ContextData.PrimaryPhysicalDeviceIndex].PresentQueue);
    }

    auto VulkanContext::FindMemoryType(UInt32_T typeFilter, VkMemoryPropertyFlags properties, VkPhysicalDevice device) -> UInt32_T {
        VkPhysicalDeviceMemoryProperties memProperties{};
        vkGetPhysicalDeviceMemoryProperties(device, &memProperties);

        for (UInt32_T i{}; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
                return i;
        }

        throw std::runtime_error("failed to find suitable memory type!");
    }

    auto VulkanContext::ShutDown() -> void {
        VulkanUtils::WaitOnDevice(VulkanContext::GetPrimaryLogicalDevice());

        if (s_ContextData.EnableValidationLayers && vkDestroyDebugUtilsMessengerEXT != nullptr)
            vkDestroyDebugUtilsMessengerEXT(GetInstance(), s_ContextData.DebugMessenger, nullptr);

        s_SwapChain->OnRelease();
        vkDestroySurfaceKHR(GetInstance(), GetSurface(), nullptr);

        vkDestroyDevice(GetPrimaryLogicalDevice(), nullptr);
        vkDestroyInstance(GetInstance(), nullptr);
        vmaDestroyAllocator(s_DefaultAllocator);
    }

    MKT_UNUSED_FUNC auto VulkanContext::IsExtensionAvailable(std::string_view targetExtensionName, VkPhysicalDevice device) -> bool {
        UInt32_T extensionCount{};
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        for (const auto& extensionProperties : availableExtensions) {
            if (std::string(extensionProperties.extensionName) == std::string(targetExtensionName))
                return true;
        }

        return false;
    }

    auto VulkanContext::FindSupportedFormat(VkPhysicalDevice device, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) -> VkFormat {
        for (VkFormat format : candidates) {
            VkFormatProperties props{};
            vkGetPhysicalDeviceFormatProperties(device, format, &props);

            if ((tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) ||
                (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features))
            {
                return format;
            }
        }

        throw std::runtime_error("failed to find supported format!");
    }

    auto VulkanContext::RecreateSwapChain(VulkanSwapChainCreateInfo&& info) -> void {
        if (s_SwapChain)
            s_SwapChain->OnRelease();

        s_SwapChain->OnCreate(std::move(info));
    }

    auto VulkanContext::EnableVSync() -> void {
        SwitchVSync_H(true);
    }

    auto VulkanContext::DisableVSync() -> void {
        SwitchVSync_H(false);
    }

    auto VulkanContext::SwitchVSync_H(bool value) -> void {
        const auto windowExtent{ Application::Get().GetMainWindow().GetExtent() };

        VulkanSwapChainCreateInfo createInfo{
                .OldSwapChain = s_SwapChain->GetSwapChainKHR(),
                .Extent = { (UInt32_T)windowExtent.first, (UInt32_T)windowExtent.second },
                .VSyncEnable = value,
        };

        RecreateSwapChain(std::move(createInfo));
    }

    auto VulkanContext::IsVSyncActive() -> bool {
        return s_SwapChain->GetSwapChainCreateInfo().VSyncEnable;
    }

    auto VulkanContext::Present() -> void {
        // Acquire the next image ready to be presented
        // Present image to screen
    }

    auto VulkanContext::InitSwapChain() -> void {
        s_SwapChain = std::make_shared<VulkanSwapChain>();
        s_SwapChain->OnCreate(VulkanSwapChain::GetDefaultCreateInfo());
    }

    auto VulkanContext::InitMemoryAllocator() -> void {
        // Setup Vulkan Functions
        VmaVulkanFunctions vulkanFunctions{};
        vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;  // Required when using VMA_DYNAMIC_VULKAN_FUNCTIONS.
        vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;      // Required when using VMA_DYNAMIC_VULKAN_FUNCTIONS.
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
        vulkanFunctions.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2;             // Fetch "vkGetBufferMemoryRequirements2" on Vulkan >= 1.1, fetch "vkGetBufferMemoryRequirements2KHR" when using VK_KHR_dedicated_allocation extension.
        vulkanFunctions.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2;               // Fetch "vkGetImageMemoryRequirements2" on Vulkan >= 1.1, fetch "vkGetImageMemoryRequirements2KHR" when using VK_KHR_dedicated_allocation extension.
        vulkanFunctions.vkBindBufferMemory2KHR = vkBindBufferMemory2;                                   // Fetch "vkBindBufferMemory2" on Vulkan >= 1.1, fetch "vkBindBufferMemory2KHR" when using VK_KHR_bind_memory2 extension.
        vulkanFunctions.vkBindImageMemory2KHR = vkBindImageMemory2;                                     // Fetch "vkBindImageMemory2" on Vulkan >= 1.1, fetch "vkBindImageMemory2KHR" when using VK_KHR_bind_memory2 extension.
        vulkanFunctions.vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2;
        vulkanFunctions.vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements;      // Fetch from "vkGetDeviceBufferMemoryRequirements" on Vulkan >= 1.3, but you can also fetch it from "vkGetDeviceBufferMemoryRequirementsKHR" if you enabled extension VK_KHR_maintenance4.
        vulkanFunctions.vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements;        // Fetch from "vkGetDeviceImageMemoryRequirements" on Vulkan >= 1.3, but you can also fetch it from "vkGetDeviceImageMemoryRequirementsKHR" if you enabled extension VK_KHR_maintenance4.

        // Setup VmaAllocator
        VmaAllocatorCreateInfo allocatorInfo{};
        allocatorInfo.vulkanApiVersion = VK_MAKE_API_VERSION(MKT_VULKAN_VERSION_VARIANT, MKT_VULKAN_VERSION_MAJOR, MKT_VULKAN_VERSION_MINOR, MKT_VULKAN_VERSION_PATCH);
        allocatorInfo.physicalDevice = GetPrimaryPhysicalDevice();
        allocatorInfo.device = GetPrimaryLogicalDevice();
        allocatorInfo.instance = GetInstance();
        allocatorInfo.pVulkanFunctions = &vulkanFunctions;

        vmaCreateAllocator(&allocatorInfo, &s_DefaultAllocator);
    }
}