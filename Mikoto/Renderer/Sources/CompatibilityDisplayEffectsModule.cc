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


#include <glm/trigonometric.hpp>

#include <Core/Profiler.hh>

#include <Renderer/Rhi/Texture.hh>
#include <Renderer/CompatibilityRenderer/Passes/CompatibilityDisplayEffectsModule.hh>

namespace mikoto::renderer {
    struct alignas( 16 ) CompatibilityChromaticAberrationParameters {
        core::float2 mTexelSize{};
        core::f32 mIntensity{};
        core::f32 mDirection{};
    };

    auto CompatibilityDisplayEffectsModule::RecordChromaticAberration( rhi::IGpuDevice& device, rhi::ICommandList& commands,
        rhi::TextureHandle toneMapped, core::f32 intensity, core::f32 directionDegrees ) -> rhi::TextureHandle {
        MKT_BEGIN_PROFILER();

        if ( intensity <= 0.0f ) {
            mOutput.Reset();
            return toneMapped;
        }

        if ( !mInitialized ) {
            mChromaticAberration.Initialize( device, "../compatibility/slang/CompatibilityChromaticAberration_Frag.slang",
                sizeof( CompatibilityChromaticAberrationParameters ) );
            mInitialized = true;
        }

        const rhi::Extent2D size{ .mWidth = toneMapped->GetWidth(), .mHeight = toneMapped->GetHeight() };
        ResizeCompatibilityTarget( device, mOutput, size, kCompatibilityOutputFormat,
            rhi::TextureUsageFlagsBits::RenderTarget | rhi::TextureUsageFlagsBits::ShaderResource |
                rhi::TextureUsageFlagsBits::CopySource, "Compatibility chromatic aberration color" );

        const CompatibilityChromaticAberrationParameters parameters{
            .mTexelSize = { 1.0f / core::as<core::f32>( size.mWidth ), 1.0f / core::as<core::f32>( size.mHeight ) },
            .mIntensity = intensity,
            .mDirection = glm::radians( directionDegrees )
        };
        mChromaticAberration.Record( commands, "Compatibility Chromatic Aberration", toneMapped, mOutput, &parameters, sizeof( parameters ) );

        return mOutput;
    }
}// namespace mikoto::renderer
