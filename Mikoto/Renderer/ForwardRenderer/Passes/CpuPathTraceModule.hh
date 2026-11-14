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

#ifndef MIKOTO_RENDERER_FORWARD_RENDERER_CPU_PATH_TRACE_MODULE_HH
#define MIKOTO_RENDERER_FORWARD_RENDERER_CPU_PATH_TRACE_MODULE_HH

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Core/FrameGraph.hh>

// Moving RT in One Weekend to the GPU

// The original sphere scene, rendered once either in full or over multiple tiles.
namespace mikoto::renderer {

    struct CpuPathTraceModuleInfo {
        FGTextureHandle mColorImage{};
        FGBufferHandle mReadbackBuffer{};
        FGBufferHandle mSpheres{};

        FGPipelineHandle mComputePipeline{};
    };

    class CpuPathTraceModule final {
    public:
        explicit CpuPathTraceModule( rhi::RenderResolution resolution );

        auto RegisterPathTracerCompute( FrameGraph& graph ) -> void;

        // Pause/resume the compute example.
        // A fullscreen graphics variant is not implemented.
        auto SetUseComputeApproach( bool enable ) -> void;

        // Full-image dispatch by default. When enabled, render one 64x64 tile per
        // execution after clearing the image. Readback waits for the whole image.
        // Changing mode does not restart a completed render.
        auto SetRenderPerTile( bool enable ) -> void;

    private:
        rhi::RenderResolution mResolution{ rhi::RenderResolution::e1080P };

        core::u32 mNextTile{};
        
        bool mSceneUploaded{};
        bool mImageCleared{};
        bool mTraceRecorded{};
        bool mReadbackPending{};
        bool mRenderPerTile{};
        
        bool mUseComputeApproach{ true };
    };

}// namespace mikoto::renderer

#endif//MIKOTO_RENDERER_FORWARD_RENDERER_CPU_PATH_TRACE_MODULE_HH
