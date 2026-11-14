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

#ifndef MIKOTO_RENDERER_COMPATIBILITY_RENDERER_SELECTION_MODULE_HH
#define MIKOTO_RENDERER_COMPATIBILITY_RENDERER_SELECTION_MODULE_HH

#include <EASTL/optional.h>
#include <EASTL/vector.h>

#include <Renderer/CompatibilityRenderer/CompatibilityPassResources.hh>
#include <Renderer/ForwardRenderer/Passes/MousePickingModule.hh>

namespace mikoto::renderer {
    class CompatibilityGeometryModule;
    struct CompatibilityEnvironment;

    /** Renders visible entity IDs and the selection mask for editor tools. */
    class CompatibilitySelectionModule {
    public:
        auto Initialize( rhi::IGpuDevice& device ) -> void;
        auto Cancel() -> void;
        MKT_NODISCARD auto Request( const ReadPixelViewportInfo& viewport, rhi::Extent2D size ) -> bool;
        MKT_NODISCARD auto TakeResult() -> eastl::optional<core::u32>;
        MKT_NODISCARD auto ReadRectangle( const ReadPixelViewportInfo& first, const ReadPixelViewportInfo& last ) const -> eastl::vector<core::u32>;
        MKT_NODISCARD auto NeedsPass() const -> bool { return mPending.has_value(); }
        MKT_NODISCARD auto GetImage() const -> const rhi::TextureHandle& { return mImage; }

        auto Record( rhi::ICommandList& commands, rhi::Extent2D size, CompatibilityGeometryModule& geometry,
            const CompatibilityEnvironment& environment, rhi::IBuffer& frameUniform ) -> void;
        MKT_NODISCARD auto RecordOutline( rhi::ICommandList& commands, rhi::TextureHandle sceneImage, const core::float4& color,
            core::f32 width, bool outputForPreview ) -> rhi::TextureHandle;

    private:
        auto ReadRegion( core::uint2 offset, rhi::Extent2D size, eastl::span<core::uint2> pixels ) const -> bool;

        rhi::IGpuDevice* mDevice{};
        rhi::TextureHandle mImage{};
        rhi::TextureHandle mDepth{};
        rhi::TextureHandle mOutlineImage{};
        CompatibilityFullscreenPass mOutline{};
        eastl::optional<ReadPixelViewportInfo> mPending{};
        bool mRecorded{};
    };
}

#endif// MIKOTO_RENDERER_COMPATIBILITY_RENDERER_SELECTION_MODULE_HH
