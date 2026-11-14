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

#include <Renderer/Core/SceneRenderState.hh>

namespace mikoto::renderer {

    SceneRenderState::SceneRenderState( const scene::Scene& scene )
        : mScene{ &scene } {
    }

    SceneRenderState::~SceneRenderState() = default;

    auto SceneRenderState::GetScene() const -> const scene::Scene& {
        return *mScene;
    }

    auto SceneRenderState::Invalidate() -> void {
        // This is a CPU change token, not a prepared GPU snapshot or completion signal.
        BumpRevision();
    }
}// namespace mikoto::renderer
