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

#ifndef MIKOTO_PANELS_STATS_PANEL_HH
#define MIKOTO_PANELS_STATS_PANEL_HH

#include <EASTL/memory.h>
#include <EASTL/string.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Diagnostics/FrameHistory.hh>

#include <Panels/Panel.hh>

namespace mikoto::editor {
    struct EditorState;

    struct StatsPanelCreateInfo {
        EditorState *mState{};

        eastl::string mGroup{ "Other" };
        bool mIsToggleable{ true };
    };

    class StatsPanel final : public Panel {
    public:
        explicit StatsPanel( const StatsPanelCreateInfo &info );

        auto OnRender( float timeStep ) -> void override;

    private:
        auto Capture() -> void;
        auto DrawSystemInfo() -> void;
        auto DrawPerformance() -> void;
        auto DrawSceneInfo() -> void;

    private:
        struct Snapshot {
            eastl::string cpu, gpu, api, scene, sceneState;
            double processVram{};
            double processSharedGpuRam{};
            double streamingCpu{};
            double streamingGpu{};
            double streamingReservedCpu{};
            double streamingReservedGpu{};

            core::usize streamingQueued{};
            core::usize streamingLoading{};
            core::usize streamingEvicted{};

            bool streamingOverBudget{};
            bool processGpuMemoryAvailable{};

            core::f64 cpuPercent{};
            core::f64 totalRam{};
            core::f64 availableRam{};
            core::f64 processRam{};
            core::f64 gpuHeapUsage{};
            core::f64 uptime{};

            core::usize entities{};
            core::usize meshes{};
            core::usize lights{};
            core::usize texts{};
            core::usize rigidBodies{};
            core::usize colliders{};
            core::usize audioSources{};
            core::usize playingAudio{};
            core::u32 renderWidth{};
            core::u32 renderHeight{};
        };

    private:
        EditorState *mState{};

        FrameHistory mHistory{};
        FrameSummary mFrames{};

        core::f32 mElapsed{};
        core::f32 mIntervalUpdate{ 0.5f };

        bool mPaused{};
        bool mCaptured{};

        core::i32 mTargetIndex{ 1 };

        Snapshot mSnapshot{};
    };
}// namespace mikoto::editor

#endif// MIKOTO_PANELS_STATS_PANEL_HH

