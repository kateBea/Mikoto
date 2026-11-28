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

#include <algorithm>
#include <chrono>
#include <thread>
#include <vector>

#if MIKOTO_ENABLE_OIDN
#include <vk_oidn/vk_oidn.h>
#endif

#include <Core/String.hh>

#include <Logging/Logger.hh>

#include <Renderer/Rhi/Texture.hh>
#include <Renderer/Rhi/Vulkan/VulkanContext.hh>
#include <Renderer/Rhi/Vulkan/VulkanDenoiser.hh>
#include <Renderer/Rhi/Vulkan/VulkanDevice.hh>
#include <Renderer/Rhi/Vulkan/VulkanDlss.hh>
#include <Renderer/Rhi/Vulkan/VulkanHelpers.hh>

namespace mikoto::renderer::vulkan {
    using namespace rhi;
    using namespace core;

    struct Denoiser::State {
        VkDevice mDevice{};
        VkCommandPool mCreationPool{};
        VkCommandBuffer mCreationCommands{};
        bool mCreationPending{};
#if MIKOTO_ENABLE_OIDN
        std::unique_ptr<vkoidn::Context> mOidnContext{};
        std::unique_ptr<vkoidn::Denoiser> mOidn{};
#endif
#if MIKOTO_ENABLE_DLSS
        DlssFeature mDlss{};
#endif
    };

    Denoiser::Denoiser( const DenoiserDescription &description )
        : IDenoiser{ description }, mState{ eastl::make_unique<State>() } {
    }

    Denoiser::~Denoiser() {
        Destroy();
    }

    auto Denoiser::Initialize() -> void {
        if ( mDescription.mType == ImageDenoiserType::eOidn ) {
            InitializeOidn();
        }
        else {
            InitializeDlss();
        }
    }

    auto Denoiser::BeginCreation() -> void {
        auto *device{ core::checked_cast<Device *>( GetOwningDevice() ) };
        mState->mDevice = device->GetDevice();
        const auto *queue{ core::checked_cast<vulkan::Queue *>( device->GetQueue( QueueType::eGraphics ) ) };
        VkCommandPoolCreateInfo pool{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        pool.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        pool.queueFamilyIndex = queue->GetFamilyIndex();
        MKT_VK_CHECK( vkCreateCommandPool( mState->mDevice, &pool, nullptr, &mState->mCreationPool ) );

        VkCommandBufferAllocateInfo allocation{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        allocation.commandPool = mState->mCreationPool;
        allocation.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        allocation.commandBufferCount = 1;
        MKT_VK_CHECK( vkAllocateCommandBuffers( mState->mDevice, &allocation, &mState->mCreationCommands ) );

        VkCommandBufferInheritanceInfo inheritance{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO };
        VkCommandBufferBeginInfo begin{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        begin.pInheritanceInfo = &inheritance;
        MKT_VK_CHECK( vkBeginCommandBuffer( mState->mCreationCommands, &begin ) );
    }

    auto Denoiser::InitializeOidn() -> void {
#if MIKOTO_ENABLE_OIDN
        if ( !core::checked_cast<Device *>( GetOwningDevice() )->SupportsOidn() ) {
            mError = "OIDN requires FP16 storage/compute, Vulkan memory-model features and push descriptors.";
            return;
        }
        auto *device{ core::checked_cast<vulkan::Device *>( GetOwningDevice() ) };
        mState->mDevice = device->GetDevice();
        std::vector<const char *> extensions{};
        for ( const auto &extension: device->GetEnabledExtensions() ) {
            extensions.push_back( extension.c_str() );
        }

        vkoidn::ContextCreateInfo contextInfo{};
        contextInfo.instance = vulkan::Context::Get()->GetInstance().mInstance;
        contextInfo.physicalDevice = device->GetPhysicalDevice()->mPhysicalDevice;
        contextInfo.device = mState->mDevice;
        contextInfo.getInstanceProcAddr = vkGetInstanceProcAddr;
        contextInfo.getDeviceProcAddr = vkGetDeviceProcAddr;
        contextInfo.enabledExtensions = extensions.data();
        contextInfo.enabledExtensionCount = static_cast<uint32_t>( extensions.size() );
        auto result{ vkoidn::Context::create( contextInfo, mState->mOidnContext ) };
        if ( result != vkoidn::Result::eSuccess ) {
            mError = string::Format( "OIDN initialization failed: {}", vkoidn::getResultString( result ) );
            MKT_CORE_LOGGER_ERROR( "{}", mError );
            return;
        }

        BeginCreation();

        // Denoiser::create compiles the network kernels and allocates scratch
        // buffers. Keep the entire operation on the graph compilation worker,
        // rather than deferring this expensive part to the first render pass.
        vkoidn::DenoiserCreateInfo network{};
        network.width = mDescription.mInputSize.x;
        network.height = mDescription.mInputSize.y;
        network.hdr = true;
        network.quality = static_cast<vkoidn::Quality>( mDescription.mQuality );
        result = vkoidn::Denoiser::create( *mState->mOidnContext, mState->mCreationCommands, network, mState->mOidn );
        MKT_VK_CHECK( vkEndCommandBuffer( mState->mCreationCommands ) );

        if ( result != vkoidn::Result::eSuccess ) {
            mError = string::Format( "OIDN network creation failed: {}", vkoidn::getResultString( result ) );
            MKT_CORE_LOGGER_ERROR( "{}", mError );
            return;
        }

        mState->mCreationPending = true;
        mReady = true;
        mIsAllocated = true;
#else
        mError = "OIDN is disabled in this build.";
#endif
    }

    auto Denoiser::InitializeDlss() -> void {
#if MIKOTO_ENABLE_DLSS
        auto *device{ core::checked_cast<vulkan::Device *>( GetOwningDevice() ) };
        auto &context{ device->GetDlssContext() };
        auto support{ context.GetCapabilities() };
        while ( support.mSupport == vulkan::DlssSupport::ePreparing ) {
            // Only the graph compilation worker waits for device prewarm.
            std::this_thread::sleep_for( std::chrono::milliseconds{ 1 } );
            support = context.GetCapabilities();
        }
        if ( support.mSupport != vulkan::DlssSupport::eAvailable ) {
            mError = support.mMessage.c_str();
            return;
        }

        BeginCreation();
        const vulkan::DlssFeatureDescription description{
            .mPreset = core::as<core::u32>( mDescription.mPreset ),
            .mInputSize = { mDescription.mInputSize.x, mDescription.mInputSize.y },
            .mOutputSize = { mDescription.mOutputSize.x, mDescription.mOutputSize.y },
        };
        const bool created{ mState->mDlss.Initialize( context, mState->mDevice, mState->mCreationCommands, description ) };
        MKT_VK_CHECK( vkEndCommandBuffer( mState->mCreationCommands ) );
        if ( !created ) {
            mError = mState->mDlss.GetError().c_str();
            return;
        }
        mState->mCreationPending = true;
        mReady = true;
        mIsAllocated = true;
#else
        mError = "DLSS is disabled in this build.";
#endif
    }

    auto Denoiser::RecordInitialization( VkCommandBuffer commands ) -> bool {
        if ( !mReady ) {
            return false;
        }
        // One-time uploads run on the graph's graphics command list. The graph
        // emits the memory dependency before the first evaluation of this state.
        if ( mState->mCreationPending ) {
            vkCmdExecuteCommands( commands, 1, &mState->mCreationCommands );
            mState->mCreationPending = false;
        }
        return true;
    }

    auto Denoiser::RecordEvaluation( VkCommandBuffer commands, const DenoiserDispatch &dispatch ) -> DenoiserResult {
        if ( !mReady || mState->mCreationPending ) {
            return DenoiserResult::eUnavailable;
        }
#if MIKOTO_ENABLE_OIDN
        if ( mDescription.mType == ImageDenoiserType::eOidn ) {
            vkoidn::DenoiseInputs inputs{};
            inputs.colorView.view = dispatch.mColor->GetNativeHandle( ObjectType::Vk_ImageView );
            inputs.outputView.view = dispatch.mOutput->GetNativeHandle( ObjectType::Vk_ImageView );
            inputs.outputView.format = GetFormat( dispatch.mOutput->GetFormat() );
            const auto result{ mState->mOidn->cmdDenoise( commands, inputs ) };
            if ( result == vkoidn::Result::eSuccess ) {
                return DenoiserResult::eSuccess;
            }
            mError = string::Format( "OIDN filtering failed: {}", vkoidn::getResultString( result ) );
            mReady = false;
            return DenoiserResult::eFailed;
        }
#endif
#if MIKOTO_ENABLE_DLSS
        DlssEvaluation evaluation{};
        const eastl::array<ITexture *, 6> inputs{
            dispatch.mColor,
            dispatch.mDiffuseAlbedo,
            dispatch.mSpecularAlbedo,
            dispatch.mNormalRoughness,
            dispatch.mDepth,
            dispatch.mMotion,
        };
        for ( core::usize index{}; index < inputs.size(); ++index ) {
            auto *texture{ inputs[index] };
            evaluation.mInputs[index] = {
                .mImage = texture->GetNativeHandle( ObjectType::Vk_Image ),
                .mView = texture->GetNativeHandle( ObjectType::Vk_ImageView ),
                .mFormat = GetFormat( texture->GetFormat() ),
                .mLayout = VK_IMAGE_LAYOUT_GENERAL,
            };
        }
        evaluation.mOutput = {
            .mImage = dispatch.mOutput->GetNativeHandle( ObjectType::Vk_Image ),
            .mView = dispatch.mOutput->GetNativeHandle( ObjectType::Vk_ImageView ),
            .mFormat = GetFormat( dispatch.mOutput->GetFormat() ),
            .mLayout = VK_IMAGE_LAYOUT_GENERAL,
        };
        std::copy( dispatch.mFrame.mWorldToView.begin(), dispatch.mFrame.mWorldToView.end(), evaluation.mWorldToView.begin() );
        std::copy( dispatch.mFrame.mViewToClip.begin(), dispatch.mFrame.mViewToClip.end(), evaluation.mViewToClip.begin() );
        evaluation.mJitterX = dispatch.mFrame.mJitter.x;
        evaluation.mJitterY = dispatch.mFrame.mJitter.y;
        evaluation.mFrameTimeMilliseconds = dispatch.mFrame.mFrameTimeMilliseconds;
        evaluation.mReset = dispatch.mFrame.mReset;
        const auto result{ mState->mDlss.Evaluate( commands, evaluation ) };
        if ( result == DlssEvaluationResult::eSuccess ) {
            return DenoiserResult::eSuccess;
        }
        if ( result == DlssEvaluationResult::eBusy ) {
            return DenoiserResult::eBusy;
        }
        mError = mState->mDlss.GetError().c_str();
        mReady = false;
        return DenoiserResult::eFailed;
#else
        return DenoiserResult::eUnavailable;
#endif
    }

    auto Denoiser::Destroy() -> void {
#if MIKOTO_ENABLE_DLSS
        mState->mDlss.Shutdown();
#endif
#if MIKOTO_ENABLE_OIDN
        mState->mOidn.reset();
        mState->mOidnContext.reset();
#endif
        if ( mState->mCreationPool ) {
            vkDestroyCommandPool( mState->mDevice, mState->mCreationPool, nullptr );
            mState->mCreationPool = VK_NULL_HANDLE;
        }
        mReady = false;
        mIsAllocated = false;
    }
}
