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

#ifndef MIKOTO_RENDERER_SCENE_RENDER_STATE_HH
#define MIKOTO_RENDERER_SCENE_RENDER_STATE_HH

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Scene/Scene.hh>

namespace mikoto::renderer {

    /**
     * Persistent rendering cache for one scene, shared by its views.
     * FrameScheduler will update changed scene data here after simulation is synchronized.
     * Sketch only: stores scene identity and an invalidation revision; no GPU work yet.
     */
    // Ownership reference: Wicked keeps instance/material/skinning buffers on Scene.
    // https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/wiScene.h
    class SceneRenderState final : public core::INonCopyable, public core::INonMoveable, public core::RevisionTracked {
    public:
        explicit SceneRenderState( const scene::Scene& scene );

        // Future preparation compares this token with the last prepared revision.
        // Advancing frames alone does not invalidate unchanged scene data.
        auto Invalidate() -> void;

        MKT_NODISCARD auto GetScene() const -> const scene::Scene&;

        ~SceneRenderState();

    private:
        // Borrowed source; the scene must outlive this state and its views.
        const scene::Scene* mScene{};

        // TODO: Own persistent instance/material/light buffers, shared deformation
        // and acceleration data. Graph-local handles and bindings remain separate.
    };
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_SCENE_RENDER_STATE_HH
