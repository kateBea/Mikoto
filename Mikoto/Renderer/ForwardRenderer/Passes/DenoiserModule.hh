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

#ifndef MIKOTO_RENDERER_FORWARD_RENDERER_DENOISER_MODULE_HH
#define MIKOTO_RENDERER_FORWARD_RENDERER_DENOISER_MODULE_HH

#include <EASTL/string.h>
#include <EASTL/unique_ptr.h>

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Denoising/Denoiser.hh>
#include <Renderer/ForwardRenderer/Passes/RayTracingModule.hh>

namespace mikoto::renderer {
    class OidnModule;
    class DlssModule;

    // One instance belongs to one renderer/view. The NRD SDK owns dispatch recipes;
    // Mikoto owns all textures, pipelines, synchronization and GPU lifetimes.
    class DenoiserModule final {
    public:
        explicit DenoiserModule( rhi::RenderResolution resolution );
        DenoiserModule( rhi::RenderResolution inputResolution, rhi::RenderResolution viewportResolution );
        DenoiserModule( core::uint2 inputSize, core::uint2 outputSize );
        ~DenoiserModule();

        auto SetSettings( DenoiserSettings settings ) -> void;
        auto RegisterPasses( FrameGraph &graph, rhi::IGpuDevice *device, bool pathTracing ) -> void;
        auto Prepare( const scene::Scene *scene, const scene::Camera *camera, const RayTracingModule &rays ) -> void;

        MKT_NODISCARD auto GetOutput() const -> FGTextureHandle;
        MKT_NODISCARD auto IsActive() const -> bool;
        MKT_NODISCARD auto GetError() const -> const eastl::string &;

    private:
        struct State;
        eastl::unique_ptr<State> mState{};
        eastl::unique_ptr<OidnModule> mOidn{};
        eastl::unique_ptr<DlssModule> mDlss{};
        FGTextureHandle mOutput{};
        DenoiserSettings mSettings{};
        core::uint2 mInputSize{};
    };
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_FORWARD_RENDERER_DENOISER_MODULE_HH
