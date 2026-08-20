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

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Passes/GeometryCullModule.hh>

namespace mikoto::renderer {

    struct MousePickingModuleInfo {
        FGTextureHandle mColorImage{};
        FGBufferHandle mReadBackBuffer{};

        FGPipelineHandle mPipeline{};
    };

    struct ReadRegion {
        core::f32 mX{};
        core::f32 mY{};
        core::f32 mWidth{};
        core::f32 mHeight{};
    };

    struct ReadPixelViewportInfo {
        core::f32 mX{};
        core::f32 mY{};

        core::f32 mViewportX{};
        core::f32 mViewportY{};
        core::f32 mViewportWidth{};
        core::f32 mViewportHeight{};
    };

    class MousePickingModule {
    public:
        explicit MousePickingModule( rhi::RenderResolution resolution );

        auto RegisterPasses( FrameGraph& graph ) -> void;

        // For optimization purposes we can specify which region gets copied every frame
        auto SetReadRegion( core::f32 x, core::f32 y, core::f32 width, core::f32 height ) -> void;

        MKT_NODISCARD auto ReadPixel( core::u32 x, core::u32 y ) const -> core::u32;
        MKT_NODISCARD auto ReadPixel( const ReadPixelViewportInfo& viewport ) const -> core::u32;

        auto SetGeometryManager( GeometryCullModule& geom ) -> void;

    private:
        auto RegisterSelectionBuffer( FrameGraph& graph ) -> void;

    private:
        GeometryCullModule* mGeometryCullModule{};
        rhi::RenderResolution mResolution{ rhi::RenderResolution::e1080P };

        ReadRegion mReadRegion{};
        eastl::vector<u32> mData{};
    };

}// namespace mikoto::renderer

#endif//MIKOTO_MOUSE_PICKING_MODULE_HH
