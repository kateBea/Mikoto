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

#include <EASTL/memory.h>

#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Logging/Logger.hh>

#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Core/SceneRenderer.hh>
#include <Renderer/ForwardRenderer/ForwardRenderer.hh>
#include <Renderer/ForwardRenderer/Passes/OutlineModule.hh>
#include <Renderer/CompatibilityRenderer/CompatibilityRenderer.hh>

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

    auto SceneRendererCreateInfo::SetName( eastl::string_view name ) -> SceneRendererCreateInfo& {
        MKT_BEGIN_PROFILER();
        this->mName = name;
        return *this;
    }

    auto SceneRendererCreateInfo::SetDevice( renderer::rhi::IGpuDevice* device ) -> SceneRendererCreateInfo& {
        MKT_BEGIN_PROFILER();
        this->mDevice = device;
        return *this;
    }

    auto SceneRendererCreateInfo::SetRendererType( SceneRendererType type ) -> SceneRendererCreateInfo& {
        mType = type;
        return *this;
    }

    auto SceneRendererCreateInfo::SetMultisampling( rhi::Multisampling multisampling ) -> SceneRendererCreateInfo& {
        mMultisampling = multisampling;
        return *this;
    }

    auto SceneRendererCreateInfo::SetRenderResolution( renderer::rhi::RenderResolution resolution ) -> SceneRendererCreateInfo& {
        MKT_BEGIN_PROFILER();
        mResolution = resolution;
        return *this;
    }

    auto SceneRendererCreateInfo::SetViewportConvention( rhi::ViewportConvention convention ) -> SceneRendererCreateInfo& {
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

        if ( static_cast<unsigned>( mResolution ) > static_cast<unsigned>( rhi::RenderResolution::e3120P ) ) {
            mResolution = rhi::RenderResolution::e1080P;
        }

        if ( static_cast<unsigned>( mMultisampling ) > static_cast<unsigned>( rhi::Multisampling::eMsaaX16 ) ) {
            mMultisampling = rhi::Multisampling::eMsaaX1;
        }

        mSsaoIntensity = glm::clamp( mSsaoIntensity, 0.0f, 4.0f );

        mShadowDistance = glm::clamp( mShadowDistance, 200.0f, 1000.0f );
        mBloom.Sanitize();
        mChromaticAberration.mIntensity = glm::clamp( mChromaticAberration.mIntensity, 0.0f, 32.0f );
        mChromaticAberration.mDirection = glm::clamp( mChromaticAberration.mDirection, -180.0f, 180.0f );
        mSelectionOutlineWidth = glm::clamp( mSelectionOutlineWidth, 1.0f, OutlineModule::kMaxOutlineWidth );

        for ( int axis{}; axis < 4; ++axis ) {
            mSelectionOutlineColor[axis] = glm::clamp( mSelectionOutlineColor[axis], 0.0f, 1.0f );
        }
    }

}// namespace mikoto::renderer
