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

#ifndef MIKOTO_RENDERER_CORE_FRAME_SCHEDULER_HH
#define MIKOTO_RENDERER_CORE_FRAME_SCHEDULER_HH

#include <EASTL/hash_map.h>
#include <EASTL/unique_ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Rhi/Fence.hh>
#include <Renderer/Rhi/Buffer.hh>
#include <Renderer/Rhi/Texture.hh>
#include <Renderer/Rhi/AccelerationStructure.hh>

namespace mikoto::asset {
    class ShaderLibrary;
}

namespace mikoto::scene {
    class Scene;
}

namespace mikoto::renderer::rhi {
    class IGpuDevice;
}

namespace mikoto::renderer {

    struct FrameSchedulerCreateInfo {
        rhi::IGpuDevice* mDevice{};
        asset::ShaderLibrary* mShaderLibrary{};
    };

    struct ScenePreparationRequirements {
        // A later view may request acceleration data after raster preparation.
        bool mRayTracing{};
    };

    /** Persistent scene resources shared by views on the scheduler's device. */
    struct SceneRenderResources {
        rhi::BufferHandle mAttributes{};
        rhi::BufferHandle mIndices{};
        rhi::BufferHandle mGeometryAllocations{};

        rhi::BufferHandle mInstances{};
        rhi::BufferHandle mMaterials{};
        rhi::BufferHandle mLights{};

        rhi::BufferHandle mSkinningPalettes{};
        rhi::BufferHandle mSkinnedPositions{};
        rhi::BufferHandle mSkinnedNormals{};

        rhi::AccelStructureHandle mTopLevel{};

        rhi::TextureHandle mEnvironmentIrradiance{};
        rhi::TextureHandle mEnvironmentPrefilter{};
    };

    /** Read-only publication of a scene's common preparation for one frame. */
    struct SceneFrameState {
        const scene::Scene* mScene{};
        core::u64 mFrameIndex{};
        core::u64 mRevision{}; // Content changes, independent of the frame counter.

        SceneRenderResources mResources{};

        core::u32 mInstanceCount{};
        core::u32 mMaterialCount{};
        core::u32 mLightCount{};
        bool mRayTracingPrepared{};

        // Preparation may have queued GPU work. Consumers order their submissions
        // after this point; a published state does not imply GPU completion.
        rhi::FenceHandle mReadyFence{};
        core::u64 mReadyFenceValue{};
    };

    // Ownership references: Wicked keeps geometry/material/skinning buffers on Scene,
    // while RenderPath3D owns camera visibility and render targets. Filament separately
    // exposes scene preparation and camera-dependent visible/light preparation.
    // This is a Mikoto API sketch, not a port of their scheduling implementations.
    // https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/wiScene.h
    // https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/wiRenderPath3D.h
    // https://github.com/google/filament/blob/main/filament/src/details/Scene.h

    /**
     * Prepares common scene data before any renderer records its view.
     *
     * The future owner supplies one scheduler per device to all scene renderers.
     * Cameras, culling results, draw lists, targets and temporal histories remain
     * view-owned. View-specific material overrides are applied after shared extraction.
     * Queue submission stays with FGScheduler/RenderContext.
     *
     * Sketch only: preparation currently publishes no state and allocates no GPU resources.
     */
    class FrameScheduler final : public core::INonCopyable, public core::INonMoveable {
    public:
        explicit FrameScheduler( const FrameSchedulerCreateInfo& createInfo );
        ~FrameScheduler();

        /** Called once after simulation/world transforms are synchronized.
         * All views use the same monotonically increasing frame index.
         */
        auto BeginFrame( core::u64 frameIndex ) -> void;

        /** Renderer entry point: ensure this scene's common data is prepared.
         * Repeated requests reuse base preparation within the frame. A later
         * ray-tracing request adds missing work without repeating extraction/skinning.
         * Returns nullptr until the requested preparation can be published.
         */
        MKT_NODISCARD auto PrepareScene( const scene::Scene& scene,
            ScenePreparationRequirements requirements = {} ) -> const SceneFrameState*;

        /** Queries already published state for the current frame without preparing it. */
        MKT_NODISCARD auto FindSceneState( const scene::Scene& scene ) const -> const SceneFrameState*;

        /** Ends CPU consumption of published state; persistent scene allocations survive.
         * Borrowed SceneFrameState pointers expire here. GPU readers retain their resources.
         */
        auto EndFrame() -> void;

        /** Notify before destroying a scene, preventing reuse of its cache identity.
         * Future retirement must account for every submitted view's GPU readers.
         */
        auto ReleaseScene( const scene::Scene& scene ) -> void;

        MKT_NODISCARD auto GetFrameIndex() const -> core::u64;

    private:
        struct SceneEntry;

        // Borrowed dependencies. Destroy the scheduler after GPU users finish and
        // before the device/library. Scene pointers are keys, never ownership.
        rhi::IGpuDevice* mDevice{};
        asset::ShaderLibrary* mShaderLibrary{};
        eastl::hash_map<const scene::Scene*, eastl::unique_ptr<SceneEntry>> mScenes{};

        core::u64 mFrameIndex{};
        bool mFrameActive{};
    };
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_CORE_FRAME_SCHEDULER_HH
