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

#include <EASTL/vector.h>

#include <Renderer/Core/FrameScheduler.hh>

namespace mikoto::renderer {

    struct FrameScheduler::SceneEntry {
        SceneFrameState mPublished{};
        bool mPrepared{};

        // Keep BLAS ownership with the shared geometry cache, not individual views.
        // Future upload caches and allocation revisions also belong to this entry.
        eastl::vector<rhi::AccelStructureHandle> mBottomLevels{};
    };

    FrameScheduler::FrameScheduler( const FrameSchedulerCreateInfo& createInfo )
        : mDevice{ createInfo.mDevice },
          mShaderLibrary{ createInfo.mShaderLibrary } {
    }

    FrameScheduler::~FrameScheduler() = default;

    auto FrameScheduler::BeginFrame( core::u64 frameIndex ) -> void {
        mFrameIndex = frameIndex;
        mFrameActive = true;
    }

    auto FrameScheduler::PrepareScene( const scene::Scene& scene,
        ScenePreparationRequirements requirements ) -> const SceneFrameState* {
        if ( !mFrameActive ) {
            return nullptr;
        }

        if ( const auto* prepared{ FindSceneState( scene ) };
            prepared && ( !requirements.mRayTracing || prepared->mRayTracingPrepared ) ) {
            return prepared;
        }

        // TODO: Find/create one persistent entry for this scene. Extract synchronized
        // components once per frame; never advance Scene::Update from this path.

        // TODO: Stage changed geometry/material/instance/light records and schedule
        // shared GPU skinning. Reuse existing storage instead of allocating per view.

        // TODO: If requested, add missing BLAS/TLAS work once for this frame.
        // An upgrade must preserve resources already consumed by earlier views.

        // TODO: Publish the frame/revision/counts and GPU readiness dependency only
        // after recording preparation. Keep the previous generation alive in flight.
        // View graphs import RHI handles and create their own descriptor bindings;
        // graph-local handles and bindless indices cannot be shared as scene identity.
        return nullptr;
    }

    auto FrameScheduler::FindSceneState( const scene::Scene& scene ) const -> const SceneFrameState* {
        if ( !mFrameActive ) {
            return nullptr;
        }

        const auto found{ mScenes.find( &scene ) };

        if ( found == mScenes.end() || !found->second->mPrepared ||
             found->second->mPublished.mFrameIndex != mFrameIndex ) {
            return nullptr;
        }

        return &found->second->mPublished;
    }

    auto FrameScheduler::EndFrame() -> void {
        // Do not discard scene GPU resources when the last view finishes recording.
        mFrameActive = false;
    }

    auto FrameScheduler::ReleaseScene( const scene::Scene& ) -> void {
        // TODO: Remove this scene from future queries and retire its resources only
        // after all in-flight views finish. Do not erase live GPU storage immediately.
    }

    auto FrameScheduler::GetFrameIndex() const -> core::u64 {
        return mFrameIndex;
    }
}// namespace mikoto::renderer
