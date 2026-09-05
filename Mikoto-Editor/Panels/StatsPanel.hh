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

#ifndef MIKOTO_STATS_PANEL_HH
#define MIKOTO_STATS_PANEL_HH

#include <EASTL/memory.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Panels/Panel.hh>

namespace mikoto::editor {
    struct EditorState;

    struct StatsPanelCreateInfo {
        EditorState *mState{};
    };

    class StatsPanel final : public Panel {
    public:
        explicit StatsPanel(const StatsPanelCreateInfo& info);

        auto OnRender(float timeStep) -> void override;

    private:
        auto DrawUpdateInfo() -> void;
        auto DrawSystemInfo() -> void;
        auto DrawPerformance( float timeStep ) -> void;

    private:
        EditorState *mState{};

        core::f32 mFrameRate{};
        core::f32 mFrameTime{};

        core::f64 mLastTime{};
        core::f64 mCurrentTime{};
        core::f32 mIntervalUpdate{ };
    };
}


#endif // MIKOTO_STATS_PANEL_HH
