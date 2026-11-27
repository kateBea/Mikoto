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

#pragma once

#include <EASTL/string.h>
#include <EASTL/unique_ptr.h>

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Denoising/Denoiser.hh>

namespace mikoto::renderer {
    class RayTracingModule;

    // One whole-image denoiser per view. Its Vulkan scratch resources stay
    // alive with this view's graph; transport history remains unfiltered.
    class OidnModule final {
    public:
        explicit OidnModule( rhi::RenderResolution resolution );
        ~OidnModule();

        auto SetSettings( DenoiserSettings settings ) -> void;
        auto RegisterPasses( FrameGraph &graph, rhi::IGpuDevice *device, bool pathTracing ) -> void;
        auto Prepare( const RayTracingModule &rays ) -> void;

        MKT_NODISCARD auto IsActive() const -> bool;
        MKT_NODISCARD auto GetError() const -> const eastl::string &;

    private:
        struct State;
        eastl::unique_ptr<State> mState{};
        DenoiserSettings mSettings{};
        rhi::RenderResolution mResolution{};
    };
}
