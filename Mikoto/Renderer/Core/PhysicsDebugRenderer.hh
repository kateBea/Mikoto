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

#ifndef MIKOTO_PHYSICS_DEBUG_RENDERER_HH
#define MIKOTO_PHYSICS_DEBUG_RENDERER_HH

#ifndef JPH_DEBUG_RENDERER
    #error This file should only be included when JPH_DEBUG_RENDERER is defined
#endif // JPH_DEBUG_RENDERER

// Jolt needs to be included before
#include <Jolt/Jolt.h>
#include <Jolt/Renderer/DebugRenderer.h>
#include <Jolt/Renderer/DebugRendererSimple.h>

#include <EASTL/string_view.h>
#include <EASTL/unique_ptr.h>

#include <Scene/Scene.hh>

#include <Assets/ShaderLibrary.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Core/Renderer.hh>
#include <Renderer/Core/FrameGraph.hh>

namespace mikoto::renderer {

    struct PhysicsDebugRendererCreateInfo {
        rhi::IGpuDevice* mDevice{};
        eastl::string_view mName{};
        eastl::string_view mShaderBasePath{};

        rhi::RenderResolution mResolution{ rhi::RenderResolution::e1080P };

        auto SetName( eastl::string_view name ) -> PhysicsDebugRendererCreateInfo&;
        auto SetDevice( rhi::IGpuDevice* device ) -> PhysicsDebugRendererCreateInfo&;
        auto SetShaderBasePath( eastl::string_view path ) -> PhysicsDebugRendererCreateInfo&;
        auto SetRenderResolution( rhi::RenderResolution resolution ) -> PhysicsDebugRendererCreateInfo&;
    };

    class PhysicsDebugRenderer final : public IRenderer, public JPH::DebugRenderer  {
    public:
        explicit PhysicsDebugRenderer( const PhysicsDebugRendererCreateInfo& spec );

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto Render( const scene::Scene* scene ) -> void override;

        auto DrawLine(
            JPH::RVec3Arg inFrom,
            JPH::RVec3Arg inTo,
            JPH::ColorArg inColor ) -> void override;

        auto DrawTriangle(
            JPH::RVec3Arg inV1,
            JPH::RVec3Arg inV2,
            JPH::RVec3Arg inV3,
            JPH::ColorArg inColor,
            ECastShadow inCastShadow ) -> void override;

        auto CreateTriangleBatch(
            const Triangle* inTriangles,
            int inTriangleCount ) -> Batch override;

        auto CreateTriangleBatch(
            const Vertex* inVertices,
            int inVertexCount,
            const JPH::uint32* inIndices,
            int inIndexCount ) -> Batch override;

        auto DrawGeometry(
            JPH::RMat44Arg inModelMatrix,
            const JPH::AABox& inWorldSpaceBounds,
            float inLODScaleSq,
            JPH::ColorArg inModelColor,
            const GeometryRef& inGeometry,
            ECullMode inCullMode,
            ECastShadow inCastShadow,
            EDrawMode inDrawMode ) -> void override;

        auto DrawText3D(
            JPH::RVec3Arg inPosition,
            const std::string_view& inString,
            JPH::ColorArg inColor, float inHeight ) -> void override;

        MKT_NODISCARD static auto Create( const PhysicsDebugRendererCreateInfo& spec ) -> eastl::unique_ptr<PhysicsDebugRenderer>;

    private:
        // [Internal usage]
        auto InitPasses() -> void;

    private:
        rhi::IGpuDevice* mDevice{};

        eastl::unique_ptr<FrameGraph> mFrameGraph{};
        eastl::unique_ptr<asset::ShaderLibrary> mShaderLibrary{};
    };
}// namespace Mikoto

#endif //MIKOTO_PHYSICS_DEBUG_RENDERER_HH
