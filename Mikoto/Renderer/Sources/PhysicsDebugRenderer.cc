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

#if defined( JPH_DEBUG_RENDERER )

// Jolt needs to be included before
#include <Jolt/Jolt.h>
#include <Jolt/Renderer/DebugRenderer.h>
#include <Jolt/Renderer/DebugRendererSimple.h>

#include <Core/Profiler.hh>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

#include <Assets/AssetRegistryService.hh>

#include <Renderer/Rhi/Utility.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/RenderSystem.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Core/PhysicsDebugRenderer.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::asset;
    using namespace mikoto::filesystem;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;


    MKT_NODISCARD static auto ToDebugColor( JPH::ColorArg color ) -> core::float4 {
        MKT_BEGIN_PROFILER();
        constexpr core::f32 inv255{ 1.0f / 255.0f };

        return {
            static_cast<core::f32>( color.r ) * inv255,
            static_cast<core::f32>( color.g ) * inv255,
            static_cast<core::f32>( color.b ) * inv255,
            static_cast<core::f32>( color.a ) * inv255
        };
    }

    MKT_NODISCARD static auto ToDebugVertex(
        JPH::RVec3Arg position,
        JPH::ColorArg color ) -> DebugVertex {
        MKT_BEGIN_PROFILER();
        return {
            .mPosition = {
                static_cast<float>( position.GetX() ),
                static_cast<float>( position.GetY() ),
                static_cast<float>( position.GetZ() ),
                {} },
            .mColor = ToDebugColor( color )
        };
    }

    auto PhysicsDebugRendererCreateInfo::SetName( eastl::string_view name ) -> PhysicsDebugRendererCreateInfo& {
        MKT_BEGIN_PROFILER();
        mName = name;
        return *this;
    }

    auto PhysicsDebugRendererCreateInfo::SetDevice( renderer::rhi::IGpuDevice* device ) -> PhysicsDebugRendererCreateInfo& {
        MKT_BEGIN_PROFILER();
        mDevice = device;
        return *this;
    }

    auto PhysicsDebugRendererCreateInfo::SetShaderBasePath( eastl::string_view path ) -> PhysicsDebugRendererCreateInfo& {
        MKT_BEGIN_PROFILER();
        mShaderBasePath = path;
        return *this;
    }

    auto PhysicsDebugRendererCreateInfo::SetRenderResolution( renderer::rhi::RenderResolution resolution ) -> PhysicsDebugRendererCreateInfo& {
        MKT_BEGIN_PROFILER();
        mResolution = resolution;
        return *this;
    }

    PhysicsDebugRenderer::PhysicsDebugRenderer( const PhysicsDebugRendererCreateInfo& desc )
        : mDevice{ desc.mDevice } {
    }

    auto PhysicsDebugRenderer::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        DebugRenderer::Initialize();

        // Temporary, as the Direct3D 11 backend does not offer support for
        // bindless which the frame graph relies on for most of its functionality
        if ( mDevice->IsGraphicsApi( renderer::rhi::GraphicsAPI::eD3D11 ) || mDevice->IsGraphicsApi( renderer::rhi::GraphicsAPI::eD3D12 ) ) {
            MKT_CORE_LOGGER_WARN( "Scene renderer expects Vulkan" );
            return;
        }

        const asset::ShaderLibraryDescription description{
            .mDevice = mDevice,
                .mRootPath{ "Resources/Shaders/slang" }
        };
        mShaderLibrary = eastl::make_unique<asset::ShaderLibrary>( description );

        if ( mShaderLibrary ) {
            mShaderLibrary->Initialize();
        }

        mFrameGraph = FrameGraph::Create( mDevice, mShaderLibrary.get() );

        InitPasses();
    }

    auto PhysicsDebugRenderer::Shutdown() -> void {
        MKT_BEGIN_PROFILER();

        if ( mShaderLibrary ) {
            mShaderLibrary->Shutdown();
            mShaderLibrary.reset();
        }
    }

    auto PhysicsDebugRenderer::Render() -> void {
        MKT_BEGIN_PROFILER();
        // Temporary, as the Direct3D 11 backend does not offer support for
        // bindless which the frame graph relies on for most of its functionality
        if ( mDevice->IsGraphicsApi( renderer::rhi::GraphicsAPI::eD3D11 ) || mDevice->IsGraphicsApi( renderer::rhi::GraphicsAPI::eD3D12 ) ) {
            return;
        }
    }

    auto PhysicsDebugRenderer::SetCameraPos( JPH::RVec3Arg inCameraPos ) -> void {
        MKT_BEGIN_PROFILER();
        mCameraPos = inCameraPos;
    }

    auto PhysicsDebugRenderer::DrawLine(
        JPH::RVec3Arg inFrom,
        JPH::RVec3Arg inTo,
        JPH::ColorArg inColor ) -> void {
        MKT_BEGIN_PROFILER();
        MKT_CORE_LOGGER_TRACE( "PhysicsDebugRenderer::DrawLine" );
    }

    auto PhysicsDebugRenderer::DrawTriangle(
        JPH::RVec3Arg inV1,
        JPH::RVec3Arg inV2,
        JPH::RVec3Arg inV3,
        JPH::ColorArg inColor,
        ECastShadow inCastShadow ) -> void {
        MKT_BEGIN_PROFILER();
        MKT_CORE_LOGGER_TRACE( "PhysicsDebugRenderer::DrawTriangle" );
    }

    auto PhysicsDebugRenderer::CreateTriangleBatch(
        const Triangle* inTriangles,
        int inTriangleCount ) -> Batch {
        MKT_BEGIN_PROFILER();
        MKT_CORE_LOGGER_TRACE( "PhysicsDebugRenderer::CreateTriangleBatch" );
        return {};
    }

    auto PhysicsDebugRenderer::CreateTriangleBatch(
        const Vertex* inVertices,
        int inVertexCount,
        const JPH::uint32* inIndices,
        int inIndexCount ) -> Batch {
        MKT_BEGIN_PROFILER();
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
        MKT_BEGIN_PROFILER();
        MKT_CORE_LOGGER_TRACE( "PhysicsDebugRenderer::DrawGeometry" );
    }

    auto PhysicsDebugRenderer::DrawText3D(
        JPH::RVec3Arg inPosition,
        const std::string_view& inString,
        JPH::ColorArg inColor, float inHeight ) -> void {
        MKT_BEGIN_PROFILER();
        MKT_CORE_LOGGER_TRACE( "PhysicsDebugRenderer::DrawText3D" );
    }

    auto PhysicsDebugRenderer::InitPasses() -> void {
    }

    auto PhysicsDebugRenderer::Create( const PhysicsDebugRendererCreateInfo& spec ) -> eastl::unique_ptr<PhysicsDebugRenderer> {
        MKT_BEGIN_PROFILER();
        return eastl::make_unique<PhysicsDebugRenderer>( spec );
    }


    auto PhysicsDebugRendererSimpleCreateInfo::SetName( eastl::string_view name ) -> PhysicsDebugRendererSimpleCreateInfo& {
        MKT_BEGIN_PROFILER();
        mName = name;
        return *this;
    }

    auto PhysicsDebugRendererSimpleCreateInfo::SetDevice( renderer::rhi::IGpuDevice* device ) -> PhysicsDebugRendererSimpleCreateInfo& {
        MKT_BEGIN_PROFILER();
        mDevice = device;
        return *this;
    }

    auto PhysicsDebugRendererSimpleCreateInfo::SetShaderBasePath( eastl::string_view path ) -> PhysicsDebugRendererSimpleCreateInfo& {
        MKT_BEGIN_PROFILER();
        mShaderBasePath = path;
        return *this;
    }

    auto PhysicsDebugRendererSimpleCreateInfo::SetRenderResolution( renderer::rhi::RenderResolution resolution ) -> PhysicsDebugRendererSimpleCreateInfo& {
        MKT_BEGIN_PROFILER();
        mResolution = resolution;
        return *this;
    }

    PhysicsDebugRendererSimple::PhysicsDebugRendererSimple( const PhysicsDebugRendererSimpleCreateInfo& desc )
        : mDevice{ desc.mDevice } {
        MKT_BEGIN_PROFILER();
        const auto [width, height]{ InferDimensions( desc.mResolution ) };
        mRenderWidth = core::as<core::u32>( width );
        mRenderHeight = core::as<core::u32>( height );
    }

    auto PhysicsDebugRendererSimple::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        DebugRenderer::Initialize();

        if ( !mDevice || !mDevice->IsGraphicsApi( renderer::rhi::GraphicsAPI::eVulkan ) ) {
            MKT_CORE_LOGGER_WARN( "PhysicsDebugRendererSimple expects Vulkan" );
            return;
        }

        InitSimpleDrawPasses();
        mIsSupported = true;
    }

    auto PhysicsDebugRendererSimple::Shutdown() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mIsSupported ) {
            return;
        }

        mDevice->WaitIdle();
        mGraph.reset();
        mColorImageTriangles.Reset();
        mColorImageLines.Reset();
        mIsSupported = false;
    }

    auto PhysicsDebugRendererSimple::Render() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mIsSupported || !mCamera ) {
            return;
        }

        mGraph->Execute();
        RenderTexts();
    }

    auto PhysicsDebugRendererSimple::SetCamera( const scene::Camera* camera ) -> void {
        MKT_BEGIN_PROFILER();
        mCamera = camera;
    }

    auto PhysicsDebugRendererSimple::BeginFrame() -> void {
        MKT_BEGIN_PROFILER();
        mLines.clear();
        mTriangles.clear();
        mTexts.clear();
        mDroppedLines = 0;
    }

    auto PhysicsDebugRendererSimple::DrawLine(
        JPH::RVec3Arg inFrom,
        JPH::RVec3Arg inTo,
        JPH::ColorArg inColor ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mLines.size() >= kMaxLines ) {
            ++mDroppedLines;
            return;
        }

        mLines.push_back( { .mFrom = ToDebugVertex( inFrom, inColor ),
            .mTo = ToDebugVertex( inTo, inColor ) } );
    }

    auto PhysicsDebugRendererSimple::DrawTriangle(
        JPH::RVec3Arg inV1,
        JPH::RVec3Arg inV2,
        JPH::RVec3Arg inV3,
        JPH::ColorArg inColor,
        ECastShadow inCastShadow ) -> void {
        MKT_BEGIN_PROFILER();
        // Constraint limits and arrow heads can emit triangles even in wireframe mode.
        DrawLine( inV1, inV2, inColor );
        DrawLine( inV2, inV3, inColor );
        DrawLine( inV3, inV1, inColor );

        if ( mTriangles.size() == kMaxVerticesTriangles ) {
            return;
        }

        mTriangles.push_back( { .mV1 = ToDebugVertex( inV1, inColor ),
            .mV2 = ToDebugVertex( inV2, inColor ),
            .mV3 = ToDebugVertex( inV3, inColor ) } );
    }

    auto PhysicsDebugRendererSimple::DrawText3D(
        JPH::RVec3Arg inPosition,
        const std::string_view& inString,
        JPH::ColorArg inColor, float inHeight ) -> void {
        MKT_BEGIN_PROFILER();
        mTexts.push_back( { .mPosition = {
                                static_cast<float>( inPosition.GetX() ),
                                static_cast<float>( inPosition.GetY() ),
                                static_cast<float>( inPosition.GetZ() ),
                                0.0f },
                                    .mColor = ToDebugColor( inColor ),
                                    .mHeight = inHeight,
                                    .mText = { inString.data(), inString.size() } } );
    }

    auto PhysicsDebugRendererSimple::InitSimpleDrawPasses() -> void {
        MKT_BEGIN_PROFILER();
        mGraph = FrameGraph::Create( mDevice, asset::AssetRegistryService::Get()->GetShaderLibrary() );
        mLinesBuffer = mGraph->Create( FGBufferDescription{}
            .SetName( "PhysicsDebug.Lines" )
            .SetUsage( rhi::BufferUsageFlagsBits::Structured | rhi::BufferUsageFlagsBits::CopyDest )
            .SetSizeBytes( sizeof( DebugLine ) * kMaxVerticesLines ) );
        mTrianglesBuffer = mGraph->Create( FGBufferDescription{}
            .SetName( "PhysicsDebug.Triangles" )
            .SetUsage( rhi::BufferUsageFlagsBits::Structured | rhi::BufferUsageFlagsBits::CopyDest )
            .SetSizeBytes( sizeof( DebugTriangle ) * kMaxVerticesTriangles ) );

        const auto color{ [&]( const char* name ) {
            return mGraph->Create( FGTextureDescription{}
                .SetName( name )
                .SetWidth( mRenderWidth )
                .SetHeight( mRenderHeight )
                .SetFormat( rhi::Format::eBGRA8_UNORM )
                .SetUsage( rhi::TextureUsageFlagsBits::RenderTarget | rhi::TextureUsageFlagsBits::ShaderResource ) );
        } };
        const auto depth{ [&]( const char* name ) {
            return mGraph->Create( FGTextureDescription{}
                .SetName( name )
                .SetWidth( mRenderWidth )
                .SetHeight( mRenderHeight )
                .SetFormat( rhi::Format::eD32 )
                .SetUsage( rhi::TextureUsageFlagsBits::DepthTarget ) );
        } };
        mLinesColorTarget = color( "PhysicsDebug.LinesColor" );
        mTrianglesColorTarget = color( "PhysicsDebug.TrianglesColor" );
        mLinesDepthTarget = depth( "PhysicsDebug.LinesDepth" );
        mTrianglesDepthTarget = depth( "PhysicsDebug.TrianglesDepth" );

        auto pipeline{ FGPipelineDescription{}
            .SetName( "PhysicsDebug.LinesPipeline" )
            .SetPipelineType( rhi::PipelineType::eGraphics )
            .SetTopology( rhi::PrimitiveTopology::eLineList )
            .SetCullMode( rhi::CullMode::eNone )
            .SetDepthTest( true )
            .SetDepthWrite( true )
            .SetBlendEnable( true )
            .SetDepthFormat( rhi::Format::eD32 )
            .AddColorFormat( rhi::Format::eBGRA8_UNORM )
            .PushShader( "PhysicsDebugSimple_Vert.slang", FGStageType::eVertex )
            .PushShader( "PhysicsDebugSimple_Frag.slang", FGStageType::ePixel ) };
        mPipelineLines = mGraph->Create( pipeline );
        mPipelineTriangles = mGraph->Create( pipeline
            .SetName( "PhysicsDebug.TrianglesPipeline" )
            .SetTopology( rhi::PrimitiveTopology::eTriangleList ) );

        mGraph->RegisterPass(
            "PhysicsDebug.Upload",
            FGPassType::eTransfer,
            [this]( FGNodeBuilder& builder, core::Blackboard& ) {
                builder.UseResource( mLinesBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                builder.UseResource( mTrianglesBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& context, core::Blackboard& ) {
                MKT_ASSERT( mLines.size() <= kMaxVerticesLines && mTriangles.size() <= kMaxVerticesTriangles, "Exceeded debug buffer capacity" );

                if ( !mLines.empty() ) {
                    context.CopyBuffer( mLinesBuffer, 0, mLines.data(), MKT_VECTOR_SIZE_BYTES( mLines ) );
                }

                if ( !mTriangles.empty() ) {
                    context.CopyBuffer( mTrianglesBuffer, 0, mTriangles.data(), MKT_VECTOR_SIZE_BYTES( mTriangles ) );
                }
            } );

        mGraph->RegisterPass(
            "PhysicsDebug.RenderLines",
            FGPassType::eGraphics,
            [this]( FGNodeBuilder& builder, core::Blackboard& ) {
                builder.UseResource( mLinesBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( mLinesColorTarget, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                builder.UseResource( mLinesDepthTarget, FGPipelineStage::eDepthTarget, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& context, core::Blackboard& ) {
                RenderLines( context );
            } );
        mGraph->RegisterPass(
            "PhysicsDebug.RenderTriangles",
            FGPassType::eGraphics,
            [this]( FGNodeBuilder& builder, core::Blackboard& ) {
                builder.UseResource( mTrianglesBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( mTrianglesColorTarget, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                builder.UseResource( mTrianglesDepthTarget, FGPipelineStage::eDepthTarget, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& context, core::Blackboard& ) {
                RenderTriangles( context );
            } );

        // The editor samples both previews after this graph has finished.
        mGraph->RegisterPass(
            "PhysicsDebug.SampledOutput",
            FGPassType::eGraphics,
            [this]( FGNodeBuilder& builder, core::Blackboard& ) {
                builder.UseResource( mLinesColorTarget, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( mTrianglesColorTarget, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
            },
            []( CommandContext&, core::Blackboard& ) {
            } );
        mGraph->Compile();
        mColorImageLines = mGraph->GetTexture( mLinesColorTarget );
        mColorImageTriangles = mGraph->GetTexture( mTrianglesColorTarget );
    }

    auto PhysicsDebugRendererSimple::RenderLines( CommandContext& context ) -> void {
        MKT_BEGIN_PROFILER();
        struct CameraData {
            core::float4x4 mViewProjection{};
            core::u64 mVertices{};
        } params{
            .mViewProjection = mCamera->GetProjection() * mCamera->GetViewMatrix(),
            .mVertices = context.GetDeviceBufferAddress( mLinesBuffer )
        };
        context.PushConstants( params );
        // Clear empty previews too, so stale shapes disappear when drawing stops.
        context.BeginRender( RenderBeginDescription{}
            .SetRenderArea( rhi::Rect{ core::as<core::i32>( mRenderWidth ), core::as<core::i32>( mRenderHeight ) } )
            .AddDepthTarget( mLinesDepthTarget )
            .AddRenderTarget( mLinesColorTarget, rhi::kColorBlack ) );
        context.SetViewportState( rhi::ViewportState{}
            .AddViewportAndScissorRect( Viewport( mRenderWidth, mRenderHeight ) ) );
        context.BindPipeline( mPipelineLines );

        if ( !mLines.empty() ) {
            context.Draw( core::as<core::u32>( mLines.size() ) * 2 );
        }

        context.EndRender();
        mLines.clear();
    }

    auto PhysicsDebugRendererSimple::RenderTexts() -> void {
        MKT_BEGIN_PROFILER();
        mTexts.clear();
    }

    auto PhysicsDebugRendererSimple::RenderTriangles( CommandContext& context ) -> void {
        MKT_BEGIN_PROFILER();
        struct CameraData {
            core::float4x4 mViewProjection{};
            core::u64 mVertices{};
        } params{
            .mViewProjection = mCamera->GetProjection() * mCamera->GetViewMatrix(),
            .mVertices = context.GetDeviceBufferAddress( mTrianglesBuffer )
        };
        context.PushConstants( params );
        // Clear empty previews too, so stale shapes disappear when drawing stops.
        context.BeginRender( RenderBeginDescription{}
            .SetRenderArea( rhi::Rect{ core::as<core::i32>( mRenderWidth ), core::as<core::i32>( mRenderHeight ) } )
            .AddDepthTarget( mTrianglesDepthTarget )
            .AddRenderTarget( mTrianglesColorTarget, rhi::kColorBlack ) );
        context.SetViewportState( rhi::ViewportState{}
            .AddViewportAndScissorRect( Viewport( mRenderWidth, mRenderHeight ) ) );
        context.BindPipeline( mPipelineTriangles );

        if ( !mTriangles.empty() ) {
            context.Draw( core::as<core::u32>( mTriangles.size() ) * 3 );
        }

        context.EndRender();
        mTriangles.clear();
    }

    auto PhysicsDebugRendererSimple::DisplayImGuiWindowTriangles( bool& open ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !mIsSupported ) {
            return;
        }

        ImGui::SetNextWindowSize( ImVec2( 420.0f, 500.0f ), ImGuiCond_FirstUseEver );

        if ( ImGui::Begin( "Physics Debug Triangles", &open ) ) {
            auto imageID{ imgui::ImGuiService::Get()->GetTextureID( mColorImageTriangles ) };
            ImGui::Image( imageID, ImVec2{ 1280, 720 } );

            ImGui::Spacing();
            ImGui::Separator();

            ImGui::Text( "FPS: %.1f", ImGui::GetIO().Framerate );
            ImGui::Text( "Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate );

            ImGui::Spacing();
            ImGui::Separator();

            static core::f32 rotationSpeed{ 1.0f };
            static eastl::array<core::f32, 4> clearColor{ 0.10f, 0.10f, 0.12f, 1.0f };

            ImGui::SliderFloat( "Rotation Speed", &rotationSpeed, 0.0f, 10.0f );
            ImGui::ColorEdit4( "Clear Color", clearColor.data() );

            ImGui::Spacing();
            ImGui::Separator();
        }

        ImGui::End();
    }

    auto PhysicsDebugRendererSimple::DisplayImGuiWindowLines( bool& open ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !mIsSupported ) {
            return;
        }

        ImGui::SetNextWindowSize( ImVec2( 420.0f, 500.0f ), ImGuiCond_FirstUseEver );

        if ( ImGui::Begin( "Physics Debug Lines", &open ) ) {
            auto imageID{ imgui::ImGuiService::Get()->GetTextureID( mColorImageLines ) };
            ImGui::Image( imageID, ImVec2{ 1280, 720 } );

            ImGui::Spacing();
            ImGui::Separator();

            ImGui::Text( "FPS: %.1f", ImGui::GetIO().Framerate );
            ImGui::Text( "Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate );

            ImGui::Spacing();
            ImGui::Separator();

            static core::f32 rotationSpeed{ 1.0f };
            static eastl::array<core::f32, 4> clearColor{ 0.10f, 0.10f, 0.12f, 1.0f };

            ImGui::SliderFloat( "Rotation Speed", &rotationSpeed, 0.0f, 10.0f );
            ImGui::ColorEdit4( "Clear Color", clearColor.data() );

            ImGui::Spacing();
            ImGui::Separator();
        }

        ImGui::End();
    }

    auto PhysicsDebugRendererSimple::Create( const PhysicsDebugRendererSimpleCreateInfo& spec ) -> eastl::unique_ptr<PhysicsDebugRendererSimple> {
        MKT_BEGIN_PROFILER();
        return eastl::make_unique<PhysicsDebugRendererSimple>( spec );
    }

    auto PhysicsDebugRendererSimple::GetLines() const -> const eastl::vector<DebugLine>& {
        MKT_BEGIN_PROFILER();
        return mLines;
    }

    auto PhysicsDebugRendererSimple::GetDroppedLineCount() const -> core::usize {
        MKT_BEGIN_PROFILER();
        return mDroppedLines;
    }
}
#endif
