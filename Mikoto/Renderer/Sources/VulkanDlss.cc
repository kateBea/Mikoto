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

// NGX discovery and initialization adapted from nvpro-samples/vk_gltf_renderer:
// https://github.com/nvpro-samples/vk_gltf_renderer/blob/8c0ef8357b19acd05f5ae988a6c9cb912f39909b/src/dlss_wrapper.cpp

#include <array>
#include <chrono>
#include <exception>
#include <filesystem>
#include <future>
#include <mutex>

#include <Renderer/Rhi/Vulkan/VulkanDlss.hh>

#if MIKOTO_ENABLE_DLSS
#include <Windows.h>

#include <nvsdk_ngx_defs_dlssd.h>
#include <nvsdk_ngx_vk.h>
#endif

namespace mikoto::renderer::vulkan {

#if MIKOTO_ENABLE_DLSS
    static constexpr const char *kProjectId{ "9b1d438f-6325-4a6c-bb57-ef08283f3ae1" };
    static constexpr const char *kEngineVersion{ "1.0" };
    static constexpr unsigned int kNvidiaVendorId{ 0x10de };
    static constexpr std::size_t kMaximumWindowsPath{ 32768 };

    // NGX has process-global state. Serialize discovery, startup and shutdown;
    // views share the device's context instead of initializing their own copies.
    MKT_NODISCARD static auto GetNgxMutex() -> std::mutex & {
        static std::mutex mutex{};
        return mutex;
    }

    MKT_NODISCARD static auto GetDiscoveryInfo( NVSDK_NGX_FeatureCommonInfo &common ) -> NVSDK_NGX_FeatureDiscoveryInfo {
        NVSDK_NGX_FeatureDiscoveryInfo info{};
        info.SDKVersion = NVSDK_NGX_Version_API;
        info.FeatureID = NVSDK_NGX_Feature_RayReconstruction;
        info.Identifier.IdentifierType = NVSDK_NGX_Application_Identifier_Type_Project_Id;
        info.Identifier.v.ProjectDesc.ProjectId = kProjectId;
        info.Identifier.v.ProjectDesc.EngineType = NVSDK_NGX_ENGINE_TYPE_CUSTOM;
        info.Identifier.v.ProjectDesc.EngineVersion = kEngineVersion;
        info.ApplicationDataPath = L".";
        info.FeatureInfo = &common;
        return info;
    }

    MKT_NODISCARD static auto GetNgxError( NVSDK_NGX_Result result ) -> std::string {
        return "NGX result " + std::to_string( static_cast<unsigned int>( result ) );
    }

    MKT_NODISCARD static auto CopyExtensions( NVSDK_NGX_Result result, unsigned int count, const VkExtensionProperties *properties ) -> DlssExtensions {
        DlssExtensions extensions{};
        extensions.mAvailable = NVSDK_NGX_SUCCEED( result );
        if ( !extensions.mAvailable ) {
            extensions.mError = GetNgxError( result );
            return extensions;
        }
        for ( unsigned int index{}; index < count; ++index ) {
            extensions.mNames.emplace_back( properties[index].extensionName );
        }
        return extensions;
    }
#endif

    struct DlssContext::State {
        std::mutex mMutex{};
        DlssCapabilities mCapabilities{};
#if MIKOTO_ENABLE_DLSS
        struct NativeContext {
            VkDevice mDevice{};
            NVSDK_NGX_Parameter *mParameters{};
            bool mInitialized{};
            DlssCapabilities mCapabilities{};

            ~NativeContext();

            auto Initialize( VkInstance instance, VkPhysicalDevice physical ) -> void;
        };

        std::unique_ptr<NativeContext> mNative{};
        std::future<std::unique_ptr<NativeContext>> mInitialization{};
#endif
    };

#if MIKOTO_ENABLE_DLSS
    DlssContext::State::NativeContext::~NativeContext() {
        const std::lock_guard lock{ GetNgxMutex() };
        if ( mParameters ) {
            ( void )NVSDK_NGX_VULKAN_DestroyParameters( mParameters );
        }
        if ( mInitialized ) {
            ( void )NVSDK_NGX_VULKAN_Shutdown1( mDevice );
        }
    }

    auto DlssContext::State::NativeContext::Initialize( VkInstance instance, VkPhysicalDevice physical ) -> void {
        // Absolute search paths work even when the launcher uses another cwd.
        std::array<wchar_t, kMaximumWindowsPath> executable{};
        const auto length{ GetModuleFileNameW( nullptr, executable.data(), static_cast<DWORD>( executable.size() ) ) };
        const auto directory{ std::filesystem::path{ std::wstring{ executable.data(), length } }.parent_path().wstring() };
        const auto cache{ std::filesystem::temp_directory_path() / L"Mikoto" / L"NGX" };
        std::filesystem::create_directories( cache );
        const wchar_t *paths[]{ directory.c_str() };
        NVSDK_NGX_FeatureCommonInfo common{};
        common.PathListInfo.Path = paths;
        common.PathListInfo.Length = 1;

        const std::lock_guard lock{ GetNgxMutex() };
        auto result{ NVSDK_NGX_VULKAN_Init_with_ProjectID( kProjectId, NVSDK_NGX_ENGINE_TYPE_CUSTOM,
            kEngineVersion, cache.c_str(), instance, physical, mDevice,
            vkGetInstanceProcAddr, vkGetDeviceProcAddr, &common ) };
        if ( NVSDK_NGX_FAILED( result ) ) {
            mCapabilities = { DlssSupport::eFailed, "DLSS startup failed: " + GetNgxError( result ) };
            return;
        }
        mInitialized = true;
        result = NVSDK_NGX_VULKAN_GetCapabilityParameters( &mParameters );
        if ( NVSDK_NGX_FAILED( result ) || !mParameters ) {
            mCapabilities = { DlssSupport::eFailed, "DLSS capability query failed: " + GetNgxError( result ) };
            return;
        }

        // Query both feature requirements and capability parameters: a
        // compatible RTX GPU can still have an old driver or missing runtime.
        auto discovery{ GetDiscoveryInfo( common ) };
        NVSDK_NGX_FeatureRequirement requirement{};
        result = NVSDK_NGX_VULKAN_GetFeatureRequirements( instance, physical, &discovery, &requirement );
        if ( NVSDK_NGX_FAILED( result ) || requirement.FeatureSupported != NVSDK_NGX_FeatureSupportResult_Supported ) {
            mCapabilities = { DlssSupport::eUnavailable, "Ray Reconstruction is unavailable on this GPU/driver: " + GetNgxError( result ) };
            return;
        }

        int available{};
        int needsUpdate{};

        ( void )mParameters->Get( NVSDK_NGX_Parameter_SuperSamplingDenoising_MinDriverVersionMajor, &mCapabilities.mMinimumDriverMajor );
        ( void )mParameters->Get( NVSDK_NGX_Parameter_SuperSamplingDenoising_MinDriverVersionMinor, &mCapabilities.mMinimumDriverMinor );
        ( void )mParameters->Get( NVSDK_NGX_Parameter_SuperSamplingDenoising_NeedsUpdatedDriver, &needsUpdate );
        
        result = mParameters->Get( NVSDK_NGX_Parameter_SuperSamplingDenoising_Available, &available );
        if ( needsUpdate ) {
            mCapabilities.mSupport = DlssSupport::eUnavailable;
            mCapabilities.mMessage = "Ray Reconstruction requires a newer NVIDIA driver.";
        }
        else if ( NVSDK_NGX_FAILED( result ) || !available ) {
            mCapabilities.mSupport = DlssSupport::eUnavailable;
            mCapabilities.mMessage = "Ray Reconstruction is unavailable on this GPU/driver.";
        }
        else {
            mCapabilities.mSupport = DlssSupport::eAvailable;
            mCapabilities.mMessage = "DLSS Ray Reconstruction is supported.";
        }
    }
#endif

    DlssContext::DlssContext()
        : mState{ std::make_unique<State>() } {
    }

    DlssContext::~DlssContext() {
        Shutdown();
    }

    auto DlssContext::Initialize( VkInstance instance, VkPhysicalDevice physical, VkDevice device, bool extensionsReady ) -> void {
        const std::lock_guard lock{ mState->mMutex };
#if MIKOTO_ENABLE_DLSS
        if ( mState->mInitialization.valid() || mState->mNative ) {
            return;
        }

        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties( physical, &properties );
        if ( !extensionsReady || properties.vendorID != kNvidiaVendorId ) {
            mState->mCapabilities = { DlssSupport::eUnavailable, "Required DLSS GPU support or Vulkan extensions are unavailable." };
            return;
        }

        mState->mCapabilities = { DlssSupport::ePreparing, "Preparing DLSS." };
        mState->mInitialization = std::async( std::launch::async, [instance, physical, device] {
            auto native{ std::make_unique<State::NativeContext>() };
            native->mDevice = device;
            native->Initialize( instance, physical );
            return native;
        } );
#endif
    }

    auto DlssContext::Shutdown() -> void {
        const std::lock_guard lock{ mState->mMutex };
#if MIKOTO_ENABLE_DLSS
        // Device shutdown joins the worker before vkDestroyDevice. Scene changes
        // and backend selection never wait on this future or reinitialize NGX.
        if ( mState->mInitialization.valid() ) {
            try {
                mState->mNative = mState->mInitialization.get();
            }
            catch ( const std::exception & ) {
            }
        }
        mState->mNative.reset();
#endif
        mState->mCapabilities = {};
    }

    auto DlssContext::GetCapabilities() -> DlssCapabilities {
        const std::lock_guard lock{ mState->mMutex };
#if MIKOTO_ENABLE_DLSS
        if ( mState->mInitialization.valid() &&
             mState->mInitialization.wait_for( std::chrono::seconds{ 0 } ) == std::future_status::ready ) {
            try {
                mState->mNative = mState->mInitialization.get();
                mState->mCapabilities = mState->mNative->mCapabilities;
            }
            catch ( const std::exception &error ) {
                mState->mCapabilities = { DlssSupport::eFailed, error.what() };
            }
        }
#endif
        return mState->mCapabilities;
    }

    auto DlssContext::GetParameters() -> NVSDK_NGX_Parameter * {
        if ( GetCapabilities().mSupport != DlssSupport::eAvailable ) {
            return nullptr;
        }
#if MIKOTO_ENABLE_DLSS
        return mState->mNative->mParameters;
#else
        return nullptr;
#endif
    }

    auto DlssContext::GetRequiredInstanceExtensions() -> DlssExtensions {
#if MIKOTO_ENABLE_DLSS
        const std::lock_guard lock{ GetNgxMutex() };
        NVSDK_NGX_FeatureCommonInfo common{};
        auto info{ GetDiscoveryInfo( common ) };

        unsigned int count{};
        VkExtensionProperties *properties{};
        const auto result{ NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements( &info, &count, &properties ) };
        return CopyExtensions( result, count, properties );
#else
        return {};
#endif
    }

    auto DlssContext::GetRequiredDeviceExtensions( VkInstance instance, VkPhysicalDevice physical ) -> DlssExtensions {
#if MIKOTO_ENABLE_DLSS
        VkPhysicalDeviceProperties gpu{};
        vkGetPhysicalDeviceProperties( physical, &gpu );
        if ( gpu.vendorID != kNvidiaVendorId ) {
            return {};
        }

        const std::lock_guard lock{ GetNgxMutex() };
        NVSDK_NGX_FeatureCommonInfo common{};
        auto info{ GetDiscoveryInfo( common ) };

        unsigned int count{};
        VkExtensionProperties *properties{};
        const auto result{ NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements( instance, physical, &info, &count, &properties ) };
        return CopyExtensions( result, count, properties );
#else
        return {};
#endif
    }
}
