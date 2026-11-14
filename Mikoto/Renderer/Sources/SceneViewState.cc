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

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Core/SceneViewState.hh>

#include <Scene/Camera.hh>

namespace mikoto::renderer {

    SceneViewState::SceneViewState( const SceneRenderState& sceneState )
        : mSceneState{ &sceneState } {
    }

    SceneViewState::~SceneViewState() = default;

    auto SceneViewState::GetSceneState() const -> const SceneRenderState& {
        return *mSceneState;
    }

    auto SceneViewState::SetCamera( const scene::Camera *camera ) -> void {
        mCamera = camera;

        // TODO: Invalidate temporal histories when the view switches cameras.
    }

    auto SceneViewState::GetCamera() const -> const scene::Camera * {
        return mCamera;
    }

    auto SceneViewState::SetRenderResolution( rhi::RenderResolution resolution ) -> void {
        mResolution = resolution;

        // TODO: Resize this view's targets and invalidate incompatible histories.
    }

    auto SceneViewState::GetRenderResolution() const -> rhi::RenderResolution {
        return mResolution;
    }
}// namespace mikoto::renderer
