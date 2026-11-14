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

#ifndef MIKOTO_RENDERER_FORWARD_RENDERER_MOUSE_PICKING_MODULE_HH
#define MIKOTO_RENDERER_FORWARD_RENDERER_MOUSE_PICKING_MODULE_HH

#include <EASTL/optional.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryCullModule.hh>

namespace mikoto::renderer {

    struct MousePickingModuleInfo {
        FGTextureHandle mColorImage{};
        FGTextureHandle mDepthImage{};
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

    /** A coordinate query against the independently depth-tested entity ID image.
     * The inherited revision identifies the current request/cancellation state.
     * Asynchronous readbacks publish only while that revision still matches.
     */
    class MousePickingModule final : public core::RevisionTracked {
    public:
        explicit MousePickingModule( rhi::RenderResolution resolution );

        auto RegisterSelectionBuffer( FrameGraph& graph ) -> void;

        MKT_NODISCARD auto ReadPixel( core::u32 x, core::u32 y ) const -> core::u32;
        MKT_NODISCARD auto ReadPixel( const ReadPixelViewportInfo& viewport ) const -> core::u32;

        auto SetGeometryManager( GeometryCullModule& geom ) -> void;
        auto RequestPixel( const ReadPixelViewportInfo& viewport ) -> bool;
        auto TakePickResult() -> eastl::optional<core::u32>;
        /** Unique visible entity IDs in the last completed ID-buffer readback. */
        auto ReadRectangle( const ReadPixelViewportInfo& first, const ReadPixelViewportInfo& last ) const -> eastl::vector<core::u32>;
        auto Invalidate() -> void;
        auto CancelPendingPick() -> void;

    private:
        struct PickingPixel {
            core::u32 mX{};
            core::u32 mY{};
        };

        MKT_NODISCARD static auto MapPickingPixel( const ReadPixelViewportInfo& viewport,
            core::u32 width, core::u32 height ) -> eastl::optional<PickingPixel>;

    private:
        GeometryCullModule* mGeometryCullModule{};
        rhi::RenderResolution mResolution{ rhi::RenderResolution::e1080P };

        ReadRegion mReadRegion{};
        eastl::vector<core::u32> mData{};
        eastl::optional<core::u32> mPickResult{};
        eastl::optional<PickingPixel> mQueuedPick{};

        bool mReadbackInFlight{};
        bool mHasReadback{};
    };

}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_FORWARD_RENDERER_MOUSE_PICKING_MODULE_HH

