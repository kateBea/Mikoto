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

#ifndef MIKOTO_RENDERER_RHI_VULKAN_VULKAN_INSTANCE_HH
#define MIKOTO_RENDERER_RHI_VULKAN_VULKAN_INSTANCE_HH

#include <EASTL/span.h>
#include <EASTL/memory.h>
#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/string_view.h>

#include <ankerl/unordered_dense.h>

#include <vk_mem_alloc.h>
#include <volk.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Platform/MainWindow.hh>

namespace mikoto::renderer::vulkan {

    struct VulkanQueueData {
        core::u32 FamilyIndex{};
        rhi::QueueOpSupportFlags mOpSupportFlags{};
        VkQueueFamilyProperties mProperties{};
    };

    struct PhysicalDevice {
        static constexpr core::i32 kInvalidQueueFamilyIndex{ -1 };

        VkPhysicalDevice mPhysicalDevice{};

        // SwapChain capabilities
        VkSurfaceCapabilitiesKHR mCapabilities{};
        eastl::vector<VkSurfaceFormatKHR> mFormats{};
        eastl::vector<VkPresentModeKHR> mPresentModes{};

        // Properties
        VkPhysicalDeviceFeatures mFeatures{};
        VkPhysicalDeviceProperties mProperties{};
        VkPhysicalDeviceMemoryProperties mMemoryProperties{};
        VkPhysicalDeviceFeatures2 mFeatures2{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };

        // Cached support, queried independently of logical-device requests.
        VkPhysicalDeviceVulkan11Features mFeatures11{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
        VkPhysicalDeviceVulkan12Features mFeatures12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
        VkPhysicalDeviceVulkan13Features mFeatures13{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };

        VkPhysicalDeviceAccelerationStructureFeaturesKHR mAccelerationFeatures{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
        VkPhysicalDeviceRayTracingPipelineFeaturesKHR mRayTracingFeatures{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };
        VkPhysicalDeviceMeshShaderFeaturesEXT mMeshFeatures{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT };

        ankerl::unordered_dense::set<eastl::string> mAvailableExtensions{};
        ankerl::unordered_dense::map<core::u32, VulkanQueueData> mQueueInfos{};

        MKT_NODISCARD auto HasQueueSupport( rhi::QueueOpSupportFlags ops ) const -> bool;
        MKT_NODISCARD auto GetSupportedFeatures() const -> rhi::GpuFeatureSupportFlags;
        MKT_NODISCARD auto IsExtensionAvailable( eastl::string_view name ) const -> bool;
        MKT_NODISCARD auto GetFamilyIndexWithSupport( rhi::QueueOpSupportFlags ops ) const -> core::i32;
        MKT_NODISCARD auto IsExtensionListAvailable( eastl::span<eastl::string> extensions ) const -> bool;
        MKT_NODISCARD auto GetQueueWithSupport( rhi::QueueOpSupportFlags ops ) const -> const VulkanQueueData*;
    };

    struct InstanceBuilder;

    struct Instance final {
        VkInstance mInstance{};
        VkSurfaceKHR mSurface{};

        core::u32 mApiVer{};

        VkDebugUtilsMessengerEXT mDebugMessenger{};
        VmaVulkanFunctions mVMAFunctions{};

        bool mEnableValidationLayers{ false };

        eastl::string mName{};
        eastl::string mAppName{};
        eastl::string mEngineName{};

        eastl::vector<PhysicalDevice> mPhysicalDevices{};
        bool mDlssExtensionsReady{};

        eastl::vector<const char*> mValidationLayers{
            "VK_LAYER_KHRONOS_validation",
        };

        eastl::vector<const char*> mInstanceExtensions{
            VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME
        };

        // Initializes structures
        explicit Instance(  InstanceBuilder& builder );

        auto QueryAvailablePhysicalDevices() -> void;

        // Cleanup
        ~Instance();
    };

    struct InstanceBuilder {
        platform::IWindow* mWindow{};

        enum class ValidationLevel {
            eCore,
            eGpuAssisted,
        } mValidationLevel{ ValidationLevel::eCore };

        eastl::string mAppName{};
        eastl::string mEngineName{};

        core::u32 mAppVersion{};
        core::u32 mApiVersion{};
        core::u32 mEngineVersion{};

        bool mEnableValidationLayers{ false };

        bool mQueryWindowExtensions{ false };

        auto SetAppName( eastl::string_view name ) -> InstanceBuilder&;
        auto SetEngineName( eastl::string_view name ) -> InstanceBuilder&;

        auto SetAppVersion( core::u32 major, core::u32 minor, core::u32 patch ) -> InstanceBuilder&;
        auto SetApiVersion( core::u32 major, core::u32 minor, core::u32 patch ) -> InstanceBuilder&;
        auto SetEngineVersion( core::u32 major, core::u32 minor, core::u32 patch ) -> InstanceBuilder&;

        auto SetValidationLevel( ValidationLevel level ) -> InstanceBuilder&;

        auto EnableValidationLayers( bool value ) -> InstanceBuilder&;
        auto QueryWindowExtensions( bool value ) -> InstanceBuilder&;
        auto QuerySurfaceSupport( platform::IWindow* window ) -> InstanceBuilder&;

        auto Build() -> eastl::unique_ptr<Instance>;
    };

}// namespace mikoto

#endif// MIKOTO_RENDERER_RHI_VULKAN_VULKAN_INSTANCE_HH

