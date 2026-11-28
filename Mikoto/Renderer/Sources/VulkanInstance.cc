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

#include <EASTL/array.h>
#include <EASTL/vector.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/string_view.h>
#include <EASTL/fixed_string.h>
#include <EASTL/unordered_set.h>

#include <volk.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

#include <Memory/Allocator.hh>

#include <Platform/Utility.hh>

#include <Renderer/Rhi/Vulkan/VulkanDlss.hh>
#include <Renderer/Rhi/Vulkan/VulkanHelpers.hh>
#include <Renderer/Rhi/Vulkan/VulkanInstance.hh>

namespace mikoto::renderer::vulkan {

    using namespace mikoto::core;
    using namespace mikoto::platform;
    using namespace mikoto::renderer::rhi;

    MKT_NODISCARD static auto CheckValidationLayerSupport( eastl::vector<const char*>& layers ) -> bool {
        MKT_BEGIN_PROFILER();

        core::u32 layerCount{};
        vkEnumerateInstanceLayerProperties( MKT_ADDRESSOF( layerCount ), nullptr );

        eastl::vector<VkLayerProperties> availableLayers( layerCount );
        vkEnumerateInstanceLayerProperties( MKT_ADDRESSOF( layerCount ), availableLayers.data() );

        for ( const auto& layer: layers ) {
            const auto found{
                std::ranges::find_if(
                    availableLayers,
                    [&]( const VkLayerProperties& layerProperty ) -> bool {
                return eastl::string_view{ layer } == layerProperty.layerName;
            } )
            };

            if ( found == availableLayers.end() ) {
                return false;
            }

            MKT_CORE_LOGGER_INFO( "Vulkan validation layer: {} (API {}.{}.{})", layer,
                VK_VERSION_MAJOR( found->specVersion ), VK_VERSION_MINOR( found->specVersion ),
                VK_VERSION_PATCH( found->specVersion ) );
        }

        return true;
    }

    static auto SetupDebugMessengerCreateInfo( VkDebugUtilsMessengerCreateInfoEXT& createInfo ) -> void {
        MKT_BEGIN_PROFILER();

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
        MKT_BEGIN_PROFILER();

        if ( vkCreateDebugUtilsMessengerEXT == nullptr ) {
            MKT_CORE_LOGGER_ERROR( "Could not create DebugUtilsMessengerEXT extension 'vkCreateDebugUtilsMessengerEXT' is not available." );
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        return vkCreateDebugUtilsMessengerEXT( instance, pCreateInfo, pAllocator, pDebugMessenger );
    }

    Instance::Instance( InstanceBuilder& builder ) {
        MKT_BEGIN_PROFILER();
        // TODO: Test VK_LAYER_LUNARG_api_dump

        VkApplicationInfo appInfo{ initializers::ApplicationInfo() };
        appInfo.pApplicationName = builder.mAppName.c_str();
        appInfo.pEngineName = builder.mEngineName.c_str();
        appInfo.applicationVersion = builder.mApiVersion;
        appInfo.engineVersion = builder.mEngineVersion;
        appInfo.apiVersion = builder.mApiVersion;

        // To avoid duplicated extensions we use a set
        eastl::unordered_set<eastl::string> extensionsSet{ mInstanceExtensions.begin(), mInstanceExtensions.end() };

        // Discover optional NGX instance extensions before vkCreateInstance.
        // Unsupported installations retain the ordinary Vulkan rendering path.
        core::u32 availableCount{};
        const auto dlss{ DlssContext::GetRequiredInstanceExtensions() };
        MKT_VK_CHECK( vkEnumerateInstanceExtensionProperties( nullptr, &availableCount, nullptr ) );

        eastl::vector<VkExtensionProperties> available( availableCount );
        MKT_VK_CHECK( vkEnumerateInstanceExtensionProperties( nullptr, &availableCount, available.data() ) );

        mDlssExtensionsReady = dlss.mAvailable && std::ranges::all_of( dlss.mNames, [&]( const auto &name ) {
            return std::ranges::any_of( available, [&]( const auto &extension ) {
                return name == extension.extensionName;
            } );
        } );

        if ( mDlssExtensionsReady ) {
            for ( const auto &extension: dlss.mNames ) {
                extensionsSet.emplace( extension.c_str() );
            }
        }

        if ( builder.mQueryWindowExtensions ) {
            auto extensions{ platform::GetWindowVulkanExtensions() };
            extensionsSet.insert( extensions.begin(), extensions.end() );
        }

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{ initializers::DebugUtilsMessengerCreateInfoEXT() };

        mEnableValidationLayers = builder.mEnableValidationLayers && CheckValidationLayerSupport( mValidationLayers );

        if ( builder.mEnableValidationLayers ) {
            if ( mEnableValidationLayers ) {
                extensionsSet.emplace( VK_EXT_DEBUG_UTILS_EXTENSION_NAME );

                if ( builder.mValidationLevel == InstanceBuilder::ValidationLevel::eGpuAssisted ) {
                    extensionsSet.emplace( VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME );
                }
            }
            else {
                MKT_CORE_LOGGER_WARN( "Requested validation layers are not available!" );
            }

            SetupDebugMessengerCreateInfo( debugCreateInfo );
        }

        mInstanceExtensions.clear();

        for ( const auto& extension: extensionsSet ) {
            mInstanceExtensions.emplace_back( extension.c_str() );
        }

        // Prepare structures for Core/GPU assisted validation layers
        // https://docs.vulkan.org/samples/latest/samples/extensions/layer_settings/README.html#_vk_ext_layer_settings_configure_validation_layers_programmatically
        eastl::array<VkValidationFeatureEnableEXT, 4> enabledFeatures{
            VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
            VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
            VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT,
            VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT
        };

        VkValidationFeaturesEXT validationFeatures{
            .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
            .pNext = MKT_ADDRESSOF( debugCreateInfo ),
            .enabledValidationFeatureCount = core::as<core::u32>( enabledFeatures.size() ),
            .pEnabledValidationFeatures = enabledFeatures.data(),
            .disabledValidationFeatureCount = 0,
            .pDisabledValidationFeatures = nullptr
        };

        // GPU instrumentation supplements core validation. Keep startup diagnostics in the chain.
        VkInstanceCreateInfo createInfo{ initializers::InstanceCreateInfo() };
        createInfo.pNext = nullptr;
        createInfo.pApplicationInfo = MKT_ADDRESSOF( appInfo );

        if ( mEnableValidationLayers ) {
            createInfo.pNext = MKT_ADDRESSOF( debugCreateInfo );

            if ( builder.mValidationLevel == InstanceBuilder::ValidationLevel::eGpuAssisted ) {
                createInfo.pNext = MKT_ADDRESSOF( validationFeatures );
            }

            createInfo.enabledLayerCount = core::as<core::u32>( mValidationLayers.size() );
            createInfo.ppEnabledLayerNames = mValidationLayers.data();
        }
        else {
            createInfo.enabledLayerCount = 0;
            createInfo.ppEnabledLayerNames = nullptr;
        }

        createInfo.enabledExtensionCount = core::as<core::u32>( mInstanceExtensions.size() );
        createInfo.ppEnabledExtensionNames = mInstanceExtensions.data();

        MKT_VK_CHECK( vkCreateInstance( MKT_ADDRESSOF( createInfo ), nullptr, MKT_ADDRESSOF( mInstance ) ) );

        // Load extensions
        volkLoadInstance( mInstance );

        // Initialize the surface
        if ( builder.mWindow != nullptr ) {
            // This logic is run if we pass a window, otherwise we do not create a surface
            MKT_VK_CHECK( platform::CreateWindowVulkanSurface( *builder.mWindow, mInstance, &mSurface ) );
        }

        QueryAvailablePhysicalDevices();

        if ( mEnableValidationLayers ) {
            MKT_VK_CHECK( CreateDebugUtilsMessengerEXT( mInstance, MKT_ADDRESSOF( debugCreateInfo ), nullptr, MKT_ADDRESSOF( mDebugMessenger ) ) );
        }
    }

    auto PhysicalDevice::GetFamilyIndexWithSupport( renderer::rhi::QueueOpSupportFlags ops ) const -> core::i32 {
        MKT_BEGIN_PROFILER();
        const auto it{ std::ranges::find_if( mQueueInfos, [ops]( const std::pair<const core::u32, VulkanQueueData>& data ) {
            return data.second.mOpSupportFlags.Has( ops );
        } ) };

        if ( it != mQueueInfos.end() ) {
            return it->second.FamilyIndex;
        }

        return kInvalidQueueFamilyIndex;
    }

    auto PhysicalDevice::GetQueueWithSupport( renderer::rhi::QueueOpSupportFlags ops ) const -> const VulkanQueueData* {
        MKT_BEGIN_PROFILER();
        const auto it{ std::ranges::find_if( mQueueInfos, [ops]( const std::pair<const core::u32, VulkanQueueData>& data ) {
            return data.second.mOpSupportFlags.Has( ops );
        } ) };

        if ( it != mQueueInfos.end() ) {
            return MKT_ADDRESSOF( it->second );
        }

        return nullptr;
    }

    auto PhysicalDevice::HasQueueSupport( renderer::rhi::QueueOpSupportFlags ops ) const -> bool {
        MKT_BEGIN_PROFILER();
        // Different queue families may supply different roles, but every requested role is required.
        QueueOpSupportFlags supported{};

        for ( const auto& [familyIndex, queue]: mQueueInfos ) {
            supported |= queue.mOpSupportFlags;
        }

        return ( supported & ops ) == ops;
    }

    auto PhysicalDevice::IsExtensionAvailable( eastl::string_view name ) const -> bool {
        MKT_BEGIN_PROFILER();
        return mAvailableExtensions.contains( eastl::string{ name } );
    }

    auto PhysicalDevice::GetSupportedFeatures() const -> GpuFeatureSupportFlags {
        MKT_BEGIN_PROFILER();
        GpuFeatureSupportFlags supported{ GpuFeatureSupportFlagsBits::None };

        // Core optional features do not depend on what a logical device requests.
        if ( mFeatures.samplerAnisotropy ) {
            supported |= GpuFeatureSupportFlagsBits::AnisotropicFiltering;
        }

        if ( mFeatures.fillModeNonSolid ) {
            supported |= GpuFeatureSupportFlagsBits::HardwareWireframe;
        }

        if ( mFeatures.textureCompressionBC ) {
            supported |= GpuFeatureSupportFlagsBits::BCTextureCompression;
        }

        // These are the descriptor-indexing features used by Mikoto's bindless tables.
        if ( mFeatures12.descriptorIndexing && mFeatures12.runtimeDescriptorArray &&
             mFeatures12.shaderSampledImageArrayNonUniformIndexing &&
             mFeatures12.descriptorBindingUniformBufferUpdateAfterBind &&
             mFeatures12.descriptorBindingSampledImageUpdateAfterBind &&
             mFeatures12.descriptorBindingStorageBufferUpdateAfterBind &&
             mFeatures12.descriptorBindingStorageImageUpdateAfterBind &&
             mFeatures12.descriptorBindingPartiallyBound && mFeatures12.descriptorBindingVariableDescriptorCount ) {
            supported |= GpuFeatureSupportFlagsBits::BindlessDescriptorTables;
        }

        // Vulkan 1.3 provides buffer addresses in core; the three KHR extensions
        // supply acceleration structures, ray-tracing pipelines and their host dependency.
        if ( IsExtensionAvailable( VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME ) &&
             IsExtensionAvailable( VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME ) &&
             IsExtensionAvailable( VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME ) &&
             mFeatures12.bufferDeviceAddress && mAccelerationFeatures.accelerationStructure &&
             mAccelerationFeatures.descriptorBindingAccelerationStructureUpdateAfterBind &&
             mRayTracingFeatures.rayTracingPipeline ) {
            supported |= GpuFeatureSupportFlagsBits::HardwareRayTracing;
        }

        if ( IsExtensionAvailable( VK_EXT_MESH_SHADER_EXTENSION_NAME ) && mMeshFeatures.meshShader ) {
            supported |= GpuFeatureSupportFlagsBits::MeshShaders;
        }

        // Presentation is specific to the surface queried by this instance.
        // Without a surface it remains unverified, even if VK_KHR_swapchain exists.
        if ( IsExtensionAvailable( VK_KHR_SWAPCHAIN_EXTENSION_NAME ) &&
             HasQueueSupport( QueueOpSupportFlagsBits::Presentation ) &&
             !mFormats.empty() && !mPresentModes.empty() ) {
            supported |= GpuFeatureSupportFlagsBits::WindowPresentation;
        }

        if (IsExtensionAvailable( VK_KHR_VIDEO_QUEUE_EXTENSION_NAME ) &&
            IsExtensionAvailable( VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME ) ) {
            supported |= GpuFeatureSupportFlagsBits::VideoDecoding;
        }

        if (IsExtensionAvailable( VK_KHR_VIDEO_QUEUE_EXTENSION_NAME ) &&
            (IsExtensionAvailable( VK_KHR_VIDEO_ENCODE_AV1_EXTENSION_NAME ) ||
            IsExtensionAvailable( VK_KHR_VIDEO_ENCODE_H264_EXTENSION_NAME ) ||
            IsExtensionAvailable( VK_KHR_VIDEO_DECODE_H265_EXTENSION_NAME ) ) ) {
            supported |= GpuFeatureSupportFlagsBits::VideoEncoding;
        }

        return supported;
    }

    auto PhysicalDevice::IsExtensionListAvailable( eastl::span<eastl::string> extensions ) const -> bool {
        MKT_BEGIN_PROFILER();
        return std::ranges::all_of( extensions.begin(), extensions.end(),
            [this]( auto const& name ) {
            return IsExtensionAvailable( name );
        } );
    }

    auto Instance::QueryAvailablePhysicalDevices() -> void {
        MKT_BEGIN_PROFILER();

        core::u32 deviceCount{};
        vkEnumeratePhysicalDevices( mInstance, MKT_ADDRESSOF( deviceCount ), nullptr );

        eastl::vector<VkPhysicalDevice> devices( deviceCount );
        vkEnumeratePhysicalDevices( mInstance, MKT_ADDRESSOF( deviceCount ), devices.data() );

        mPhysicalDevices.reserve( deviceCount );

        for ( VkPhysicalDevice& vkPhysicalDevice: devices ) {
            auto& physicalDevice{ mPhysicalDevices.emplace_back() };

            // Physical device
            physicalDevice.mPhysicalDevice = vkPhysicalDevice;

            // Extensions
            core::u32 extensionCount{};
            vkEnumerateDeviceExtensionProperties( vkPhysicalDevice, nullptr, MKT_ADDRESSOF( extensionCount ), nullptr );
            eastl::vector<VkExtensionProperties> extensions( extensionCount );

            vkEnumerateDeviceExtensionProperties( vkPhysicalDevice, nullptr, MKT_ADDRESSOF( extensionCount ), extensions.data() );
            std::ranges::for_each( extensions, [&]( const VkExtensionProperties& extension ) mutable {
                physicalDevice.mAvailableExtensions.emplace( extension.extensionName );
            } );

            // Query core and extension features once, alongside the cached extension list.
            // Querying support does not enable anything on a logical device.
            physicalDevice.mFeatures2.pNext = MKT_ADDRESSOF( physicalDevice.mFeatures11 );
            physicalDevice.mFeatures11.pNext = MKT_ADDRESSOF( physicalDevice.mFeatures12 );
            physicalDevice.mFeatures12.pNext = MKT_ADDRESSOF( physicalDevice.mFeatures13 );

            if ( physicalDevice.IsExtensionAvailable( VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME ) ) {
                physicalDevice.mAccelerationFeatures.pNext = physicalDevice.mFeatures13.pNext;
                physicalDevice.mFeatures13.pNext = MKT_ADDRESSOF( physicalDevice.mAccelerationFeatures );
            }

            if ( physicalDevice.IsExtensionAvailable( VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME ) ) {
                physicalDevice.mRayTracingFeatures.pNext = physicalDevice.mFeatures13.pNext;
                physicalDevice.mFeatures13.pNext = MKT_ADDRESSOF( physicalDevice.mRayTracingFeatures );
            }

            if ( physicalDevice.IsExtensionAvailable( VK_EXT_MESH_SHADER_EXTENSION_NAME ) ) {
                physicalDevice.mMeshFeatures.pNext = physicalDevice.mFeatures13.pNext;
                physicalDevice.mFeatures13.pNext = MKT_ADDRESSOF( physicalDevice.mMeshFeatures );
            }

            vkGetPhysicalDeviceFeatures2( vkPhysicalDevice, MKT_ADDRESSOF( physicalDevice.mFeatures2 ) );
            physicalDevice.mFeatures = physicalDevice.mFeatures2.features;

            // Do not retain self-referencing pointers when PhysicalDevice is copied or moved.
            physicalDevice.mFeatures2.pNext = nullptr;
            physicalDevice.mFeatures11.pNext = nullptr;
            physicalDevice.mFeatures12.pNext = nullptr;
            physicalDevice.mFeatures13.pNext = nullptr;
            physicalDevice.mAccelerationFeatures.pNext = nullptr;
            physicalDevice.mRayTracingFeatures.pNext = nullptr;
            physicalDevice.mMeshFeatures.pNext = nullptr;

            // Adapter identity, limits and memory heaps are also independent of requests.
            vkGetPhysicalDeviceProperties( vkPhysicalDevice, MKT_ADDRESSOF( physicalDevice.mProperties ) );
            vkGetPhysicalDeviceMemoryProperties( vkPhysicalDevice, MKT_ADDRESSOF( physicalDevice.mMemoryProperties ) );

            // Queues info
            core::u32 queueFamilyCount{};
            vkGetPhysicalDeviceQueueFamilyProperties( vkPhysicalDevice, MKT_ADDRESSOF( queueFamilyCount ), nullptr );

            eastl::vector<VkQueueFamilyProperties> queueFamilies( queueFamilyCount );
            vkGetPhysicalDeviceQueueFamilyProperties( vkPhysicalDevice, std::addressof( queueFamilyCount ), queueFamilies.data() );

            eastl::vector<VkQueueFamilyVideoPropertiesKHR> videoFamilies( queueFamilyCount,
                initializers::QueueFamilyVideoPropertiesKHR() );
            eastl::vector<VkQueueFamilyProperties2> queueDetails( queueFamilyCount,
                initializers::QueueFamilyProperties2() );

            if ( physicalDevice.IsExtensionAvailable( VK_KHR_VIDEO_QUEUE_EXTENSION_NAME ) ) {
                for ( core::u32 index{}; index < queueFamilyCount; ++index ) {
                    queueDetails[index].pNext = &videoFamilies[index];
                }

                vkGetPhysicalDeviceQueueFamilyProperties2( vkPhysicalDevice, &queueFamilyCount, queueDetails.data() );
            }

            // Vulkan does not give the family index,
            // so I need to manually keep track of it
            // Each family can have multiple queues
            for ( core::u32 queueFamilyIndex{}; queueFamilyIndex < core::as<core::u32>( queueFamilies.size() ); queueFamilyIndex++ ) {
                VkQueueFamilyProperties& properties{ queueFamilies[queueFamilyIndex] };

                if ( properties.queueCount == 0 ) {
                    continue;
                }

                VulkanQueueData& queueData{ physicalDevice.mQueueInfos[queueFamilyIndex] };

                // Fill info
                queueData.FamilyIndex = queueFamilyIndex;
                queueData.mProperties = properties;

                if ( ( properties.queueFlags & VK_QUEUE_VIDEO_DECODE_BIT_KHR ) &&
                     ( videoFamilies[queueFamilyIndex].videoCodecOperations & VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR ) ) {
                    queueData.mOpSupportFlags |= rhi::QueueOpSupportFlagsBits::VideoDecode;
                }

                // Check if the queue supports graphics operations
                if ( properties.queueFlags & VK_QUEUE_GRAPHICS_BIT ) {
                    queueData.mOpSupportFlags |= renderer::rhi::QueueOpSupportFlagsBits::Graphics;
                }

                // Check if the queue supports transfer operations
                if ( properties.queueFlags & VK_QUEUE_TRANSFER_BIT ) {
                    queueData.mOpSupportFlags |= renderer::rhi::QueueOpSupportFlagsBits::Transfer;
                }

                // Check if the queue supports compute operations
                if ( properties.queueFlags & VK_QUEUE_COMPUTE_BIT ) {
                    queueData.mOpSupportFlags |= renderer::rhi::QueueOpSupportFlagsBits::Compute;
                }

                if ( mSurface != VK_NULL_HANDLE ) {
                    // Support for presentation requested
                    VkBool32 presentSupport{ VK_FALSE };
                    MKT_VK_CHECK( vkGetPhysicalDeviceSurfaceSupportKHR( vkPhysicalDevice, queueFamilyIndex, mSurface, MKT_ADDRESSOF( presentSupport ) ) );

                    if ( presentSupport == VK_TRUE ) {
                        queueData.mOpSupportFlags |= renderer::rhi::QueueOpSupportFlagsBits::Presentation;
                    }
                }
            }

            // Load SwapChain capabilities
            if ( mSurface != VK_NULL_HANDLE ) {
                core::u32 formatCount{};
                vkGetPhysicalDeviceSurfaceFormatsKHR( vkPhysicalDevice, mSurface, MKT_ADDRESSOF( formatCount ), nullptr );

                if ( formatCount != 0 ) {
                    physicalDevice.mFormats.resize( formatCount );
                    vkGetPhysicalDeviceSurfaceFormatsKHR( vkPhysicalDevice, mSurface, MKT_ADDRESSOF( formatCount ), physicalDevice.mFormats.data() );
                }

                core::u32 presentModeCount{};
                vkGetPhysicalDeviceSurfacePresentModesKHR( vkPhysicalDevice, mSurface, MKT_ADDRESSOF( presentModeCount ), nullptr );

                if ( presentModeCount != 0 ) {
                    physicalDevice.mPresentModes.resize( presentModeCount );
                    vkGetPhysicalDeviceSurfacePresentModesKHR( vkPhysicalDevice, mSurface, MKT_ADDRESSOF( presentModeCount ), physicalDevice.mPresentModes.data() );
                }
            }
        }
    }

    Instance::~Instance() {
        MKT_BEGIN_PROFILER();
        // Surface
        if ( mSurface != VK_NULL_HANDLE ) {
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

    auto InstanceBuilder::SetAppName( eastl::string_view name ) -> InstanceBuilder& {
        MKT_BEGIN_PROFILER();
        mAppName = name;
        return *this;
    }

    auto InstanceBuilder::SetEngineName( eastl::string_view name ) -> InstanceBuilder& {
        MKT_BEGIN_PROFILER();
        mEngineName = name;
        return *this;
    }

    auto InstanceBuilder::SetAppVersion( core::u32 major, core::u32 minor, core::u32 patch ) -> InstanceBuilder& {
        MKT_BEGIN_PROFILER();
        mAppVersion = VK_MAKE_API_VERSION( 0, major, minor, patch );
        return *this;
    }

    auto InstanceBuilder::SetApiVersion( core::u32 major, core::u32 minor, core::u32 patch ) -> InstanceBuilder& {
        MKT_BEGIN_PROFILER();
        mApiVersion = VK_MAKE_API_VERSION( 0, major, minor, patch );
        return *this;
    }

    auto InstanceBuilder::SetEngineVersion( core::u32 major, core::u32 minor, core::u32 patch ) -> InstanceBuilder& {
        MKT_BEGIN_PROFILER();
        mEngineVersion = VK_MAKE_API_VERSION( 0, major, minor, patch );
        return *this;
    }

    auto InstanceBuilder::SetValidationLevel( ValidationLevel level ) -> InstanceBuilder& {
        MKT_BEGIN_PROFILER();
        mValidationLevel = level;
        return *this;
    }

    auto InstanceBuilder::EnableValidationLayers( bool value ) -> InstanceBuilder& {
        MKT_BEGIN_PROFILER();
        mEnableValidationLayers = value;
        return *this;
    }

    auto InstanceBuilder::QueryWindowExtensions( bool value ) -> InstanceBuilder& {
        MKT_BEGIN_PROFILER();
        mQueryWindowExtensions = value;
        return *this;
    }

    auto InstanceBuilder::QuerySurfaceSupport( platform::IWindow* window ) -> InstanceBuilder& {
        MKT_BEGIN_PROFILER();
        mWindow = window;
        return *this;
    }

    auto InstanceBuilder::Build() -> eastl::unique_ptr<Instance> {
        MKT_BEGIN_PROFILER();
        return eastl::make_unique<Instance>( *this );
    }
}// namespace mikoto