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

#ifndef MIKOTO_SCRIPTING_SCRIPT_BUILD_TRACKER_HH
#define MIKOTO_SCRIPTING_SCRIPT_BUILD_TRACKER_HH

#include <EASTL/shared_ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Filesystem/Path.hh>

#include <Scene/Scene.hh>

namespace mikoto::scripting {

    /** @brief Tracks C# source changes and queues debounced background builds. */
    class ScriptBuildTracker final {
    public:
        /** @brief Creates a tracker with no active project or pending builds. */
        ScriptBuildTracker();

        /** @brief Retires watches; queued builds keep their state until completion. */
        ~ScriptBuildTracker();

        /**
         * @brief Refreshes source watches and schedules builds from the editor thread.
         * @param scene Optional scene whose C# scripts supply additional source directories.
         * @param assets Asset root directory to watch; changing it retires previous state.
         * @param project Optional C# project file to build when first selected.
         */
        auto Update( scene::Scene* scene, const filesystem::Path& assets, const filesystem::Path& project = {} ) -> void;

        /**
         * @brief Checks for queued source changes or a running build.
         * @returns True while a build is pending or executing.
         */
        MKT_NODISCARD auto IsBuilding() const -> bool;

    private:
        struct State;
        eastl::shared_ptr<State> mState{};
        filesystem::Path mAssets{};
        filesystem::Path mProject{};
    };

}// namespace mikoto::scripting

#endif// MIKOTO_SCRIPTING_SCRIPT_BUILD_TRACKER_HH
