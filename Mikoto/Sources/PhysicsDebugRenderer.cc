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

#ifndef JPH_DEBUG_RENDERER
    #error This file should only be included when JPH_DEBUG_RENDERER is defined
#endif // JPH_DEBUG_RENDERER

// Jolt needs to be included before
#include <Jolt/Jolt.h>
#include <Jolt/Renderer/DebugRenderer.h>
#include <Jolt/Renderer/DebugRendererSimple.h>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

// TODO: Command context needed because incomplete class
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/CommandContext.hh>

#include <Renderer/Core/PhysicsDebugRenderer.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::asset;
    using namespace mikoto::filesystem;
    using namespace mikoto::renderer::rhi;

    auto PhysicsDebugRendererCreateInfo::SetName( eastl::string_view name ) -> PhysicsDebugRendererCreateInfo & {
        mName = name;
        return *this;
    }

    auto PhysicsDebugRendererCreateInfo::SetDevice( IGpuDevice *device ) -> PhysicsDebugRendererCreateInfo & {
        mDevice = device;
        return *this;
    }

    auto PhysicsDebugRendererCreateInfo::SetShaderBasePath( eastl::string_view path ) -> PhysicsDebugRendererCreateInfo & {
        mShaderBasePath = path;
        return *this;
    }

    auto PhysicsDebugRendererCreateInfo::SetRenderResolution( RenderResolution resolution ) -> PhysicsDebugRendererCreateInfo & {
        mResolution = resolution;
        return *this;
    }

    PhysicsDebugRenderer::PhysicsDebugRenderer( const PhysicsDebugRendererCreateInfo& desc  )
        : mDevice{ desc.mDevice }
    {

    }

    auto PhysicsDebugRenderer::Init() -> void {
        DebugRenderer::Initialize();

        // Temporary, as the Direct3D 11 backend does not offer support for
        // bindless which the frame graph relies on for most of its functionality
        if (mDevice->IsGraphicsApi(GraphicsAPI::eD3D11) || mDevice->IsGraphicsApi( GraphicsAPI::eD3D12 )) {
            MKT_CORE_LOGGER_WARN( "Scene renderer expects Vulkan" );
            return;
        }

        const ShaderLibraryDescription description{
            .mDevice = mDevice,
            .mRootPath{ "Resources/Shaders/slang" } };
        mShaderLibrary = eastl::make_unique<ShaderLibrary>( description );

        if (mShaderLibrary) {
            mShaderLibrary->Initialize();
        }

        mFrameGraph = FrameGraph::Create( mDevice, mShaderLibrary.get() );

        InitPasses();
    }

    auto PhysicsDebugRenderer::Shutdown() -> void {
        if (mShaderLibrary) {
            mShaderLibrary->Shutdown();
            mShaderLibrary.reset();
        }
    }

    auto PhysicsDebugRenderer::Render( const scene::Scene* scene ) -> void {
        // Temporary, as the Direct3D 11 backend does not offer support for
        // bindless which the frame graph relies on for most of its functionality
        if (mDevice->IsGraphicsApi(GraphicsAPI::eD3D11) || mDevice->IsGraphicsApi( GraphicsAPI::eD3D12 )) {
            return;
        }
    }

    auto PhysicsDebugRenderer::DrawLine(
            JPH::RVec3Arg inFrom,
            JPH::RVec3Arg inTo,
            JPH::ColorArg inColor ) -> void {
        MKT_CORE_LOGGER_TRACE( "PhysicsDebugRenderer::DrawLine" );
    }

    auto PhysicsDebugRenderer::DrawTriangle(
        JPH::RVec3Arg inV1,
        JPH::RVec3Arg inV2,
        JPH::RVec3Arg inV3,
        JPH::ColorArg inColor,
        ECastShadow inCastShadow ) -> void {
        MKT_CORE_LOGGER_TRACE( "PhysicsDebugRenderer::DrawTriangle" );
    }

    auto PhysicsDebugRenderer::CreateTriangleBatch(
        const Triangle* inTriangles,
        int inTriangleCount ) -> Batch {
        MKT_CORE_LOGGER_TRACE( "PhysicsDebugRenderer::CreateTriangleBatch" );
        return {};
    }

    auto PhysicsDebugRenderer::CreateTriangleBatch(
        const Vertex* inVertices,
        int inVertexCount,
        const JPH::uint32* inIndices,
        int inIndexCount ) -> Batch {
        MKT_CORE_LOGGER_TRACE( "PhysicsDebugRenderer::CreateTriangleBatch" );
        return {};
    }

    auto PhysicsDebugRenderer::DrawGeometry(
        JPH::RMat44Arg inModelMatrix,
        const JPH::AABox& inWorldSpaceBounds,
        float inLODScaleSq,
        JPH::ColorArg inModelColor,
        const GeometryRef& inGeometry,
        ECullMode inCullMode,
        ECastShadow inCastShadow,
        EDrawMode inDrawMode ) -> void {
        MKT_CORE_LOGGER_TRACE( "PhysicsDebugRenderer::DrawGeometry" );
    }

    auto PhysicsDebugRenderer::DrawText3D(
        JPH::RVec3Arg inPosition,
        const std::string_view& inString,
        JPH::ColorArg inColor, float inHeight ) -> void {
        MKT_CORE_LOGGER_TRACE( "PhysicsDebugRenderer::DrawText3D" );
    }

    auto PhysicsDebugRenderer::InitPasses() -> void {

    }

    auto PhysicsDebugRenderer::Create( const PhysicsDebugRendererCreateInfo& spec ) -> eastl::unique_ptr<PhysicsDebugRenderer> {
        return eastl::make_unique<PhysicsDebugRenderer>( spec );
    }
}// namespace mikoto::renderer