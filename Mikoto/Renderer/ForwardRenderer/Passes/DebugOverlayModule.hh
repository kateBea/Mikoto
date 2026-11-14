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

#ifndef MIKOTO_RENDERER_FORWARD_RENDERER_DEBUG_OVERLAY_MODULE_HH
#define MIKOTO_RENDERER_FORWARD_RENDERER_DEBUG_OVERLAY_MODULE_HH

#include <EASTL/vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Physics/PhysicsWorld.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryCullModule.hh>

namespace mikoto::renderer {

    struct DebugVertex {
        core::float4 mPosition{};
        core::float4 mColor{};
    };

    struct DebugLine {
        DebugVertex mFrom{};
        DebugVertex mTo{};
    };

    struct SceneVolumeSettings {
        bool mShowBounds{};
        bool mShowCameraFrustums{};
        bool mShowPointLightRanges{};
        bool mShowSpotLightRanges{};
        bool mSelectedOnly{ true };
        core::u64 mSelectedEntity{};
        core::float2 mGameViewport{};
        float mLineWidth{ 2.0f };
    };

    class DebugOverlayModule final {
    public:
        explicit DebugOverlayModule( rhi::RenderResolution resolution );

        auto SetMultisampling( rhi::Multisampling multisampling ) -> void;

        auto SetScene( const scene::Scene *scene ) -> void;
        auto SetCamera( const scene::Camera *camera ) -> void;

        auto SetGeometryManager( GeometryCullModule &geom ) -> void;

        auto RegisterPhysicsDebugger( FrameGraph &graph ) -> void;
        auto RegisterSceneVolumes( FrameGraph &graph ) -> void;

        auto GetSceneVolumeSettings() -> SceneVolumeSettings&;
        auto GetPhysicsSettings() -> physics::PhysicsDebugDrawSettings&;

        MKT_NODISCARD auto GetPhysicsLineCount() const -> core::u32;
        MKT_NODISCARD auto GetDroppedPhysicsLineCount() const -> core::usize;

    private:
        auto GatherSceneVolumes() -> void;

    private:
        const scene::Scene *mScene{};
        const scene::Camera *mCamera{};

        GeometryCullModule *mGeometryCullModule{};

        static constexpr core::usize kMaxVolumeLines{ 64'000 };
        SceneVolumeSettings mSceneVolumeSettings{};
        eastl::vector<DebugLine> mVolumeLines{};
        FGBufferHandle mVolumeLinesBuffer{};

        FGBufferHandle mPhysicsLines{};
        core::u32 mPhysicsLineCount{};
        core::usize mDroppedPhysicsLines{};

        physics::PhysicsDebugDrawSettings mPhysicsSettings{};

        rhi::RenderResolution mResolution{ rhi::RenderResolution::e1080P };
    };
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_FORWARD_RENDERER_DEBUG_OVERLAY_MODULE_HH
