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

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/matrix.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <Core/Profiler.hh>

#include <Scene/Scene.hh>
#include <Scene/Camera.hh>
#include <Scene/Component.hh>

#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Core/SceneRenderer.hh>
#include <Renderer/CompatibilityRenderer/Passes/CompatibilityGeometryModule.hh>
#include <Renderer/CompatibilityRenderer/Passes/CompatibilityLightingModule.hh>
#include <Renderer/CompatibilityRenderer/Passes/CompatibilityShadowModule.hh>

namespace mikoto::renderer {
    static auto ComputeShadowMatrix( const scene::Camera& camera, const core::float3& direction,
        core::f32 shadowDistance, core::u32 resolution ) -> core::float4x4 {
        // Fit one camera-relative orthographic map and snap it to the shadow texels.
        // https://learn.microsoft.com/en-us/windows/win32/dxtecharts/common-techniques-to-improve-shadow-depth-maps
        constexpr core::float3 worldUp{ 0.0f, 1.0f, 0.0f };
        constexpr core::float3 alternateUp{ 0.0f, 0.0f, 1.0f };
        const auto up{ glm::abs( glm::dot( direction, worldUp ) ) > 0.99f ? alternateUp : worldUp };
        const auto lightView{ glm::lookAtRH( core::float3{}, direction, up ) };
        const auto cameraTransform{ glm::inverse( camera.GetViewMatrix() ) };
        const auto forward{ -core::float3{ cameraTransform[2] } };
        const core::f32 radius{ shadowDistance * 0.5f };
        const auto center{ lightView * core::float4{ camera.GetPosition() + forward * radius, 1.0f } };
        const core::f32 texel{ shadowDistance / core::as<core::f32>( resolution ) };
        const core::float2 snapped{ glm::round( core::float2{ center } / texel ) * texel };

        return glm::orthoRH_ZO( snapped.x - radius, snapped.x + radius, snapped.y - radius, snapped.y + radius,
            -center.z - shadowDistance, -center.z + shadowDistance ) * lightView;
    }

    auto CompatibilityShadowModule::Record( rhi::IGpuDevice* device, rhi::ICommandList* commands, const scene::Scene& scene,
        const scene::Camera& camera, CompatibilityGeometryModule& geometry, const CompatibilityEnvironment& environment,
        const SceneRenderSettings& settings ) -> void {
        MKT_BEGIN_PROFILER();

        mDiagnostics = {};
        mLightIndex = 0;
        if ( !geometry.GetDrawCount() ) {
            return;
        }

        core::float3 direction{};
        core::u32 index{};
        const auto& registry{ scene.GetRegistry() };

        // Use the same active-light order and limit as the lighting uniform.
        for ( const auto& [entity, tag, transform, component]:
            registry.view<scene::TagComponent, scene::TransformComponent, scene::LightComponent>().each() ) {
            if ( !tag.IsActive() ) {
                continue;
            }

            const auto type{ component.GetActiveType() };
            if ( type != LightType::eDirectional && type != LightType::ePoint && type != LightType::eSpot ) {
                continue;
            }

            ++mDiagnostics.mActiveLights[core::as<core::usize>( type )];
            ++index;
            if ( !mLightIndex && type == LightType::eDirectional ) {
                const auto& light{ component.Get<DirectionalLight>() };
                if ( light.IsShadowCaster() ) {
                    direction = light.GetWorldDirection( transform.GetWorldTransform() );
                    mLightIndex = index;
                }
            }

            if ( index == CompatibilityLightingModule::kMaxLights ) {
                break;
            }
        }

        if ( !mLightIndex || glm::dot( direction, direction ) <= 0.0f ) {
            mLightIndex = 0;
            return;
        }

        constexpr core::u32 lowResolution{ 512 };
        constexpr core::u32 mediumResolution{ 1024 };
        constexpr core::u32 highResolution{ 2048 };
        const core::u32 resolution{ settings.mShadowQuality == ShadowMapQuality::eLow ? lowResolution :
            settings.mShadowQuality == ShadowMapQuality::eHigh ? highResolution : mediumResolution };
        const rhi::Extent2D size{ .mWidth = resolution, .mHeight = resolution };
        ResizeCompatibilityTarget( device, mColor, size, rhi::Format::eR32_FLOAT,
            rhi::TextureUsageFlagsBits::RenderTarget | rhi::TextureUsageFlagsBits::ShaderResource, "Compatibility shadow map" );
        ResizeCompatibilityTarget( device, mDepth, size, kCompatibilityDepthFormat,
            rhi::TextureUsageFlagsBits::DepthTarget, "Compatibility shadow depth" );

        // The shadow vertex shader reads the first matrix in the frame buffer.
        mMatrix = ComputeShadowMatrix( camera, glm::normalize( direction ), settings.mShadowDistance, resolution );
        if ( !mUniform ) {
            mUniform = CreateReusableCompatibilityUniform( device, sizeof( mMatrix ) );
        }
        auto uniform{ WriteCompatibilityUniform( device, commands, mUniform, &mMatrix, sizeof( mMatrix ) ) };

        auto pass{ rhi::RenderPassDescription{}
            .SetScopeName( "Compatibility Shadows" )
            .SetRenderArea( rhi::Rect{ core::as<core::i32>( resolution ), core::as<core::i32>( resolution ) } )
            .AddDepthTarget( mDepth )
            .AddRenderTarget( mColor, rhi::kColorWhite ) };
        commands->SetTransition( rhi::TransitionDescription{}
            .AddTexture( mColor.GetPtr(), rhi::ResourceStates::eRenderTarget )
            .AddTexture( mDepth.GetPtr(), rhi::ResourceStates::eDepthWrite ) );
        commands->BeginRenderPass( pass );
        SetCompatibilityViewport( commands, size );
        geometry.RecordShadows( commands, environment, *uniform.GetPtr() );
        commands->EndRenderPass();
        commands->SetTransition( rhi::TransitionDescription{}.AddTexture( mColor.GetPtr(), rhi::ResourceStates::eShaderResource ) );

        mDiagnostics.mMappedLights[core::as<core::usize>( LightType::eDirectional )] = 1;
        mDiagnostics.mSubmittedMaps = 1;
        mDiagnostics.mCasterBatches = core::as<core::u32>( geometry.GetDrawCount() );
        mDiagnostics.mSubmittedBatches = mDiagnostics.mCasterBatches;
    }

    auto CompatibilityShadowModule::GetMap() const -> rhi::TextureHandle {
        return mLightIndex ? mColor : rhi::TextureHandle{};
    }
}
