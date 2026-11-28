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
#include <condition_variable>
#include <deque>
#include <exception>
#include <filesystem>
#include <future>
#include <map>
#include <mutex>
#include <thread>
#include <utility>

#include <Renderer/Rhi/Vulkan/VulkanDlss.hh>

#if MIKOTO_ENABLE_DLSS
#include <Platform/Windows/Windows.hh>

#include <nvsdk_ngx_defs_dlssd.h>
#include <nvsdk_ngx_vk.h>
#include <nvsdk_ngx_helpers_vk.h>
#include <nvsdk_ngx_helpers_dlssd_vk.h>
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
        return NVSDK_NGX_FeatureDiscoveryInfo{
            .SDKVersion = NVSDK_NGX_Version_API,
            .FeatureID = NVSDK_NGX_Feature_RayReconstruction,
            .Identifier = {
                .IdentifierType = NVSDK_NGX_Application_Identifier_Type_Project_Id,
                .v = {
                    .ProjectDesc = {
                        .ProjectId = kProjectId,
                        .EngineType = NVSDK_NGX_ENGINE_TYPE_CUSTOM,
                        .EngineVersion = kEngineVersion,
                    },
                },
            },
            .ApplicationDataPath = L".",
            .FeatureInfo = &common,
        };

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
        std::map<uint64_t, DlssInputSizes> mInputSizes{};
        std::mutex mMutex{};
        DlssCapabilities mCapabilities{};

#if MIKOTO_ENABLE_DLSS
        struct NativeContext {
            VkDevice mDevice{};
            
            NVSDK_NGX_Parameter *mParameters{};
            DlssCapabilities mCapabilities{};
            
            bool mInitialized{};

            struct RetiredFeature {
                NVSDK_NGX_Handle *mHandle{};
                NVSDK_NGX_Parameter *mParameters{};
            };

            std::mutex mRetirementMutex{};
            std::condition_variable mRetirementSignal{};
            std::deque<RetiredFeature> mRetiredFeatures{};
            std::jthread mRetirementWorker{};

            auto ProcessRetirements( std::stop_token stop ) -> void;

            auto Initialize( VkInstance instance, VkPhysicalDevice physical ) -> void;
            auto RetireFeature( NVSDK_NGX_Handle *handle, NVSDK_NGX_Parameter *parameters ) -> void;

            ~NativeContext();
        };

        std::shared_ptr<NativeContext> mNative{};
        std::future<std::unique_ptr<NativeContext>> mInitialization{};
#endif
    };

#if MIKOTO_ENABLE_DLSS
    DlssContext::State::NativeContext::~NativeContext() {
        if ( mRetirementWorker.joinable() ) {
            mRetirementWorker.request_stop();
            mRetirementSignal.notify_all();
            mRetirementWorker.join();
        }

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
            mRetirementWorker = std::jthread{ [this]( std::stop_token stop ) {
                ProcessRetirements( stop );
            } };
        }
    }
    auto DlssContext::State::NativeContext::RetireFeature( NVSDK_NGX_Handle *handle, NVSDK_NGX_Parameter *parameters ) -> void {
        {
            const std::lock_guard lock{ mRetirementMutex };
            mRetiredFeatures.push_back( RetiredFeature{ .mHandle = handle, .mParameters = parameters } );
        }
        mRetirementSignal.notify_one();
    }

    auto DlssContext::State::NativeContext::ProcessRetirements( std::stop_token stop ) -> void {
        while ( true ) {
            RetiredFeature retired{};
            {
                std::unique_lock lock{ mRetirementMutex };
                mRetirementSignal.wait( lock, [&] {
                    return stop.stop_requested() || !mRetiredFeatures.empty();
                } );
                if ( mRetiredFeatures.empty() ) {
                    return;
                }
                retired = mRetiredFeatures.front();
                mRetiredFeatures.pop_front();
            }

            const std::lock_guard lock{ GetNgxMutex() };
            if ( retired.mHandle ) {
                ( void )NVSDK_NGX_VULKAN_ReleaseFeature( retired.mHandle );
            }
            
            if ( retired.mParameters ) {
                ( void )NVSDK_NGX_VULKAN_DestroyParameters( retired.mParameters );
            }
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
        mState->mInputSizes.clear();

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

    auto DlssContext::GetInputSizes( VkExtent2D output ) -> DlssInputSizes {
        DlssInputSizes sizes{
            .mOptimal = output,
            .mMinimum = output,
            .mMaximum = output,
        };
        const auto capabilities{ GetCapabilities() };
        sizes.mSupport = capabilities.mSupport;
        if ( capabilities.mSupport != DlssSupport::eAvailable ) {
            return sizes;
        }
#if MIKOTO_ENABLE_DLSS
        const auto key{ ( static_cast<uint64_t>( output.width ) << 32 ) | output.height };
        {
            const std::lock_guard lock{ mState->mMutex };
            if ( const auto cached{ mState->mInputSizes.find( key ) }; cached != mState->mInputSizes.end() ) {
                return cached->second;
            }
        }
        // A concurrent feature creation must never stall resolution selection on
        // the render/UI thread. Retry next frame while keeping the active graph.
        const std::unique_lock sdkLock{ GetNgxMutex(), std::try_to_lock };
        if ( !sdkLock.owns_lock() ) {
            sizes.mSupport = DlssSupport::ePreparing;
            return sizes;
        }
        // This SDK exposes the RR callback but not the convenience helper used
        // by newer samples. Query its RR capability callback directly; SR's
        // callback would return the wrong recommendations for this feature.
        auto *parameters{ GetParameters() };
        void *callback{};
        auto result{ parameters->Get( NVSDK_NGX_Parameter_DLSSDOptimalSettingsCallback, &callback ) };
        if ( NVSDK_NGX_SUCCEED( result ) && callback ) {
            parameters->Set( NVSDK_NGX_Parameter_Width, output.width );
            parameters->Set( NVSDK_NGX_Parameter_Height, output.height );
            parameters->Set( NVSDK_NGX_Parameter_PerfQualityValue, NVSDK_NGX_PerfQuality_Value_MaxQuality );
            const auto query{ reinterpret_cast<PFN_NVSDK_NGX_DLSS_GetOptimalSettingsCallback>( callback ) };
            result = query( parameters );
            if ( NVSDK_NGX_SUCCEED( result ) ) {
                parameters->Get( NVSDK_NGX_Parameter_OutWidth, &sizes.mOptimal.width );
                parameters->Get( NVSDK_NGX_Parameter_OutHeight, &sizes.mOptimal.height );
                sizes.mMinimum = sizes.mOptimal;
                sizes.mMaximum = sizes.mOptimal;
                parameters->Get( NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Min_Render_Width, &sizes.mMinimum.width );
                parameters->Get( NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Min_Render_Height, &sizes.mMinimum.height );
                parameters->Get( NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Max_Render_Width, &sizes.mMaximum.width );
                parameters->Get( NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Max_Render_Height, &sizes.mMaximum.height );
            }
        }
        if ( NVSDK_NGX_FAILED( result ) || !callback ) {
            sizes.mSupport = DlssSupport::eUnavailable;
        }
        const std::lock_guard lock{ mState->mMutex };
        mState->mInputSizes[key] = sizes;
#endif
        return sizes;
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
    struct DlssFeature::State {
        std::string mError{};
        DlssFeatureDescription mDescription{};
#if MIKOTO_ENABLE_DLSS
        std::shared_ptr<DlssContext::State::NativeContext> mNative{};
        NVSDK_NGX_Handle *mHandle{};
        NVSDK_NGX_Parameter *mParameters{};
#endif
    };

    DlssFeature::DlssFeature()
        : mState{ std::make_unique<State>() } {
    }

    DlssFeature::~DlssFeature() {
        Shutdown();
    }

    auto DlssFeature::Initialize( DlssContext &context, VkDevice device, VkCommandBuffer commands,
        const DlssFeatureDescription &description ) -> bool {
        Shutdown();
        mState->mError.clear();
        mState->mDescription = description;
#if MIKOTO_ENABLE_DLSS
        const auto capabilities{ context.GetCapabilities() };
        if ( capabilities.mSupport != DlssSupport::eAvailable ) {
            mState->mError = capabilities.mMessage;
            return false;
        }

        {
            const std::lock_guard lock{ context.mState->mMutex };
            mState->mNative = context.mState->mNative;
        }

        // SDK calls are not thread-safe. This expensive path belongs to the
        // graph worker; active views use try_lock and show their raw frame instead.
        const std::lock_guard lock{ GetNgxMutex() };
        auto result{ NVSDK_NGX_VULKAN_AllocateParameters( &mState->mParameters ) };
        if ( NVSDK_NGX_SUCCEED( result ) ) {
            NVSDK_NGX_DLSSD_Create_Params create{};
            create.InWidth = description.mInputSize.width;
            create.InHeight = description.mInputSize.height;
            create.InTargetWidth = description.mOutputSize.width;
            create.InTargetHeight = description.mOutputSize.height;
            create.InPerfQualityValue = NVSDK_NGX_PerfQuality_Value_MaxQuality;
            create.InDenoiseMode = NVSDK_NGX_DLSS_Denoise_Mode_DLUnified;
            create.InRoughnessMode = NVSDK_NGX_DLSS_Roughness_Mode_Packed;
            create.InUseHWDepth = NVSDK_NGX_DLSS_Depth_Type_HW;
            create.InFeatureCreateFlags = NVSDK_NGX_DLSS_Feature_Flags_IsHDR | NVSDK_NGX_DLSS_Feature_Flags_MVLowRes;
            // Preset hints are baked at creation, on this feature's own parameter
            // map. Changing one view cannot overwrite another view's model.
            const auto preset{ static_cast<NVSDK_NGX_RayReconstruction_Hint_Render_Preset>( description.mPreset ) };
            mState->mParameters->Set( NVSDK_NGX_Parameter_RayReconstruction_Hint_Render_Preset_DLAA, preset );
            mState->mParameters->Set( NVSDK_NGX_Parameter_RayReconstruction_Hint_Render_Preset_Quality, preset );
            mState->mParameters->Set( NVSDK_NGX_Parameter_RayReconstruction_Hint_Render_Preset_Balanced, preset );
            mState->mParameters->Set( NVSDK_NGX_Parameter_RayReconstruction_Hint_Render_Preset_Performance, preset );
            mState->mParameters->Set( NVSDK_NGX_Parameter_RayReconstruction_Hint_Render_Preset_UltraPerformance, preset );
            constexpr unsigned int singleGpuNodeMask{ 1 };
            result = NGX_VULKAN_CREATE_DLSSD_EXT1( device, commands, singleGpuNodeMask, singleGpuNodeMask,
                &mState->mHandle, mState->mParameters, &create );
        }
        if ( NVSDK_NGX_FAILED( result ) ) {
            mState->mError = "DLSS feature creation failed: " + GetNgxError( result );
            return false;
        }
        return true;
#else
        mState->mError = "DLSS is disabled in this build.";
        return false;
#endif
    }

    auto DlssFeature::Shutdown() -> void {
#if MIKOTO_ENABLE_DLSS
        if ( mState->mNative && ( mState->mHandle || mState->mParameters ) ) {
            // The caller retires its DeviceObject after GPU completion. NGX release
            // then runs on the context's retirement worker, never on the UI thread.
            mState->mNative->RetireFeature( mState->mHandle, mState->mParameters );
        }
        mState->mHandle = nullptr;
        mState->mParameters = nullptr;
        mState->mNative.reset();
#endif
    }

    auto DlssFeature::IsReady() const -> bool {
#if MIKOTO_ENABLE_DLSS
        return mState->mHandle != nullptr && mState->mError.empty();
#else
        return false;
#endif
    }

    auto DlssFeature::GetError() const -> const std::string & {
        return mState->mError;
    }

    auto DlssFeature::Evaluate( VkCommandBuffer commands, const DlssEvaluation &evaluation ) -> DlssEvaluationResult {
#if MIKOTO_ENABLE_DLSS
        if ( !IsReady() ) {
            return DlssEvaluationResult::eFailed;
        }
        const std::unique_lock lock{ GetNgxMutex(), std::try_to_lock };
        if ( !lock.owns_lock() ) {
            return DlssEvaluationResult::eBusy;
        }

        constexpr VkImageSubresourceRange colorRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        std::array<NVSDK_NGX_Resource_VK, 6> inputs{};
        std::array<VkImageMemoryBarrier2, 6> barriers{};
        unsigned int barrierCount{};
        for ( std::size_t index{}; index < inputs.size(); ++index ) {
            const auto &image{ evaluation.mInputs[index] };
            inputs[index] = NVSDK_NGX_Create_ImageView_Resource_VK( image.mView, image.mImage, colorRange,
                image.mFormat, mState->mDescription.mInputSize.width, mState->mDescription.mInputSize.height, false );
            if ( image.mLayout != VK_IMAGE_LAYOUT_GENERAL ) {
                auto &barrier{ barriers[barrierCount++] };
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
                barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
                barrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
                barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
                barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
                barrier.oldLayout = image.mLayout;
                barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = image.mImage;
                barrier.subresourceRange = colorRange;
            }
        }

        // Graph inputs remain logical reads. Restore their declared layouts after
        // the SDK call so its native GENERAL requirement stays local to this pass.
        VkDependencyInfo dependency{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        dependency.imageMemoryBarrierCount = barrierCount;
        dependency.pImageMemoryBarriers = barriers.data();
        if ( barrierCount ) {
            vkCmdPipelineBarrier2( commands, &dependency );
        }

        const auto &image{ evaluation.mOutput };
        auto output{ NVSDK_NGX_Create_ImageView_Resource_VK( image.mView, image.mImage, colorRange,
            image.mFormat, mState->mDescription.mOutputSize.width, mState->mDescription.mOutputSize.height, true ) };
        NVSDK_NGX_VK_DLSSD_Eval_Params evaluate{};
        evaluate.pInColor = &inputs[0];
        evaluate.pInDiffuseAlbedo = &inputs[1];
        evaluate.pInSpecularAlbedo = &inputs[2];
        evaluate.pInNormals = &inputs[3];
        evaluate.pInRoughness = &inputs[3];
        evaluate.pInDepth = &inputs[4];
        evaluate.pInMotionVectors = &inputs[5];
        evaluate.pInOutput = &output;
        // NGX receives the de-jitter offset; the ray generator stores film jitter.
        evaluate.InJitterOffsetX = -evaluation.mJitterX;
        evaluate.InJitterOffsetY = -evaluation.mJitterY;
        evaluate.InMVScaleX = 1.0f;
        evaluate.InMVScaleY = 1.0f;
        evaluate.InRenderSubrectDimensions.Width = mState->mDescription.mInputSize.width;
        evaluate.InRenderSubrectDimensions.Height = mState->mDescription.mInputSize.height;
        evaluate.InReset = evaluation.mReset ? 1 : 0;
        evaluate.InPreExposure = 1.0f;
        evaluate.InExposureScale = 1.0f;
        evaluate.InFrameTimeDeltaInMsec = evaluation.mFrameTimeMilliseconds;
        evaluate.pInWorldToViewMatrix = const_cast<float *>( evaluation.mWorldToView.data() );
        evaluate.pInViewToClipMatrix = const_cast<float *>( evaluation.mViewToClip.data() );
        const auto result{ NGX_VULKAN_EVALUATE_DLSSD_EXT( commands, mState->mHandle, mState->mParameters, &evaluate ) };

        for ( unsigned int index{}; index < barrierCount; ++index ) {
            std::swap( barriers[index].oldLayout, barriers[index].newLayout );
        }
        if ( barrierCount ) {
            vkCmdPipelineBarrier2( commands, &dependency );
        }
        if ( NVSDK_NGX_FAILED( result ) ) {
            mState->mError = "DLSS evaluation failed: " + GetNgxError( result );
            return DlssEvaluationResult::eFailed;
        }
        return DlssEvaluationResult::eSuccess;
#else
        return DlssEvaluationResult::eFailed;
#endif
    }

}
