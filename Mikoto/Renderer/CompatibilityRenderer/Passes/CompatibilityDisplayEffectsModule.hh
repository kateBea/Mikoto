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


#ifndef MIKOTO_RENDERER_COMPATIBILITY_RENDERER_DISPLAY_EFFECTS_MODULE_HH
#define MIKOTO_RENDERER_COMPATIBILITY_RENDERER_DISPLAY_EFFECTS_MODULE_HH

#include <Renderer/CompatibilityRenderer/CompatibilityPassResources.hh>

namespace mikoto::renderer {
    /** Records optional display-space effects after tone mapping. */
    class CompatibilityDisplayEffectsModule {
    public:
        MKT_NODISCARD auto RecordChromaticAberration( rhi::IGpuDevice& device, rhi::ICommandList& commands,
            rhi::TextureHandle toneMapped, core::f32 intensity, core::f32 directionDegrees ) -> rhi::TextureHandle;

    private:
        CompatibilityFullscreenPass mChromaticAberration{};
        rhi::TextureHandle mOutput{};
        bool mInitialized{};
    };
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_COMPATIBILITY_RENDERER_DISPLAY_EFFECTS_MODULE_HH
