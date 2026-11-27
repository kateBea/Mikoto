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

#include <cmath>

#include <EASTL/memory.h>

#include <Core/Exception.hh>
#include <Core/Profiler.hh>

#include <Logging/Logger.hh>

#include <Renderer/CompatibilityRenderer/CompatibilityRenderer.hh>
#include <Renderer/Core/SceneRenderer.hh>
#include <Renderer/ForwardRenderer/ForwardRenderer.hh>
#include <Renderer/ForwardRenderer/Passes/OutlineModule.hh>
#include <Renderer/Rhi/GpuDevice.hh>

namespace mikoto::renderer {

    auto ISceneRenderer::Create() -> SceneRendererCreateInfo {
        return {};
    }

    auto SceneRendererCreateInfo::Build() const -> eastl::unique_ptr<ISceneRenderer> {
        MKT_BEGIN_PROFILER();

        if ( !mDevice ) {
            MKT_THROW_INVALID_ARGUMENT( "A scene renderer requires a GPU device." );
        }

        switch ( mType ) {
            case SceneRendererType::eForwardPlus:
                if ( !mDevice->IsGraphicsApi( rhi::GraphicsAPI::eVulkan ) &&
                     !mDevice->IsGraphicsApi( rhi::GraphicsAPI::eD3D12 ) ) {
                    MKT_CORE_LOGGER_DEBUG( "ForwardPlus scene rendering requires a Vulkan or D3D12 device. Defaulting to compatibility mode." );
                }
                else {
                    return eastl::make_unique<ForwardRenderer>( *this );
                }
            default:
                return eastl::make_unique<CompatibilityRenderer>( *this );
        }
    }

    auto SceneRendererCreateInfo::SetName( eastl::string_view name ) -> SceneRendererCreateInfo & {
        MKT_BEGIN_PROFILER();
        this->mName = name;
        return *this;
    }

    auto SceneRendererCreateInfo::SetDevice( renderer::rhi::IGpuDevice *device ) -> SceneRendererCreateInfo & {
        MKT_BEGIN_PROFILER();
        this->mDevice = device;
        return *this;
    }

    auto SceneRendererCreateInfo::SetRendererType( SceneRendererType type ) -> SceneRendererCreateInfo & {
        mType = type;
        return *this;
    }

    auto SceneRendererCreateInfo::SetMultisampling( rhi::Multisampling multisampling ) -> SceneRendererCreateInfo & {
        mMultisampling = multisampling;
        return *this;
    }

    auto SceneRendererCreateInfo::SetRenderResolution( renderer::rhi::RenderResolution resolution ) -> SceneRendererCreateInfo & {
        MKT_BEGIN_PROFILER();
        mResolution = resolution;
        return *this;
    }

    auto SceneRendererCreateInfo::SetViewportConvention( rhi::ViewportConvention convention ) -> SceneRendererCreateInfo & {
        MKT_BEGIN_PROFILER();
        mViewportConvention = convention;
        return *this;
    }

    auto SceneRenderSettings::Sanitize() -> void {
        if ( static_cast<unsigned>( mDepthOfFieldQuality ) > static_cast<unsigned>( DepthOfFieldQuality::eCinematic ) ) {
            mDepthOfFieldQuality = DepthOfFieldQuality::eHigh;
        }

        if ( static_cast<unsigned>( mShadowQuality ) > static_cast<unsigned>( ShadowMapQuality::eHigh ) ) {
            mShadowQuality = ShadowMapQuality::eMedium;
        }

        mRayTracing.Sanitize();
        mDenoiser.Sanitize();

        mInfiniteGrid.mCellSize = glm::clamp( mInfiniteGrid.mCellSize, 0.01f, 10000.0f );
        mInfiniteGrid.mOpacity = glm::clamp( mInfiniteGrid.mOpacity, 0.0f, 1.0f );
        mInfiniteGrid.mLineWidth = glm::clamp( mInfiniteGrid.mLineWidth, 0.5f, 4.0f );
        mInfiniteGrid.mAxisWidth = glm::clamp( mInfiniteGrid.mAxisWidth, 0.5f, 8.0f );

        mClouds.mFieldResolution = mClouds.mFieldResolution <= 64 ? 64 : mClouds.mFieldResolution <= 128 ? 128
                                                                                                         : 256;
        mClouds.mMaxSteps = glm::clamp( mClouds.mMaxSteps, 32u, 512u );
        mClouds.mSunSteps = glm::clamp( mClouds.mSunSteps, 4u, 64u );
        mClouds.mStepLength = glm::clamp( mClouds.mStepLength, 0.5f, 128.0f );
        mClouds.mMaxDistance = glm::clamp( mClouds.mMaxDistance, 100.0f, 200000.0f );
        mClouds.mHistoryWeight = glm::clamp( mClouds.mHistoryWeight, 0.0f, 0.98f );
        mClouds.mDetailStrength = glm::clamp( mClouds.mDetailStrength, 0.0f, 2.0f );
        mClouds.mMultipleScattering = glm::clamp( mClouds.mMultipleScattering, 0.0f, 0.9f );
        mClouds.mAmbientIntensity = glm::clamp( mClouds.mAmbientIntensity, 0.0f, 4.0f );
        mClouds.mEdgeDarkening = glm::clamp( mClouds.mEdgeDarkening, 0.0f, 1.0f );
        mClouds.mInnerGlow = glm::clamp( mClouds.mInnerGlow, 0.0f, 1.0f );
        mClouds.mDetailMipBias = glm::clamp( mClouds.mDetailMipBias, -2.0f, 2.0f );
        mClouds.mAnimatedJitterDistance = glm::clamp( mClouds.mAnimatedJitterDistance, 0.0f, 2000.0f );

        auto &reflections{ mScreenSpaceReflections };
        reflections.mMaxSteps = glm::clamp( reflections.mMaxSteps, 16u, 256u );
        reflections.mSpatialSamples = glm::clamp( reflections.mSpatialSamples, 1u, 16u );
        reflections.mMaxDistance = glm::clamp( reflections.mMaxDistance, 0.1f, 1000.0f );
        reflections.mThickness = glm::clamp( reflections.mThickness, 0.005f, 5.0f );
        reflections.mMaxRoughness = glm::clamp( reflections.mMaxRoughness, 0.05f, 1.0f );
        reflections.mHistoryWeight = glm::clamp( reflections.mHistoryWeight, 0.0f, 0.98f );
        reflections.mSamplingBias = glm::clamp( reflections.mSamplingBias, 0.0f, 0.9f );
        reflections.mIntensity = glm::clamp( reflections.mIntensity, 0.0f, 1.0f );

        if ( static_cast<unsigned>( mResolution ) > static_cast<unsigned>( rhi::RenderResolution::e3120P ) ) {
            mResolution = rhi::RenderResolution::e1080P;
        }

        if ( static_cast<unsigned>( mMultisampling ) > static_cast<unsigned>( rhi::Multisampling::eMsaaX16 ) ) {
            mMultisampling = rhi::Multisampling::eMsaaX1;
        }

        mSsaoIntensity = glm::clamp( mSsaoIntensity, 0.0f, 4.0f );

        mShadowDistance = glm::clamp( mShadowDistance, 200.0f, 1000.0f );
        mChromaticAberration.mIntensity = glm::clamp( mChromaticAberration.mIntensity, 0.0f, 32.0f );
        mChromaticAberration.mDirection = glm::clamp( mChromaticAberration.mDirection, -180.0f, 180.0f );
        mSelectionOutlineWidth = glm::clamp( mSelectionOutlineWidth, 1.0f, OutlineModule::kMaxOutlineWidth );

        for ( int axis{}; axis < 4; ++axis ) {
            mSelectionOutlineColor[axis] = glm::clamp( mSelectionOutlineColor[axis], 0.0f, 1.0f );
        }
    }

}// namespace mikoto::renderer
