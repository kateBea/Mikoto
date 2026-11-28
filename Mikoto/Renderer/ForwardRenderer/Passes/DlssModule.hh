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

#ifndef MIKOTO_RENDERER_FORWARD_RENDERER_DLSS_MODULE_HH
#define MIKOTO_RENDERER_FORWARD_RENDERER_DLSS_MODULE_HH

#include <EASTL/string.h>

#include <EASTL/unique_ptr.h>

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Denoising/Denoiser.hh>

namespace mikoto::scene {
    class Camera;
    class Scene;
}

namespace mikoto::renderer {
    class RayTracingModule;

    // Each view owns its feature and output. Transport uses input resolution;
    // editor depth, picking, overlays and display passes use viewport resolution.
    class DlssModule final {
    public:
        DlssModule( rhi::RenderResolution inputResolution, rhi::RenderResolution viewportResolution );
        DlssModule( core::uint2 inputSize, core::uint2 outputSize );
        ~DlssModule();

        auto SetSettings( const DenoiserSettings &settings ) -> void;
        auto Prepare( const scene::Scene *scene, const scene::Camera *camera, const RayTracingModule &rays ) -> void;
        auto RegisterPasses( FrameGraph &graph ) -> void;

        MKT_NODISCARD auto GetOutput() const -> FGTextureHandle;
        MKT_NODISCARD auto IsActive() const -> bool;
        MKT_NODISCARD auto GetError() const -> const eastl::string &;

    private:
        struct State;
        eastl::unique_ptr<State> mState{};
        const RayTracingModule *mRays{};
        DenoiserSettings mSettings{};
        core::uint2 mInputSize{};
        core::uint2 mOutputSize{};
    };
}

#endif// MIKOTO_RENDERER_FORWARD_RENDERER_DLSS_MODULE_HH
