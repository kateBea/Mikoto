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


#include <Core/Profiler.hh>

#include <Renderer/Rhi/Texture.hh>
#include <Renderer/CompatibilityRenderer/Passes/CompatibilityPostProcessModule.hh>

namespace mikoto::renderer {
    struct alignas( 16 ) CompatibilityToneMapParameters {
        core::float4 mClearColor{};
        core::f32 mExposure{};
        core::f32 mGamma{};
        core::i32 mToneMapper{};
        core::u32 mOutputForPreview{};
    };

    auto CompatibilityPostProcessModule::Initialize( rhi::IGpuDevice* device ) -> void {
        mDevice = device;
        mToneMap.Initialize( device, "../compatibility/slang/CompatibilityTonemap_Frag.slang",
            sizeof( CompatibilityToneMapParameters ) );
    }

    auto CompatibilityPostProcessModule::RecordToneMap( rhi::ICommandList* commands, rhi::TextureHandle sceneColor,
        const Settings& settings ) -> rhi::TextureHandle {
        MKT_BEGIN_PROFILER();

        ResizeCompatibilityTarget( mDevice, mOutput, { .mWidth = sceneColor->GetWidth(), .mHeight = sceneColor->GetHeight() },
            kCompatibilityOutputFormat, rhi::TextureUsageFlagsBits::RenderTarget | rhi::TextureUsageFlagsBits::ShaderResource |
                rhi::TextureUsageFlagsBits::CopySource, "Compatibility tone mapped color" );

        const auto& clear{ settings.mClearColor };
        const CompatibilityToneMapParameters parameters{
            .mClearColor = { clear.mR, clear.mG, clear.mB, clear.mA },
            .mExposure = settings.mExposure,
            .mGamma = settings.mGamma,
            .mToneMapper = settings.mToneMapper,
            .mOutputForPreview = settings.mOutputForPreview ? 1u : 0u
        };
        mToneMap.Record( commands, "Compatibility Tonemap", sceneColor, mOutput, &parameters, sizeof( parameters ) );

        return mOutput;
    }

    auto CompatibilityPostProcessModule::GetOutput() const -> rhi::TextureHandle {
        return mOutput;
    }
}// namespace mikoto::renderer
