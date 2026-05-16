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

#ifndef MIKOTO_MOUSE_PICKING_MODULE_HH
#define MIKOTO_MOUSE_PICKING_MODULE_HH

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Math/Random.hh>

#include <Renderer/Core/Rhi.hh>

#include <Renderer/Core/FrameGraph.hh>

#include <Renderer/Passes/GeometryCullModule.hh>

namespace mikoto::renderer {

    struct MousePickingModuleInfo {
        FGTextureHandle mColorImage{};
        FGBufferHandle mReadBackBuffer{};

        FGPipelineHandle mPipeline{};
    };

    class MousePickingModule {
    public:
        explicit MousePickingModule(rhi::RenderResolution resolution);

        auto RegisterPasses( FrameGraph& graph ) -> void;

        MKT_NODISCARD auto ReadPixel( u32 x, u32 y ) -> core::u32;

        auto SetGeometryManager( GeometryCullModule& geom ) -> void;

    private:
        auto RegisterSelectionBuffer( FrameGraph& graph ) -> void;

    private:
        GeometryCullModule* mGeometryCullModule{};
        rhi::RenderResolution mResolution{ rhi::RenderResolution::e1080P };

        eastl::vector<u32> mData{};
    };

}// namespace mikoto

#endif//MIKOTO_MOUSE_PICKING_MODULE_HH
