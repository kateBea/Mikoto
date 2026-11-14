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


#ifndef MIKOTO_RENDERER_COMPATIBILITY_RENDERER_GEOMETRY_MODULE_HH
#define MIKOTO_RENDERER_COMPATIBILITY_RENDERER_GEOMETRY_MODULE_HH

#include <EASTL/vector.h>

#include <Renderer/Rhi/Pipeline.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Rhi/CommandList.hh>

#include <Renderer/CompatibilityRenderer/Passes/CompatibilityMaterialModule.hh>

namespace mikoto::scene {
    class Camera;
    class Scene;
}

namespace mikoto::renderer {
    struct CompatibilityEnvironment;

    /** Gathers visible meshes and records their compatibility raster passes. */
    class CompatibilityGeometryModule {
    public:
        auto Initialize( rhi::IGpuDevice* device ) -> void;
        auto Prepare( const scene::Scene& scene, const scene::Camera& camera, core::u32 hoverID ) -> void;
        auto Record(
            rhi::ICommandList* commands,
            const CompatibilityEnvironment& environment,
            rhi::IBuffer& frameUniform,
            const rhi::TextureHandle& shadowMap ) -> void;
        auto RecordSelection(
            rhi::ICommandList* commands,
            const CompatibilityEnvironment& environment,
            rhi::IBuffer& frameUniform ) -> void;
        auto RecordShadows(
            rhi::ICommandList* commands,
            const CompatibilityEnvironment& environment,
            rhi::IBuffer& frameUniform ) -> void;
        auto ClearDraws() -> void;
        MKT_NODISCARD auto GetDrawCount() const -> core::usize { return mOpaque.size() + mTransparent.size(); }
        MKT_NODISCARD auto HasHighlights() const -> bool { return mHasHighlights; }

    private:
        struct Draw {
            rhi::BufferHandle mVertices{};
            rhi::BufferHandle mIndices{};
            const material::SurfaceMaterial* mSurface{};
            core::float4x4 mWorld{ 1.0f };
            core::float4x4 mNormalMatrix{ 1.0f };
            core::u32 mID{};
            bool mHighlighted{};
            core::f32 mDepth{};
        };

        auto RecordDraw(
            rhi::ICommandList* commands,
            const Draw& draw,
            const CompatibilityEnvironment& environment,
            rhi::IBuffer& frameUniform, const rhi::TextureHandle& shadowMap,
            bool transparentPass = false ) -> void;

        CompatibilityMaterialModule mMaterials{};
        rhi::PipelineHandle mPipeline{};
        rhi::PipelineHandle mTransparentPipeline{};
        rhi::PipelineHandle mSelectionPipeline{};
        rhi::PipelineHandle mShadowPipeline{};
        eastl::vector<Draw> mOpaque{};
        eastl::vector<Draw> mTransparent{};
        bool mHasHighlights{};
    };
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_COMPATIBILITY_RENDERER_GEOMETRY_MODULE_HH
