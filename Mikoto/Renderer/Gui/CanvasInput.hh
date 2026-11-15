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

#ifndef MIKOTO_RENDERER_GUI_CANVAS_INPUT_HH
#define MIKOTO_RENDERER_GUI_CANVAS_INPUT_HH

#include <Core/Types.hh>

namespace mikoto::scene {
    class Scene;
    class Camera;
}

namespace mikoto::renderer {

    enum class CanvasNavigation : core::u8;

    /**
     * @brief Routes pointer input in game-image UV coordinates to canvases.
     * @param scene Scene whose canvases receive the input.
     * @param camera Camera defining the displayed game image.
     * @param uv Pointer coordinates in the game image, excluding letterboxing.
     * @param down Whether the pointer button is currently held.
     * @param pressed Whether the pointer button was pressed this frame.
     * @param released Whether the pointer button was released this frame.
     * @param allowNewPress Whether a new press may acquire a canvas.
     * @returns True when a canvas consumes input.
     */
    auto UpdateCanvasInput( scene::Scene& scene, const scene::Camera& camera, core::float2 uv,
                            bool down, bool pressed, bool released, bool allowNewPress ) -> bool;

    /**
     * @brief Releases pointer capture and focus on all scene canvases.
     * @param scene Scene whose canvases receive the input.
     */
    auto CancelCanvasInput( scene::Scene& scene ) -> void;

    /**
     * @brief Routes keyboard navigation to an eligible scene canvas.
     * @param scene Scene whose canvases receive the input.
     * @param action Keyboard navigation action to route.
     * @returns True when a canvas consumes the action.
     */
    auto NavigateCanvasInput( scene::Scene& scene, CanvasNavigation action ) -> bool;

}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_GUI_CANVAS_INPUT_HH
