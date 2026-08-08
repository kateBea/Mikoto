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

#ifndef MIKOTO_INSTANCE_HH
#define MIKOTO_INSTANCE_HH

#include <volk.h>
#include <vk_mem_alloc.h>

#include <EASTL/memory.h>
#include <EASTL/span.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/vector.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>

#include <Platform/MainWindow.hh>

#include <Renderer/Rhi/GpuDevice.hh>

namespace mikoto::renderer::vulkan {

    struct VulkanQueueData {
        core::u32 FamilyIndex{};
        rhi::QueueOpSupportFlags mOpSupportFlags{};
        VkQueueFamilyProperties mProperties{};
    };

    struct PhysicalDevice {
        static constexpr core::i32 kInvalidQueueFamilyIndex{ -1 };

        VkPhysicalDevice mPhysicalDevice{};

        // Swapchain capabilities
        VkSurfaceCapabilitiesKHR mCapabilities{};
        eastl::vector<VkSurfaceFormatKHR> mFormats{};
        eastl::vector<VkPresentModeKHR> mPresentModes{};

        // Properties
        VkPhysicalDeviceFeatures mFeatures{};
        VkPhysicalDeviceProperties mProperties{};
        VkPhysicalDeviceMemoryProperties mMemoryProperties{};
        VkPhysicalDeviceFeatures2 mFeatures2{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };

        ankerl::unordered_dense::set<eastl::string> mAvailableExtensions{};
        ankerl::unordered_dense::map<core::u32, VulkanQueueData> mQueueInfos{};

        MKT_NODISCARD auto GetFamilyIndexWithSupport( rhi::QueueOpSupportFlags ops ) const -> core::i32;
        MKT_NODISCARD auto GetQueueWithSupport( rhi::QueueOpSupportFlags ops ) const -> const VulkanQueueData*;
        MKT_NODISCARD auto HasQueueSupport( rhi::QueueOpSupportFlags ops ) const -> bool;
        MKT_NODISCARD auto IsExtensionAvailable( eastl::string_view name ) const -> bool;
        MKT_NODISCARD auto IsExtensionListAvailable( eastl::span<eastl::string> extensions ) const -> bool;
    };

    struct InstanceBuilder;

    struct Instance final {
        VkInstance mInstance{};
        VkSurfaceKHR mSurface{};

        core::u32 mApiVer{};

        VkDebugUtilsMessengerEXT mDebugMessenger{};
        VmaVulkanFunctions mVMAFunctions{};

#if defined( NDEBUG )
        bool mEnableValidationLayers{ false };
#else
        bool mEnableValidationLayers{ true };
#endif

        eastl::string mName{};
        eastl::string mAppName{};
        eastl::string mEngineName{};

        eastl::vector<PhysicalDevice> mPhysicalDevices{};

        eastl::vector<const char*> mValidationLayers{
            "VK_LAYER_KHRONOS_validation",
            //"VK_LAYER_LUNARG_crash_diagnostic"
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
        platform::Window* mWindow{};

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

        bool mQueryGlfwExtensions{ false };

        auto SetAppName( eastl::string_view name ) -> InstanceBuilder&;
        auto SetEngineName( eastl::string_view name ) -> InstanceBuilder&;

        auto SetAppVersion( core::u32 major, core::u32 minor, core::u32 patch ) -> InstanceBuilder&;
        auto SetApiVersion( core::u32 major, core::u32 minor, core::u32 patch ) -> InstanceBuilder&;
        auto SetEngineVersion( core::u32 major, core::u32 minor, core::u32 patch ) -> InstanceBuilder&;

        auto SetValidationLevel( ValidationLevel level ) -> InstanceBuilder&;

        auto EnableValidationLayers( bool value ) -> InstanceBuilder&;
        auto QueryGLFWExtensions( bool value ) -> InstanceBuilder&;
        auto QuerySurfaceSupport( platform::Window* window ) -> InstanceBuilder&;

        auto Build() -> eastl::unique_ptr<Instance>;
    };

}// namespace mikoto

#endif//MIKOTO_INSTANCE_HH
