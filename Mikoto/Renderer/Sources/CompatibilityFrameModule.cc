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


#include <glm/matrix.hpp>

#include <Core/Profiler.hh>

#include <Scene/Camera.hh>

#include <Renderer/Rhi/Buffer.hh>
#include <Renderer/CompatibilityRenderer/CompatibilityPassResources.hh>
#include <Renderer/CompatibilityRenderer/Passes/CompatibilityFrameModule.hh>

namespace mikoto::renderer {
    // Match the uniform layout in CompatibilityFrame.slang.
    struct alignas( 16 ) CompatibilityFrameParameters {
        core::float4x4 mViewProjection{ 1.0f };
        core::float4x4 mInverseSkyViewProjection{ 1.0f };
        core::float4 mCameraPosition{};
        core::float4 mEnvironment{};
        core::uint4 mLighting{};
        CompatibilityLightingModule::Lights mLights{};
        core::float4x4 mShadowViewProjection{ 1.0f };
    };

    auto CompatibilityFrameModule::Initialize( rhi::IGpuDevice& device ) -> void {
        mDevice = &device;
        mUniform = CreateReusableCompatibilityUniform( device, sizeof( CompatibilityFrameParameters ) );
    }

    auto CompatibilityFrameModule::Prepare( rhi::ICommandList& commands, const scene::Scene* scene, const scene::Camera* camera,
        const core::float4& environment, const core::float4x4& shadowMatrix, core::u32 shadowLight ) -> rhi::BufferHandle {
        MKT_BEGIN_PROFILER();

        CompatibilityFrameParameters frame{ .mEnvironment = environment, .mShadowViewProjection = shadowMatrix };
        if ( camera ) {
            frame.mViewProjection = camera->GetViewProjection();
            // An infinite panorama follows camera rotation without following translation.
            const core::float4x4 skyView{ core::float3x3{ camera->GetViewMatrix() } };
            frame.mInverseSkyViewProjection = glm::inverse( camera->GetProjection() * skyView );
            frame.mCameraPosition = core::float4{ camera->GetPosition(), 1.0f };
        }

        frame.mLighting.x = core::as<core::u32>( mLighting.Gather( scene, frame.mLights ) );
        frame.mLighting.y = shadowLight;

        return WriteCompatibilityUniform( *mDevice, commands, mUniform, &frame, sizeof( frame ) );
    }
}// namespace mikoto::renderer
