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

#ifndef MIKOTO_PANELS_GPU_STATS_PANEL_HH
#define MIKOTO_PANELS_GPU_STATS_PANEL_HH

#include <EASTL/array.h>

#include <Renderer/Core/SceneRenderer.hh>

#include <Panels/Panel.hh>

namespace mikoto::editor {
    struct EditorState;

    struct GPUStatsPanelCreateInfo {
        EditorState* mState{};
        renderer::rhi::IGpuDevice* mGpuDevice{};

        eastl::string mGroup{ "Other" };
        bool mIsToggleable{ true };
    };

    class GPUStatsPanel final : public Panel {
    public:
        explicit GPUStatsPanel( const GPUStatsPanelCreateInfo& info );
        auto OnRender( core::f32 timestep ) -> void override;

    private:
        EditorState* mState{};
        renderer::rhi::IGpuDevice* mGpuDevice{};
        const renderer::ISceneRenderer* mLastRenderer{};

        core::u32 mHistoryHead{};
        core::u32 mHistoryCount{};
        core::u64 mLastSubmission{};

        eastl::array<core::f32, 240> mHistory{};

        bool mInspectGameRenderer{};
    };
}// namespace mikoto::editor

#endif// MIKOTO_PANELS_GPU_STATS_PANEL_HH

