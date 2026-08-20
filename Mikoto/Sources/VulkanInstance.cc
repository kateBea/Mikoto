//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <ranges>

#include <volk.h>
#include <GLFW/glfw3.h>

#include <EASTL/array.h>
#include <EASTL/vector.h>
#include <EASTL/string_view.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/fixed_string.h>
#include <EASTL/unordered_set.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Exception.hh>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

#include <Memory/Allocator.hh>

#include <Renderer/Rhi/Vulkan/VulkanHelpers.hh>
#include <Renderer/Rhi/Vulkan/VulkanInstance.hh>

namespace mikoto::renderer::vulkan {

    using namespace mikoto::core;
    using namespace mikoto::platform;
    using namespace mikoto::renderer::rhi;

    MKT_NODISCARD static auto GetGlfwRequiredExtensions() -> eastl::vector<const char*> {
        u32 glfwExtensionCount{};
        const char** glfwExtensions{ glfwGetRequiredInstanceExtensions( MKT_ADDRESSOF( glfwExtensionCount ) ) };

        if ( glfwExtensions == nullptr ) {
            MKT_ASSERT( false, "Vulkan is not available on this platform!" );
        }

        return eastl::vector<const char*>{ glfwExtensions, glfwExtensions + glfwExtensionCount };
    }

    MKT_NODISCARD auto CheckValidationLayerSupport( eastl::vector<const char *>& layers ) -> bool {
        u32 layerCount{};
        vkEnumerateInstanceLayerProperties( MKT_ADDRESSOF( layerCount ), nullptr );

        eastl::vector<VkLayerProperties> availableLayers( layerCount );
        vkEnumerateInstanceLayerProperties( MKT_ADDRESSOF( layerCount ), availableLayers.data() );

        for ( const auto &layer: layers ) {
            const auto found{
                std::ranges::any_of(
                        availableLayers,
                        [&]( const VkLayerProperties &layerProperty ) -> bool {
                            return eastl::string_view{ layer } == layerProperty.layerName;
                        } )
            };

            if ( !found ) {
                return false;
            }
        }

        return true;
    }

    static auto SetupDebugMessengerCreateInfo( VkDebugUtilsMessengerCreateInfoEXT &createInfo ) -> void {
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

        createInfo.pfnUserCallback =
                []( VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                    VkDebugUtilsMessageTypeFlagsEXT messageType,
                    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                    void *pUserData ) -> VKAPI_ATTR VkBool32 {
            // Unused
            ( void )messageSeverity;
            ( void )messageType;
            ( void )pUserData;

            switch ( messageSeverity ) {
                case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
                    MKT_CORE_LOGGER_ERROR( "Vulkan [Error] {}", pCallbackData->pMessage );
                    break;
                case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
                    MKT_CORE_LOGGER_WARN( "Vulkan [Warn] {}", pCallbackData->pMessage );
                    break;
                case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
                    MKT_CORE_LOGGER_INFO( "Vulkan [Info] {}", pCallbackData->pMessage );
                    break;
                case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
                    MKT_CORE_LOGGER_DEBUG( "Vulkan [Debug] {}", pCallbackData->pMessage );
                    break;
                default:
                    MKT_CORE_LOGGER_ERROR( "Vulkan [Unhandled Severity] {}", pCallbackData->pMessage );
                    break;
            }
            return VK_FALSE;
        };
    }

    MKT_NODISCARD static auto CreateDebugUtilsMessengerEXT(
            VkInstance instance,
            VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
            VkAllocationCallbacks* pAllocator,
            VkDebugUtilsMessengerEXT* pDebugMessenger ) -> VkResult {
        if ( vkCreateDebugUtilsMessengerEXT == nullptr ) {
            MKT_CORE_LOGGER_ERROR( "Could not create DebugUtilsMessengerEXT extension 'vkCreateDebugUtilsMessengerEXT' is not available." );
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        return vkCreateDebugUtilsMessengerEXT( instance, pCreateInfo, pAllocator, pDebugMessenger );
    }

    Instance::Instance( InstanceBuilder& builder ) {
        VkApplicationInfo appInfo{ initializers::ApplicationInfo() };
        appInfo.pApplicationName = builder.mAppName.c_str();
        appInfo.pEngineName = builder.mEngineName.c_str();
        appInfo.applicationVersion = builder.mApiVersion;
        appInfo.engineVersion = builder.mEngineVersion;
        appInfo.apiVersion = builder.mApiVersion;

        // To avoid duplicated extensions we use a set
        eastl::unordered_set<eastl::string> extensionsSet{ mInstanceExtensions.begin(), mInstanceExtensions.end() };
        if (builder.mQueryGlfwExtensions) {
            auto extensions{ GetGlfwRequiredExtensions() };
            extensionsSet.insert( extensions.begin(), extensions.end() );
        }

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{ initializers::DebugUtilsMessengerCreateInfoEXT() };

        if ( builder.mEnableValidationLayers ) {
            if (CheckValidationLayerSupport( mValidationLayers )) {
                extensionsSet.emplace( VK_EXT_DEBUG_UTILS_EXTENSION_NAME );
            } else {
                MKT_CORE_LOGGER_WARN( "Requested validation layers are not available!" );
            }

            SetupDebugMessengerCreateInfo( debugCreateInfo );
        }

        mInstanceExtensions.clear();
        for (const auto& extension : extensionsSet) {
            mInstanceExtensions.emplace_back( extension.c_str() );
        }

        // Prepare structures for Core/GPU assisted validation layers
        eastl::array<VkValidationFeatureEnableEXT, 5> enabledFeatures{
            VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
            VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
            VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT,
            VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT,
            VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT };

        VkValidationFeaturesEXT validationFeatures{
            .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
            .pNext = nullptr,
            .enabledValidationFeatureCount = as<u32>( enabledFeatures.size() ),
            .pEnabledValidationFeatures = enabledFeatures.data(),
            .disabledValidationFeatureCount = 0,
            .pDisabledValidationFeatures = nullptr };

        // Sync validations??
        // https://vulkan.lunarg.com/doc/view/latest/windows/synchronization_usage.html

        // When core validations are passed check GPU assisted ones, cannot enable both, GPU assisted validations are very slow
        VkInstanceCreateInfo createInfo{ initializers::InstanceCreateInfo() };
        createInfo.pNext = MKT_ADDRESSOF( debugCreateInfo );
        createInfo.pApplicationInfo = MKT_ADDRESSOF( appInfo );

        switch (builder.mValidationLevel) {
            case InstanceBuilder::ValidationLevel::eCore:
                createInfo.pNext = MKT_ADDRESSOF( debugCreateInfo );
                break;
            case InstanceBuilder::ValidationLevel::eGpuAssisted:
                createInfo.pNext = MKT_ADDRESSOF( validationFeatures );
                break;
        }

        createInfo.enabledLayerCount = as<u32>( mValidationLayers.size() );
        createInfo.ppEnabledLayerNames = mValidationLayers.data();

        createInfo.enabledExtensionCount = as<u32>( mInstanceExtensions.size() );
        createInfo.ppEnabledExtensionNames = mInstanceExtensions.data();

        MKT_VK_CHECK(vkCreateInstance( MKT_ADDRESSOF( createInfo ), nullptr, MKT_ADDRESSOF( mInstance ) ) );

        // Load extensions
        volkLoadInstance( mInstance );

        // Initialize the surface
        if ( builder.mWindow != nullptr ) {
            // Assumes the passed window is GLFW, need to implement support for other handles
            // This logic is run if we pass a window, otherwise we do not create a surface
            const auto window{ eastl::any_cast<GLFWwindow*>( builder.mWindow->GetNativeWindow() ) };
            MKT_VK_CHECK( glfwCreateWindowSurface( mInstance, window, nullptr,  MKT_ADDRESSOF( mSurface ) ) );
        }

        QueryAvailablePhysicalDevices();

        if (mEnableValidationLayers) {
            MKT_VK_CHECK(CreateDebugUtilsMessengerEXT( mInstance, MKT_ADDRESSOF( debugCreateInfo ), nullptr, MKT_ADDRESSOF( mDebugMessenger ) ));
        }
    }

    auto PhysicalDevice::GetFamilyIndexWithSupport( QueueOpSupportFlags ops ) const -> i32 {
        const auto it{ std::ranges::find_if(mQueueInfos, [ops](const std::pair<const u32, VulkanQueueData>& data) {
            return data.second.mOpSupportFlags.Has(ops);
        }) };

        if (it != mQueueInfos.end()) {
            return it->second.FamilyIndex;
        }

        return kInvalidQueueFamilyIndex;
    }

    auto PhysicalDevice::GetQueueWithSupport( QueueOpSupportFlags ops ) const -> const VulkanQueueData* {
        const auto it{ std::ranges::find_if(mQueueInfos, [ops](const std::pair<const u32, VulkanQueueData>& data) {
            return data.second.mOpSupportFlags.Has(ops);
        }) };

        if (it != mQueueInfos.end()) {
            return MKT_ADDRESSOF( it->second );
        }

        return nullptr;
    }

    auto PhysicalDevice::HasQueueSupport( QueueOpSupportFlags ops ) const -> bool {
        return std::ranges::any_of(mQueueInfos, [ops](const std::pair<const u32, VulkanQueueData>& data) {
            return data.second.mOpSupportFlags.Has(ops);
        }) ;
    }

    auto PhysicalDevice::IsExtensionAvailable( eastl::string_view name ) const -> bool {
        return mAvailableExtensions.contains( eastl::string{ name } );
    }

    auto PhysicalDevice::IsExtensionListAvailable( eastl::span<eastl::string> extensions ) const -> bool {
        return std::ranges::all_of(extensions.begin(), extensions.end(),
            [this](auto const& name) {
                return IsExtensionAvailable( name );
            });
    }

    auto Instance::QueryAvailablePhysicalDevices() -> void {
        u32 deviceCount{};
        vkEnumeratePhysicalDevices( mInstance, MKT_ADDRESSOF( deviceCount ), nullptr );

        eastl::vector<VkPhysicalDevice> devices( deviceCount );
        vkEnumeratePhysicalDevices( mInstance, MKT_ADDRESSOF( deviceCount ), devices.data() );

        mPhysicalDevices.reserve( deviceCount );
        for ( VkPhysicalDevice& physicalDevice : devices ) {
            auto& dev{ mPhysicalDevices.emplace_back() };

            // Physical device
            dev.mPhysicalDevice = physicalDevice;

            // Extensions
            u32 extensionCount{};
            vkEnumerateDeviceExtensionProperties( physicalDevice, nullptr, MKT_ADDRESSOF( extensionCount ), nullptr );
            eastl::vector<VkExtensionProperties> extensions( extensionCount );
            vkEnumerateDeviceExtensionProperties( physicalDevice, nullptr, MKT_ADDRESSOF( extensionCount ), extensions.data() );
            std::ranges::for_each( extensions, [&] (const VkExtensionProperties& extension) mutable {
                dev.mAvailableExtensions.emplace( extension.extensionName );
            });

            // Features and properties
            vkGetPhysicalDeviceFeatures( physicalDevice, MKT_ADDRESSOF( dev.mFeatures ) );
            vkGetPhysicalDeviceFeatures2( physicalDevice, MKT_ADDRESSOF( dev.mFeatures2 ) );
            vkGetPhysicalDeviceProperties( physicalDevice, MKT_ADDRESSOF( dev.mProperties ) );
            vkGetPhysicalDeviceMemoryProperties( physicalDevice, MKT_ADDRESSOF( dev.mMemoryProperties ) );


            // Queues info
            u32 queueFamilyCount{};
            vkGetPhysicalDeviceQueueFamilyProperties( physicalDevice, MKT_ADDRESSOF( queueFamilyCount ), nullptr );

            eastl::vector<VkQueueFamilyProperties> queueFamilies( queueFamilyCount );
            vkGetPhysicalDeviceQueueFamilyProperties( physicalDevice, std::addressof( queueFamilyCount ), queueFamilies.data() );

            // Vulkan does not give the family index,
            // so I need to manually keep track of it
            // Each family can have multiple queues
            for ( u32 queueFamilyIndex{}; queueFamilyIndex < as<u32>( queueFamilies.size() ); queueFamilyIndex++ ) {
                VkQueueFamilyProperties& properties{ queueFamilies[queueFamilyIndex] };
                if (properties.queueCount == 0) {
                    continue;
                }

                VulkanQueueData& queueData{ dev.mQueueInfos[queueFamilyIndex] };

                // Fill info
                queueData.FamilyIndex = queueFamilyIndex;
                queueData.mProperties = properties;

                // Check if the queue supports graphics operations
                if (properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                    queueData.mOpSupportFlags |= QueueOpSupportFlagsBits::kGraphics;
                }

                // Check if the queue supports transfer operations
                if (properties.queueFlags & VK_QUEUE_TRANSFER_BIT) {
                    queueData.mOpSupportFlags |= QueueOpSupportFlagsBits::kTransfer;
                }

                // Check if the queue supports compute operations
                if (properties.queueFlags & VK_QUEUE_COMPUTE_BIT) {
                    queueData.mOpSupportFlags |= QueueOpSupportFlagsBits::kCompute;
                }

                if (mSurface != VK_NULL_HANDLE) {
                    // Support for presentation requested
                    VkBool32 presentSupport{ VK_FALSE };
                    MKT_VK_CHECK( vkGetPhysicalDeviceSurfaceSupportKHR( physicalDevice, queueFamilyIndex, mSurface, MKT_ADDRESSOF( presentSupport ) ) );

                    if (presentSupport == VK_TRUE) {
                        queueData.mOpSupportFlags |= QueueOpSupportFlagsBits::kPresentation;
                    }
                }
            }

            // Load swapchain capabilities
            if (mSurface != VK_NULL_HANDLE) {
                vkGetPhysicalDeviceSurfaceCapabilitiesKHR( physicalDevice, mSurface, MKT_ADDRESSOF( dev.mCapabilities ) );

                u32 formatCount{};
                vkGetPhysicalDeviceSurfaceFormatsKHR( physicalDevice, mSurface, MKT_ADDRESSOF( formatCount ), nullptr );
                if ( formatCount != 0 ) {
                    dev.mFormats.resize( formatCount );
                    vkGetPhysicalDeviceSurfaceFormatsKHR( physicalDevice, mSurface, MKT_ADDRESSOF( formatCount ), dev.mFormats.data() );
                }

                u32 presentModeCount{};
                vkGetPhysicalDeviceSurfacePresentModesKHR( physicalDevice, mSurface, MKT_ADDRESSOF( presentModeCount ), nullptr );
                if ( presentModeCount != 0 ) {
                    dev.mPresentModes.resize( presentModeCount );
                    vkGetPhysicalDeviceSurfacePresentModesKHR( physicalDevice, mSurface, MKT_ADDRESSOF( presentModeCount ), dev.mPresentModes.data() );
                }
            }
        }
    }

    Instance::~Instance() {
        // Surface
        if (mSurface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR( mInstance, mSurface, nullptr );
        }

        // Debug messanger
        if ( mDebugMessenger != VK_NULL_HANDLE ) {
            vkDestroyDebugUtilsMessengerEXT( mInstance, mDebugMessenger, nullptr );
        }

        // Instance
        if ( mInstance != VK_NULL_HANDLE ) {
            vkDestroyInstance( mInstance, nullptr );
        }
    }

    auto InstanceBuilder::SetAppName( eastl::string_view name ) -> InstanceBuilder & {
        mAppName = name;
        return *this;
    }

    auto InstanceBuilder::SetEngineName( eastl::string_view name ) -> InstanceBuilder & {
        mEngineName = name;
        return *this;
    }

    auto InstanceBuilder::SetAppVersion( u32 major, u32 minor, u32 patch ) -> InstanceBuilder & {
        mAppVersion = VK_MAKE_API_VERSION( 0, major, minor, patch );
        return *this;
    }

    auto InstanceBuilder::SetApiVersion( u32 major, u32 minor, u32 patch ) -> InstanceBuilder & {
        mApiVersion = VK_MAKE_API_VERSION( 0, major, minor, patch );
        return *this;
    }

    auto InstanceBuilder::SetEngineVersion( u32 major, u32 minor, u32 patch ) -> InstanceBuilder & {
        mEngineVersion = VK_MAKE_API_VERSION( 0, major, minor, patch );
        return *this;
    }

    auto InstanceBuilder::SetValidationLevel( ValidationLevel level ) -> InstanceBuilder & {
        mValidationLevel = level;
        return *this;
    }

    auto InstanceBuilder::EnableValidationLayers( bool value ) -> InstanceBuilder & {
        mEnableValidationLayers = value;
        return *this;
    }

    auto InstanceBuilder::QueryGLFWExtensions( bool value ) -> InstanceBuilder & {
        mQueryGlfwExtensions = value;
        return *this;
    }

    auto InstanceBuilder::QuerySurfaceSupport( Window* window ) -> InstanceBuilder& {
        mWindow = window;
        return *this;
    }

    auto InstanceBuilder::Build() -> eastl::unique_ptr<Instance> {
        return eastl::make_unique<Instance>( *this );
    }
}// namespace mikoto