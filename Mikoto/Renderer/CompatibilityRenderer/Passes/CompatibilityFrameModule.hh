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


#ifndef MIKOTO_RENDERER_COMPATIBILITY_RENDERER_FRAME_MODULE_HH
#define MIKOTO_RENDERER_COMPATIBILITY_RENDERER_FRAME_MODULE_HH

#include <Renderer/Rhi/Buffer.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Rhi/CommandList.hh>

#include <Renderer/CompatibilityRenderer/Passes/CompatibilityLightingModule.hh>

namespace mikoto::scene {
    class Camera;
}

namespace mikoto::renderer {
    /** Uploads the camera and lights consumed by the compatibility scene passes. */
    class CompatibilityFrameModule {
    public:
        auto Initialize( rhi::IGpuDevice* device ) -> void;
        MKT_NODISCARD auto Prepare(
            rhi::ICommandList* commands,
            const scene::Scene* scene, const scene::Camera* camera,
            const core::float4& environment,
            const core::float4x4& shadowMatrix, core::u32 shadowLight ) -> rhi::BufferHandle;

    private:
        rhi::IGpuDevice* mDevice{};
        rhi::BufferHandle mUniform{};
        CompatibilityLightingModule mLighting{};
    };
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_COMPATIBILITY_RENDERER_FRAME_MODULE_HH
