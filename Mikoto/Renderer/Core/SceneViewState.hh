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

#ifndef MIKOTO_RENDERER_SCENE_VIEW_STATE_HH
#define MIKOTO_RENDERER_SCENE_VIEW_STATE_HH

#include <Core/Core.hh>

#include <Renderer/Rhi/Types.hh>

#include <Scene/Camera.hh>

namespace mikoto::renderer {

    class SceneRenderState;

    /**
     * Rendering state for one independently scheduled camera/configuration.
     * Borrows shared scene data and will own visibility, targets and temporal histories.
     * One view may expose several outputs, such as color and depth. No rendering yet.
     */
    // Ownership reference: Wicked separates camera visibility/targets from Scene data.
    // https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/wiRenderPath3D.h
    class SceneViewState final : public core::INonCopyable, public core::INonMoveable {
    public:
        explicit SceneViewState( const SceneRenderState& sceneState );
        ~SceneViewState();

        auto SetCamera( const scene::Camera* camera ) -> void;
        auto SetRenderResolution( rhi::RenderResolution resolution ) -> void;

        MKT_NODISCARD auto GetSceneState() const -> const SceneRenderState&;
        MKT_NODISCARD auto GetCamera() const -> const scene::Camera*;
        MKT_NODISCARD auto GetRenderResolution() const -> rhi::RenderResolution;

    private:
        // Borrowed inputs; owners keep the scene state and selected camera alive.
        const scene::Camera *mCamera{};
        const SceneRenderState* mSceneState{};
        rhi::RenderResolution mResolution{ rhi::RenderResolution::e1080P };

        // TODO: Own camera-dependent culling/draw data, output targets and histories.
        // Material overrides for a capture belong to this view, after shared extraction.
    };
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_SCENE_VIEW_STATE_HH
