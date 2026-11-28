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

#ifndef MIKOTO_RENDERER_RHI_VULKAN_VULKAN_DLSS_HH
#define MIKOTO_RENDERER_RHI_VULKAN_VULKAN_DLSS_HH

#include <memory>
#include <string>
#include <vector>

#include <volk.h>

#include <Core/Core.hh>

struct NVSDK_NGX_Parameter;

namespace mikoto::renderer::vulkan {
    enum class DlssSupport {
        eDisabled,
        ePreparing,
        eUnavailable,
        eAvailable,
        eFailed
    };

    struct DlssCapabilities {
        DlssSupport mSupport{ DlssSupport::eDisabled };
        std::string mMessage{};
        unsigned int mMinimumDriverMajor{};
        unsigned int mMinimumDriverMinor{};
    };

    struct DlssExtensions {
        bool mAvailable{};
        std::vector<std::string> mNames{};
        std::string mError{};
    };

    // One NGX context per logical device, shared by all scene and game views.
    // Extension discovery precedes Vulkan creation; NGX startup runs on a worker.
    class DlssContext final {
    public:
        DlssContext();
        ~DlssContext();

        auto Initialize( VkInstance instance, VkPhysicalDevice physical, VkDevice device, bool extensionsReady ) -> void;
        auto Shutdown() -> void;

        // Polls without waiting. Availability describes SDK support, not a live RR feature.
        MKT_NODISCARD auto GetCapabilities() -> DlssCapabilities;
        MKT_NODISCARD auto GetParameters() -> NVSDK_NGX_Parameter *;

        MKT_NODISCARD static auto GetRequiredInstanceExtensions() -> DlssExtensions;
        MKT_NODISCARD static auto GetRequiredDeviceExtensions( VkInstance instance, VkPhysicalDevice physical ) -> DlssExtensions;

    private:
        struct State;
        std::unique_ptr<State> mState{};
    };
}

#endif// MIKOTO_RENDERER_RHI_VULKAN_VULKAN_DLSS_HH
