/**
 * VulkanContext.cc
 * Created by kate on 7/3/23.
 * */

// C++ Standard Library
#include <any>
#include <set>
#include <vector>
#include <memory>
#include <numeric>
#include <limits>
#include <stdexcept>
#include <unordered_set>

// Third-Party Libraries
#include "fmt/format.h"
#include "vk_mem_alloc.h"
#include "volk.h"

// Project Headers
#include "Common/Common.hh"
#include "Common/StringUtils.hh"
#include "Common/VulkanUtils.hh"
#include "Core/Application.hh"
#include "Core/Assert.hh"
#include "Core/Logger.hh"
#include "Platform/GlfwWindow.hh"
#include "Platform/Window.hh"
#include "Renderer/Renderer.hh"
#include <Renderer/Vulkan/DeletionQueue.hh>
#include "Renderer/Vulkan/VulkanContext.hh"
#include "Renderer/Vulkan/VulkanSwapChain.hh"

// TODO: implement vulkan delete queue
namespace Mikoto {
    auto VulkanContext::Init(const std::shared_ptr<Window>& handle) -> void {
        // Initialize the Volk library
        const VkResult ret{ volkInitialize() };
        s_ContextData.VOLKInitSuccess = ret == VK_SUCCESS;
        MKT_ASSERT(s_ContextData.VOLKInitSuccess, "Failed to initialize VOLK!");

        // At the moment, Vulkan is working with GLFW windows on desktops
        s_ContextData.WindowHandle = std::dynamic_pointer_cast<GlfwWindow>(handle);
        MKT_ASSERT(s_ContextData.WindowHandle, "Window handle for Vulkan Context initialization is NULL");

        CreateInstance();
        SetupDebugMessenger();
        CreateSurface();
        PickPrimaryPhysicalDevice();
        CreatePrimaryLogicalDevice();


        InitSwapChain();
        InitMemoryAllocator();

        auto& rendererInfo{ Renderer::GetRendererData() };
        rendererInfo.GPUName = GetPrimaryPhysicalDeviceProperties().deviceName;
        rendererInfo.CPUName = StringUtils::Trim(GetCPUName());
        rendererInfo.DriverVersion = std::to_string(GetPrimaryPhysicalDeviceProperties().driverVersion);
        rendererInfo.VRAMSize = (double)GetPrimaryPhysicalDeviceMemoryProperties().memoryHeaps[0].size / 1'000'000;

        CreatePrimaryLogicalDeviceCommandPools();
        CreatePrimaryLogicalDeviceCommandBuffers();
        CreateSynchronizationPrimitives();

        DeletionQueue::Push([=]() -> void {
            // Clear main draw command buffers
            vkFreeCommandBuffers(GetPrimaryLogicalDevice(),
                                 s_MainCommandPool.Get(),
                                 static_cast<UInt32_T>(s_RenderCommandBufferHandles.size()),
                                 s_RenderCommandBufferHandles.data());

            // Immediate submit context objects release
            vkFreeCommandBuffers(GetPrimaryLogicalDevice(),
                                 s_ImmediateSubmitContext.CommandPool.Get(),
                                 1,
                                 std::addressof(s_ImmediateSubmitContext.CommandBuffer));

            vkDestroyFence(GetPrimaryLogicalDevice(), s_ImmediateSubmitContext.UploadFence, nullptr);
        });
    }

    auto CreateDebugUtilsMessengerEXT(
            VkInstance instance,
            const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
            const VkAllocationCallbacks* pAllocator,
            VkDebugUtilsMessengerEXT* pDebugMessenger) -> VkResult
    {
        // Exposed by volk.h
        if (vkCreateDebugUtilsMessengerEXT == nullptr)
            return VK_ERROR_EXTENSION_NOT_PRESENT;

        return vkCreateDebugUtilsMessengerEXT(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }

    auto VulkanContext::CreateInstance() -> void {
        if (s_ContextData.EnableValidationLayers && !CheckValidationLayerSupport()) {
            MKT_THROW_RUNTIME_ERROR("Validation layers requested, but not available!");
        }

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

        if (s_ContextData.EnableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

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

        if (vkCreateInstance(&createInfo, nullptr, &s_ContextData.Instance) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to create instance!");
        }

        // load all required Vulkan entry-points,
        // including all extensions
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

                MKT_CORE_LOGGER_ERROR("{}", pCallbackData->pMessage);
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

        if (deviceCount == 0) {
            MKT_THROW_RUNTIME_ERROR("Failed to find GPUs with Vulkan support!");
        }

        s_ContextData.PhysicalDevices = std::vector<VkPhysicalDevice>(deviceCount);
        s_ContextData.PhysicalDeviceFeatures = std::vector<VkPhysicalDeviceFeatures>(deviceCount);
        s_ContextData.PhysicalDeviceProperties = std::vector<VkPhysicalDeviceProperties>(deviceCount);
        s_ContextData.PhysicalDeviceMemoryProperties = std::vector<VkPhysicalDeviceMemoryProperties>(deviceCount);

        s_QueueFamiliesData = std::vector<QueuesData>(deviceCount);

        for (auto& physicalDevice : s_ContextData.PhysicalDevices) {
            physicalDevice = VK_NULL_HANDLE;
        }

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

        if (s_ContextData.PhysicalDevices[s_ContextData.PrimaryPhysicalDeviceIndex] == VK_NULL_HANDLE) {
            MKT_THROW_RUNTIME_ERROR("Could not find a suitable GPU to use as primary physical device!");
        }

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
            if ((queueFamily.queueCount > 0) && (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
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
        const QueuesData indices{ FindQueueFamilies(device) };
        const bool extensionsSupported{ CheckForDeviceRequiredExtensionSupport(device) };
        bool swapChainAdequate{}; (void)swapChainAdequate;

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
        if (s_ContextData.EnableValidationLayers && vkDestroyDebugUtilsMessengerEXT != nullptr)
            vkDestroyDebugUtilsMessengerEXT(GetInstance(), s_ContextData.DebugMessenger, nullptr);

        vkDestroySurfaceKHR(GetInstance(), GetSurface(), nullptr);

        s_SwapChain->OnRelease();

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
            if (StringUtils::Equal(extensionProperties.extensionName, targetExtensionName)) {
                return true;
            }
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

        MKT_THROW_RUNTIME_ERROR("Failed to find supported format!");
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
        const auto windowExtent{ Application::Get().GetMainWindow().GetExtent() /*remove this and pass this to prepareFrame*/ };

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

    auto VulkanContext::GetDetailedStatistics() -> const VmaTotalStatistics& {
        vmaCalculateStatistics(s_DefaultAllocator, std::addressof(s_TotalStatistics));

        return s_TotalStatistics;
    }

    auto VulkanContext::CreatePrimaryLogicalDeviceCommandPools() -> void {
        VkCommandPoolCreateInfo createInfo{ VulkanUtils::Initializers::CommandPoolCreateInfo() };
        auto queueFamilyData{ FindQueueFamilies(GetPrimaryPhysicalDevice()) };

        createInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        createInfo.queueFamilyIndex = queueFamilyData.GraphicsFamilyIndex;

        s_MainCommandPool.OnCreate(createInfo);

        // Immediate submit command pull
        // default command pool we will use for immediate submission
        VkCommandPoolCreateInfo immediateSubmitCreateInfo{ VulkanUtils::Initializers::CommandPoolCreateInfo() };
        immediateSubmitCreateInfo.queueFamilyIndex = queueFamilyData.GraphicsFamilyIndex;
        immediateSubmitCreateInfo.flags = 0;

        s_ImmediateSubmitContext.CommandPool.OnCreate(immediateSubmitCreateInfo);
    }

    auto VulkanContext::CreatePrimaryLogicalDeviceCommandBuffers() -> void {
        // create one command buffer for each swapchain image and reuse during rendering
        const UInt32_T commandBuffersCount{ static_cast<UInt32_T>(GetSwapChain()->GetImageCount()) };

        VkCommandBufferAllocateInfo allocInfo{ VulkanUtils::Initializers::CommandBufferAllocateInfo() };
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = s_MainCommandPool.Get();
        allocInfo.commandBufferCount = 1;

        for (Size_T count{}; count < commandBuffersCount; ++count) {
            VkCommandBuffer cmd{};

            if (vkAllocateCommandBuffers(VulkanContext::GetPrimaryLogicalDevice(), &allocInfo, std::addressof(cmd)) != VK_SUCCESS) {
                MKT_THROW_RUNTIME_ERROR("Failed to allocate command buffer");
            }

            s_RenderCommandBufferHandles.emplace_back(cmd);
        }

        // Immediate submit command buffers
        VkCommandBufferAllocateInfo immediateSubmitAllocInfo{ VulkanUtils::Initializers::CommandBufferAllocateInfo() };
        immediateSubmitAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        immediateSubmitAllocInfo.commandPool = s_ImmediateSubmitContext.CommandPool.Get();
        immediateSubmitAllocInfo.commandBufferCount = 1;

        // default command buffer we will use for immediate submission
        vkAllocateCommandBuffers(VulkanContext::GetPrimaryLogicalDevice(),
                                 std::addressof(immediateSubmitAllocInfo),
                                 std::addressof(s_ImmediateSubmitContext.CommandBuffer));
    }

    auto VulkanContext::PrepareFrame() -> void {
        auto ret{ GetSwapChain()->GetNextImageIndex(s_CurrentImageIndex,
                                                   s_SwapChainSyncObjects.RenderFence,
                                                   s_SwapChainSyncObjects.PresentSemaphore) };

        if (ret == VK_ERROR_OUT_OF_DATE_KHR) {
            VulkanSwapChainCreateInfo createInfo{ VulkanContext::GetSwapChain()->GetSwapChainCreateInfo() };
            VulkanContext::RecreateSwapChain(std::move(createInfo));
        }

        if (ret != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to acquire swap chain image!");
        }
    }

    auto VulkanContext::SubmitFrame() -> void {
        QueueSubmitInfo submitInfo{};

        // Specify present and render semaphores
        submitInfo.SyncObjects.RenderFence = s_SwapChainSyncObjects.RenderFence;
        submitInfo.SyncObjects.RenderSemaphore = s_SwapChainSyncObjects.RenderSemaphore;
        submitInfo.SyncObjects.PresentSemaphore = s_SwapChainSyncObjects.PresentSemaphore;

        // Setup commands and specify queue
        submitInfo.CommandsCount = static_cast<UInt32_T>(s_BatchedGraphicQueueCommands.size());
        submitInfo.Commands = s_BatchedGraphicQueueCommands.data();
        submitInfo.Queue = GetPrimaryLogicalDeviceGraphicsQueue();

        SubmitToQueue(submitInfo);

        s_BatchedGraphicQueueCommands.clear();
    }

    auto VulkanContext::SubmitCommands(const VkCommandBuffer* commands, const UInt32_T count) -> void {
        QueueSubmitInfo submitInfo{};

        // Specify present and render semaphores
        submitInfo.SyncObjects.RenderFence = s_SwapChainSyncObjects.RenderFence;
        submitInfo.SyncObjects.RenderSemaphore = s_SwapChainSyncObjects.RenderSemaphore;
        submitInfo.SyncObjects.PresentSemaphore = s_SwapChainSyncObjects.PresentSemaphore;

        // Setup commands and specify queue
        submitInfo.CommandsCount = count;
        submitInfo.Commands = commands;
        submitInfo.Queue = GetPrimaryLogicalDeviceGraphicsQueue();

        SubmitToQueue(submitInfo);
    }

    auto VulkanContext::Present() -> void {
        auto& swapChain{ *GetSwapChain() };
        auto& syncObjects{ VulkanContext::s_SwapChainSyncObjects };

        auto result{ swapChain.Present(s_CurrentImageIndex, syncObjects.RenderSemaphore) };

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            VulkanSwapChainCreateInfo createInfo{ swapChain.GetSwapChainCreateInfo() };
            VulkanContext::RecreateSwapChain(std::move(createInfo));
            return;
        }

        if (result != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to submit imgui command buffers!");
        }
    }

    auto VulkanContext::CreateSynchronizationPrimitives() -> void {
        VkFenceCreateInfo fenceInfo{ VulkanUtils::Initializers::FenceCreateInfo() };

        //we want to create the fence with the Create Signaled flag, so we
        // can wait on it before using it on a GPU command (for the first frame)
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        if (vkCreateFence(VulkanContext::GetPrimaryLogicalDevice(),
                          std::addressof(fenceInfo),
                          nullptr,
                          std::addressof(s_SwapChainSyncObjects.RenderFence)) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR(fmt::format("Failed to create Vulkan render fence!"));
        }

        VkSemaphoreCreateInfo semaphoreCreateInfo{ VulkanUtils::Initializers::SemaphoreCreateInfo() };

        if (vkCreateSemaphore(VulkanContext::GetPrimaryLogicalDevice(),
                              std::addressof(semaphoreCreateInfo),
                              nullptr,
                              std::addressof(s_SwapChainSyncObjects.PresentSemaphore)) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR(fmt::format("Failed to create Vulkan present semaphore"));
        }

        if (vkCreateSemaphore(VulkanContext::GetPrimaryLogicalDevice(),
                              std::addressof(semaphoreCreateInfo),
                              nullptr,
                              std::addressof(s_SwapChainSyncObjects.RenderSemaphore)) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR(fmt::format("Failed to create Vulkan render semaphore"));
        }

        // Immediate submit synchronization primitives
        VkFenceCreateInfo immediateSubmitFenceInfo{ VulkanUtils::Initializers::FenceCreateInfo() };

        if (vkCreateFence(VulkanContext::GetPrimaryLogicalDevice(),
                          std::addressof(immediateSubmitFenceInfo),
                          nullptr,
                          std::addressof(s_ImmediateSubmitContext.UploadFence)) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR(fmt::format("Failed to create Vulkan immediate submit fence!"));
        }
    }

    auto VulkanContext::SubmitToQueue(const QueueSubmitInfo& submitInfo) -> void {
        // Prepare the submission to the queue
        // We want to wait on the present semaphore, which is signaled when the swapchain is ready (there's image available to render to)
        // We will signal the render semaphore, to signal that rendering has finished

        VkSubmitInfo submit{ VulkanUtils::Initializers::SubmitInfo() };
        submit.pNext = nullptr;

        VkPipelineStageFlags waitStage{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

        submit.pWaitDstStageMask = std::addressof(waitStage);

        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = std::addressof(submitInfo.SyncObjects.PresentSemaphore);

        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = std::addressof(submitInfo.SyncObjects.RenderSemaphore);

        submit.commandBufferCount = submitInfo.CommandsCount;
        submit.pCommandBuffers = submitInfo.Commands;

        // Submit command buffer to the queue for execution
        // Render fence will now block until the graphic commands finish execution
        if (vkQueueSubmit(submitInfo.Queue, 1, std::addressof(submit), submitInfo.SyncObjects.RenderFence) != VK_SUCCESS)  {
            MKT_THROW_RUNTIME_ERROR("Error in Vulkan context on attempt to queue submit!");
        }
    }

    auto VulkanContext::BatchCommandBuffer(VkCommandBuffer cmd) -> void {
        s_BatchedGraphicQueueCommands.emplace_back(cmd);
    }

    auto VulkanContext::ImmediateSubmit(const std::function<void(VkCommandBuffer)>& task, VkQueue queue) -> void {
        VkCommandBuffer cmd{ s_ImmediateSubmitContext.CommandBuffer };

        // Begin the command buffer recording. We will use this command buffer exactly
        // once before resetting, so we tell vulkan that
        VkCommandBufferBeginInfo cmdBeginInfo{ VulkanUtils::CommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT) };

        if (vkBeginCommandBuffer(cmd, &cmdBeginInfo) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Error on vkBeginCommandBuffer on ImmediateSubmit");
        }

        // execute the function
        task(cmd);

        if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Error on vkBeginCommandBuffer on ImmediateSubmit");
        }

        VkSubmitInfo submitInfo{ VulkanUtils::SubmitInfo(cmd) };


        // Submit command buffer to the queue and execute it.
        // UploadFence will now block until the graphic commands finish execution
        if (vkQueueSubmit(queue, 1, std::addressof(submitInfo), s_ImmediateSubmitContext.UploadFence)) {
            MKT_THROW_RUNTIME_ERROR("Error on vkQueueSubmit on ImmediateSubmit");
        }

        vkWaitForFences(GetPrimaryLogicalDevice(), 1, std::addressof(s_ImmediateSubmitContext.UploadFence), true, 9999999999);
        vkResetFences(GetPrimaryLogicalDevice(), 1, std::addressof(s_ImmediateSubmitContext.UploadFence));

        // reset the command buffers inside the command pool
        vkResetCommandPool(GetPrimaryLogicalDevice(), s_ImmediateSubmitContext.CommandPool.Get(), 0);
    }
}