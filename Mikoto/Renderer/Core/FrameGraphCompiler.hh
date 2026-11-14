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

#ifndef MIKOTO_RENDERER_FRAME_GRAPH_COMPILER_HH
#define MIKOTO_RENDERER_FRAME_GRAPH_COMPILER_HH

#include <EASTL/utility.h>
#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/fixed_vector.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/ReferenceCounted.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Core/FrameGraphTypes.hh>

namespace mikoto::renderer {

    // Forward declarations
    class FrameGraph;
    struct FGNodeControl;
    class FGResourceManager;

    struct FGBarrier {
        FGResourceHandle mResourceID{};

        // Source states depend on the passes that execute and are resolved when recording.
        eastl::fixed_vector<FGPipelineStage, 2> mNewStages{};
        FGResourceAccess mNewAccess{};
    };

    struct FGExecutionPlan : public core::ReferenceCounted<FGExecutionPlan> {
        // Pass names in execution order
        eastl::vector<eastl::string> mSortedPasses{};

        // [passName] -> [resourceHandle] -> [resourceName, barrier]
        ankerl::unordered_dense::map<eastl::string,
            ankerl::unordered_dense::map<FGResourceHandle, eastl::pair<eastl::string, FGBarrier>>> mBarriers{};
    };

    class FGCompiler final {
    public:
        explicit FGCompiler( rhi::IGpuDevice* device );

        /** Builds the execution plan and joins creation of dirty GPU resources. */
        MKT_NODISCARD auto Compile( FrameGraph& graph ) -> core::Ref<FGExecutionPlan>;

    private:
        auto BuildNodeEdges() -> void;
        auto CullGraphNodes() -> void;
        auto BuildNodeBarriers() -> void;
        auto BuildResources() -> void;

        // Builds the pass order; native recording contexts remain graph runtime state.
        auto BuildExecutionContext() -> void;

    private:
        rhi::IGpuDevice* mDevice{ nullptr };
        FGNodeControl* mNodeControl{};
        FGResourceManager* mResources{};
        FrameGraph* mGraph{};
        core::Ref<FGExecutionPlan> mExecutionPlan{};
    };

}

#endif//MIKOTO_RENDERER_FRAME_GRAPH_COMPILER_HH
