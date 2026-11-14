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


#ifndef MIKOTO_RENDERER_COMPATIBILITY_RENDERER_POST_PROCESS_MODULE_HH
#define MIKOTO_RENDERER_COMPATIBILITY_RENDERER_POST_PROCESS_MODULE_HH

#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Rhi/CommandList.hh>

#include <Renderer/CompatibilityRenderer/CompatibilityPassResources.hh>

namespace mikoto::renderer {
    /** Converts linear scene color into the tone-mapped image used by display effects. */
    class CompatibilityPostProcessModule {
    public:
        struct Settings {
            rhi::Color mClearColor{};
            core::f32 mExposure{ 1.0f };
            core::f32 mGamma{ 2.2f };
            core::i32 mToneMapper{};
            bool mOutputForPreview{};
        };

        auto Initialize( rhi::IGpuDevice* device ) -> void;
        MKT_NODISCARD auto RecordToneMap(
            rhi::ICommandList* commands,
            rhi::TextureHandle sceneColor,
            const Settings& settings ) -> rhi::TextureHandle;
        MKT_NODISCARD auto GetOutput() const -> rhi::TextureHandle;

    private:
        rhi::IGpuDevice* mDevice{};
        CompatibilityFullscreenPass mToneMap{};
        rhi::TextureHandle mOutput{};
    };
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_COMPATIBILITY_RENDERER_POST_PROCESS_MODULE_HH
