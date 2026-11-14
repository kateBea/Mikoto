//    Copyright 2025 ケイト
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

#ifndef MIKOTO_PANELS_SCENE_CONTROLS_HH
#define MIKOTO_PANELS_SCENE_CONTROLS_HH

namespace mikoto::editor {

    struct EditorState;

    // Each caller supplies visibility independently of the shared scene actions.
    struct SceneControlVisibility {
        bool mPlay{ true };
        bool mSimulate{ true };
        bool mStop{ true };
    };

    /** @brief Width of visible playback buttons using the current style; zero when all are hidden. */
    auto GetSceneControlsWidth( const SceneControlVisibility& visibility ) -> float;
    /** @brief Draws visible play, simulate and stop actions at the current cursor. */
    auto DrawSceneControls( EditorState& state, const SceneControlVisibility& visibility ) -> void;
}

#endif// MIKOTO_PANELS_SCENE_CONTROLS_HH
