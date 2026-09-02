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

    // https://jrouwe.github.io/JoltPhysics/?utm_source=chatgpt.com

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

        auto Render() -> void override;

        auto SetCameraPos(JPH::RVec3Arg inCameraPos) -> void;

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

        JPH::RVec3 mCameraPos{};

        eastl::unique_ptr<FrameGraph> mFrameGraph{};
        eastl::unique_ptr<asset::ShaderLibrary> mShaderLibrary{};
    };


    // Simplified physics debug renderer
    struct PhysicsDebugRendererSimpleCreateInfo {
        rhi::IGpuDevice* mDevice{};
        eastl::string_view mName{};
        eastl::string_view mShaderBasePath{};

        rhi::RenderResolution mResolution{ rhi::RenderResolution::e1080P };

        auto SetName( eastl::string_view name ) -> PhysicsDebugRendererSimpleCreateInfo&;
        auto SetDevice( rhi::IGpuDevice* device ) -> PhysicsDebugRendererSimpleCreateInfo&;
        auto SetShaderBasePath( eastl::string_view path ) -> PhysicsDebugRendererSimpleCreateInfo&;
        auto SetRenderResolution( rhi::RenderResolution resolution ) -> PhysicsDebugRendererSimpleCreateInfo&;
    };

    struct DebugVertex {
        core::float4 mPosition{};
        core::float4 mColor{};
    };

    struct DebugLine {
        DebugVertex mFrom{};
        DebugVertex mTo{};
    };

    struct DebugTriangle {
        DebugVertex mV1{};
        DebugVertex mV2{};
        DebugVertex mV3{};
    };

    struct DebugText {
        core::float4 mPosition{};
        core::float4 mColor{};
        core::f32 mHeight{};
        eastl::string mText{};
    };

    class PhysicsDebugRendererSimple final : public IRenderer, public JPH::DebugRendererSimple  {
    public:
        explicit PhysicsDebugRendererSimple( const PhysicsDebugRendererSimpleCreateInfo& spec );

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto Render() -> void override;

        auto SetCamera( const scene::Camera* camera ) -> void;

        auto DrawLine(
            JPH::RVec3Arg inFrom,
            JPH::RVec3Arg inTo,
            JPH::ColorArg inColor ) -> void override;

        auto DrawText3D(
            JPH::RVec3Arg inPosition,
            const std::string_view& inString,
            JPH::ColorArg inColor, float inHeight ) -> void override;

        auto DrawTriangle(
            JPH::RVec3Arg inV1,
            JPH::RVec3Arg inV2,
            JPH::RVec3Arg inV3,
            JPH::ColorArg inColor,
            ECastShadow inCastShadow ) -> void override;

        auto DisplayImGuiWindowLines( bool& open ) -> void;
        auto DisplayImGuiWindowTriangles( bool& open ) -> void;

        MKT_NODISCARD static auto Create( const PhysicsDebugRendererSimpleCreateInfo& spec ) -> eastl::unique_ptr<PhysicsDebugRendererSimple>;

    private:
        // [Internal usage]
        auto InitSimpleDrawPasses() -> void;

        auto RenderLines() -> void;
        auto RenderTexts() -> void;
        auto RenderTriangles() -> void;

    private:
        static constexpr core::usize kMaxVerticesLines{ 64'000 };
        static constexpr core::usize kMaxVerticesTriangles{ 64'000 };

        rhi::IGpuDevice* mDevice{};

        const scene::Camera* mCamera{};

        eastl::vector<DebugLine> mLines{};
        eastl::vector<DebugText> mTexts{};
        eastl::vector<DebugTriangle> mTriangles{};

        bool mIsImguiWindowActiveLines{};
        bool mIsImguiWindowActiveTriangles{};

        renderer::rhi::BufferHandle mLinesBuffer{};
        renderer::rhi::BufferHandle mTrianglesBuffer{};

        renderer::rhi::TextureHandle mColorImageTriangles{};
        renderer::rhi::TextureHandle mDepthImageTriangles{};

        renderer::rhi::TextureHandle mColorImageLines{};
        renderer::rhi::TextureHandle mDepthImageLines{};

        renderer::rhi::ShaderModuleHandle mVertexShader{};
        renderer::rhi::ShaderModuleHandle mPixelShader{};

        renderer::rhi::CommandListHandle mCommandList{};

        renderer::rhi::PipelineHandle mPipelineLines{};
        renderer::rhi::PipelineHandle mPipelineTriangles{};
        renderer::rhi::BindingSetHandle mBindingSetLinesHandle{};
        renderer::rhi::BindingSetHandle mBindingSetTrianglesHandle{};
        renderer::rhi::BindingLayoutHandle mBindingLayoutHandle{};
        renderer::rhi::PipelineLayoutHandle mPipelineLayoutHandle{};
    };


    // Debug renderer playback


    // Debug renderer recorder
}// namespace Mikoto

#endif //MIKOTO_PHYSICS_DEBUG_RENDERER_HH
