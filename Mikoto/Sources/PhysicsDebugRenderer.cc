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
#include <Renderer/Core/RenderSystem.hh>

#include <Renderer/Core/PhysicsDebugRenderer.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::asset;
    using namespace mikoto::filesystem;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;


    MKT_NODISCARD static auto ToDebugColor(JPH::ColorArg color) -> core::float4 {
        constexpr f32 inv255{ 1.0f / 255.0f };

        return {
            static_cast<f32>(color.r) * inv255,
            static_cast<f32>(color.g) * inv255,
            static_cast<f32>(color.b) * inv255,
            static_cast<f32>(color.a) * inv255
        };
    }

    MKT_NODISCARD static auto ToDebugVertex(
            JPH::RVec3Arg position,
            JPH::ColorArg color ) -> DebugVertex {
        return {
            .mPosition = {
                    static_cast<float>( position.GetX() ),
                    static_cast<float>( position.GetY() ),
                    static_cast<float>( position.GetZ() ),
                    {} },
            .mColor = ToDebugColor(color)
        };
    }

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

    auto PhysicsDebugRenderer::SetCameraPos( JPH::RVec3Arg inCameraPos ) -> void {
        mCameraPos = inCameraPos;
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


    auto PhysicsDebugRendererSimpleCreateInfo::SetName( eastl::string_view name ) -> PhysicsDebugRendererSimpleCreateInfo & {
        mName = name;
        return *this;
    }

    auto PhysicsDebugRendererSimpleCreateInfo::SetDevice( IGpuDevice *device ) -> PhysicsDebugRendererSimpleCreateInfo & {
        mDevice = device;
        return *this;
    }

    auto PhysicsDebugRendererSimpleCreateInfo::SetShaderBasePath( eastl::string_view path ) -> PhysicsDebugRendererSimpleCreateInfo & {
        mShaderBasePath = path;
        return *this;
    }

    auto PhysicsDebugRendererSimpleCreateInfo::SetRenderResolution( RenderResolution resolution ) -> PhysicsDebugRendererSimpleCreateInfo & {
        mResolution = resolution;
        return *this;
    }

    PhysicsDebugRendererSimple::PhysicsDebugRendererSimple( const PhysicsDebugRendererSimpleCreateInfo& desc  )
        : mDevice{ desc.mDevice }
    {

    }

    auto PhysicsDebugRendererSimple::Init() -> void {
        DebugRenderer::Initialize();

        const ShaderLibraryDescription description{
            .mDevice = mDevice,
            .mRootPath{ "Resources/Shaders/slang" } };

        InitPasses();

        InitSimpleDrawPasses();
    }

    auto PhysicsDebugRendererSimple::Shutdown() -> void {
        mDevice->WaitIdle();

        mPipelineLines.Release();
        mPipelineTriangles.Release();

        mPipelineLayoutHandle.Release();
        mBindingLayoutHandle.Release();

        mVertexShader.Release();
        mPixelShader.Release();

        mColorImageTriangles.Release();
        mDepthImageTriangles.Release();

        mColorImageLines.Release();
        mDepthImageLines.Release();

        mBindingSetLinesHandle.Release();
        mBindingSetTrianglesHandle.Release();

        mCommandList.Release();

        mLinesBuffer.Release();
        mTrianglesBuffer.Release();
    }

    auto PhysicsDebugRendererSimple::Render( const scene::Scene* scene ) -> void {
        mCommandList->Begin( { .mScopeName = "PhysicsDebugRendererSimple Render" } );

        if (!mLines.empty()) {
            RenderLines();
        }

        if (!mTriangles.empty()) {
            RenderTriangles();
        }

        mCommandList->SetTransition( mColorImageLines.GetRaw(), ResourceStates::eShaderResource );
        mCommandList->SetTransition( mColorImageTriangles.GetRaw(), ResourceStates::eShaderResource );
        mCommandList->End();

        auto submitInfo{ SubmitInfo{}
            .AddCommandList( mCommandList ) };
        RenderSystem::Get()->BatchSubmission(eastl::move(submitInfo), QueueType::eGraphics);
    }

    auto PhysicsDebugRendererSimple::SetCamera( const scene::Camera* camera ) -> void {
        mCamera = camera;
    }

    auto PhysicsDebugRendererSimple::DrawLine(
            JPH::RVec3Arg inFrom,
            JPH::RVec3Arg inTo,
            JPH::ColorArg inColor ) -> void {
        mLines.push_back( {
            .mFrom = ToDebugVertex( inFrom, inColor ),
            .mTo = ToDebugVertex( inTo, inColor ) } );
    }

    auto PhysicsDebugRendererSimple::DrawTriangle(
        JPH::RVec3Arg inV1,
        JPH::RVec3Arg inV2,
        JPH::RVec3Arg inV3,
        JPH::ColorArg inColor,
        ECastShadow inCastShadow ) -> void {
        mTriangles.push_back( {
            .mV1 = ToDebugVertex( inV1, inColor ),
            .mV2 = ToDebugVertex( inV2, inColor ),
            .mV3 = ToDebugVertex( inV3, inColor ) } );
    }

    auto PhysicsDebugRendererSimple::DrawText3D(
        JPH::RVec3Arg inPosition,
        const std::string_view& inString,
        JPH::ColorArg inColor, float inHeight ) -> void {
        mTexts.push_back( {
            .mPosition = {
                static_cast<float>( inPosition.GetX() ),
                static_cast<float>( inPosition.GetY() ),
                static_cast<float>( inPosition.GetZ() ),
                0.0f },
            .mColor = ToDebugColor( inColor ),
            .mHeight = inHeight,
            .mText = { inString.data(), inString.size() } } );
    }

    auto PhysicsDebugRendererSimple::InitPasses() -> void {

    }

    auto PhysicsDebugRendererSimple::InitSimpleDrawPasses() -> void {
        auto verticesDesc{ BufferCreateDescription{}
            .SetBufferUsage( BufferUsageFlagsBits::kStorage | BufferUsageFlagsBits::kCopyDst )
            .SetHeapType( HeapType::eDeviceLocal )
            .ForElement( MKT_SIZEOF( DebugLine ), 1000 )
            .SetCpuAccessType( CpuAccessType::eRead ) };
        mLinesBuffer = mDevice->CreateBuffer( verticesDesc );

        auto indicesDesc{ BufferCreateDescription{}
            .SetBufferUsage( BufferUsageFlagsBits::kStorage | BufferUsageFlagsBits::kCopyDst )
            .SetHeapType( HeapType::eDeviceLocal )
            .ForElement( MKT_SIZEOF( DebugTriangle ), 1000 )
            .SetCpuAccessType( CpuAccessType::eRead ) };
        mTrianglesBuffer = mDevice->CreateBuffer( indicesDesc );

        // Create color attachment
        auto colorDesc{ TextureCreateDescription{}
            .SetWidth( as<i32>( 1920 ) )
            .SetHeight( as<i32>( 1080 ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kShaderResource )
            .SetFormat( Format::eBGRA8_UNORM ) };

        mColorImageTriangles = mDevice->CreateTexture( colorDesc );
        mColorImageTriangles->SetDebugName( "PhysicsDebugRendererSimple Color image Triangles" );

        mColorImageLines = mDevice->CreateTexture( colorDesc );
        mColorImageLines->SetDebugName( "PhysicsDebugRendererSimple Color image Triangles" );

        // Create depth attachment
        auto depthDesc{ TextureCreateDescription{}
            .SetWidth( as<i32>( 1920 ) )
            .SetHeight( as<i32>( 1080 ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kDepthTarget )
            .SetFormat( Format::eD32 ) };

        mDepthImageTriangles = mDevice->CreateTexture( depthDesc );
        mDepthImageTriangles->SetDebugName( "PhysicsDebugRendererSimple Depth image Triangles" );

        mDepthImageLines = mDevice->CreateTexture( depthDesc );
        mDepthImageLines->SetDebugName( "PhysicsDebugRendererSimple Depth image Lines" );

        // Create shaders
        FileHandle vsShader{ FileService::Get()->LoadFile( "Resources/Shaders/slang/PhysicsDebugSimple_Vert.spv" ) };
        auto vertexShaderDescription{ ShaderModuleCreateDescription{}
            .SetContents( vsShader )
            .SetModuleName( vsShader->GetName() )
            .SetModulePath( vsShader->GetPath() )
            .SetLanguage( ShaderLanguage::eSPIRV )
            .SetStage( ShaderType::eVertex ) };
        mVertexShader = mDevice->CreateShader( vertexShaderDescription );

        FileHandle pxShader{ FileService::Get()->LoadFile( "Resources/Shaders/slang/PhysicsDebugSimple_Frag.spv" ) };
        auto fragmentShaderDescription{ ShaderModuleCreateDescription{}
            .SetContents( pxShader )
            .SetModuleName( pxShader->GetName() )
            .SetModulePath( pxShader->GetPath() )
            .SetLanguage( ShaderLanguage::eSPIRV )
            .SetStage( ShaderType::ePixel ) };
        mPixelShader = mDevice->CreateShader( fragmentShaderDescription );

        // Create the command list
        mCommandList = mDevice->CreateCommandList( QueueType::eGraphics );
        mCommandList->SetEnableAutomaticBarriers( true );
        mCommandList->SetDebugName( "PhysicsDebugRendererSimple CommandList" );

        // We will upload a texture and a buffer to do some effects, see Triangle_Frag
        // Ideally we want to automate this process by allowing each backend to be able to use shader reflection
        auto layoutDesc{ BindingLayoutDescription{}
            .SetRegisterSpace( 0 )
            .SetShaderVisibility(ShaderFlagsBits::kAll)
            .AddItem(BindingLayoutItem::StructuredBuffer_SRV(0)) };
        mBindingLayoutHandle = mDevice->CreateBindingLayout(layoutDesc);

        mPipelineLayoutHandle = mDevice->CreatePipelineLayout( PipelineLayoutCreateDescription{}
            .AddBindingLayout( mBindingLayoutHandle ) );

        auto linesGraphicsPipelineDescription{ GraphicsPipelineDescription{}
            .AddShader( mPixelShader )
            .AddShader( mVertexShader )
            .SetDepthFormat( Format::eD32 )
            .AddColorFormat( Format::eBGRA8_UNORM )
            .SetPolygonMode( PolygonMode::eFill )
            .SetWindingOrder( WindingOrder::eCounterClockwise )
            .SetTopology( PrimitiveTopology::eLineList )
            .SetPipelineLayout( mPipelineLayoutHandle ) };
        mPipelineLines = mDevice->CreatePipeline( linesGraphicsPipelineDescription );
        mPipelineLines->SetDebugName( "PhysicsDebugRendererSimple Lines Pipeline" );

        auto trianglesPipelineDescription{
            GraphicsPipelineDescription{ linesGraphicsPipelineDescription }
            .SetPolygonMode( PolygonMode::eFill )
            .SetTopology( PrimitiveTopology::eTriangleList ) };
        mPipelineTriangles = mDevice->CreatePipeline( trianglesPipelineDescription );
        mPipelineTriangles->SetDebugName( "PhysicsDebugRendererSimple Triangles Pipeline" );

        auto bindingSetLinesDesc{ BindingSetDescription{}
            .AddItem( BindingSetItem::StructuredBuffer_SRV( 0, mLinesBuffer.GetRaw() ) ) };
        mBindingSetLinesHandle = mDevice->CreateBindingSet( bindingSetLinesDesc, mBindingLayoutHandle );

        auto bindingSetTrianglesDesc{ BindingSetDescription{}
            .AddItem( BindingSetItem::StructuredBuffer_SRV( 0, mTrianglesBuffer.GetRaw() ) ) };
        mBindingSetTrianglesHandle = mDevice->CreateBindingSet( bindingSetTrianglesDesc, mBindingLayoutHandle );
    }

    auto PhysicsDebugRendererSimple::RenderLines() -> void {
        mCommandList->Write( mLinesBuffer.GetRaw(), mLines.data(), MKT_VECTOR_SIZE_BYTES( mLines ) );

        eastl::array<ubyte, kMaxPushConstantSize> ps{};
        struct CameraData {
            float4x4 mViewProjection{};
        } params {
            .mViewProjection = mCamera->GetProjection() * mCamera->GetViewMatrix() };
        std::memcpy( ps.data(), MKT_ADDRESSOF( params ), MKT_SIZEOF( params ) );
        mCommandList->SetPushConstants( mPipelineLayoutHandle.GetRaw(), ps.data(), kMaxPushConstantSize, ShaderFlagsBits::kAll );

        // Set graphics state
        auto graphicsState{ GraphicsState{}
            .SetRenderArea( Rect{ 1920, 1080 } )
            .AddDepthTarget( mDepthImageLines )
            .AddRenderTarget( mColorImageLines, Color{ 1.0f, 0.2f, 0.4f, 1.0f } ) };
        mCommandList->BeginRendering( graphicsState );

        auto bindingDescription{ BindResourcesDescription{}
            .SetBindPoint( PipelineType::eGraphics )
            .SetPipelineLayout( mPipelineLayoutHandle.GetRaw() )
            .AddResourceSet( 0, mBindingSetLinesHandle.GetRaw() ) };
        mCommandList->BindPipelineResources( bindingDescription );

        mCommandList->BindPipeline( mPipelineLines.GetRaw() );

        mCommandList->SetViewportState( ViewportState{}
            .AddViewportAndScissorRect( Viewport( 1920, 1080 ) ) );

        const auto drawArguments{ DrawArguments{}
            .SetInstanceCount( 1 )
            .SetVertexCount( as<u32>(mLines.size()) ) };
        mCommandList->Draw( drawArguments );

        mCommandList->EndRendering();

        mLines.clear();
    }

    auto PhysicsDebugRendererSimple::RenderTexts() -> void {
        mTexts.clear();
    }

    auto PhysicsDebugRendererSimple::RenderTriangles() -> void {
        mCommandList->Write( mTrianglesBuffer.GetRaw(), mTriangles.data(), MKT_VECTOR_SIZE_BYTES( mTriangles ) );

        eastl::array<ubyte, kMaxPushConstantSize> ps{};
        struct CameraData {
            float4x4 mViewProjection{};
        } params {
            .mViewProjection = mCamera->GetProjection() * mCamera->GetViewMatrix() };
        std::memcpy( ps.data(), MKT_ADDRESSOF( params ), MKT_SIZEOF( params ) );
        mCommandList->SetPushConstants( mPipelineLayoutHandle.GetRaw(), ps.data(), kMaxPushConstantSize, ShaderFlagsBits::kAll );

        // Set graphics state
        auto graphicsState{ GraphicsState{}
            .SetRenderArea( Rect{ 1920, 1080 } )
            .AddDepthTarget( mDepthImageTriangles )
            .AddRenderTarget( mColorImageTriangles, Color{ 1.0f, 0.2f, 0.4f, 1.0f } ) };
        mCommandList->BeginRendering( graphicsState );

        auto bindingDescription{ BindResourcesDescription{}
            .SetBindPoint( PipelineType::eGraphics )
            .SetPipelineLayout( mPipelineLayoutHandle.GetRaw() )
            .AddResourceSet( 0, mBindingSetTrianglesHandle.GetRaw() ) };
        mCommandList->BindPipelineResources( bindingDescription );

        mCommandList->BindPipeline( mPipelineTriangles.GetRaw() );

        mCommandList->SetViewportState( ViewportState{}
            .AddViewportAndScissorRect( Viewport( 1920, 1080 ) ) );

        const auto drawArguments{ DrawArguments{}
            .SetInstanceCount( 1 )
            .SetVertexCount( as<u32>(mTriangles.size()) * 3 ) };
        mCommandList->Draw( drawArguments );

        mCommandList->EndRendering();

        mTriangles.clear();
    }

    auto PhysicsDebugRendererSimple::DisplayImGuiWindowTriangles(bool &open) -> void {
        ImGui::SetNextWindowSize( ImVec2( 420.0f, 500.0f ), ImGuiCond_FirstUseEver );

        if ( ImGui::Begin( "Physics Debug Triangles", &open  ) ) {
            auto imageID{ ImGuiService::Get()->GetTextureID( mColorImageTriangles ) };
            ImGui::Image( imageID, ImVec2{ 1280, 720 } );

            ImGui::Spacing();
            ImGui::Separator();

            ImGui::Text( "FPS: %.1f", ImGui::GetIO().Framerate );
            ImGui::Text( "Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate );

            ImGui::Spacing();
            ImGui::Separator();

            static f32 rotationSpeed{ 1.0f };
            static eastl::array<f32, 4> clearColor{ 0.10f, 0.10f, 0.12f, 1.0f };

            ImGui::SliderFloat( "Rotation Speed", &rotationSpeed, 0.0f, 10.0f );
            ImGui::ColorEdit4( "Clear Color", clearColor.data() );

            ImGui::Spacing();
            ImGui::Separator();
        }

        ImGui::End();
    }

    auto PhysicsDebugRendererSimple::DisplayImGuiWindowLines( bool& open  ) -> void {
        ImGui::SetNextWindowSize( ImVec2( 420.0f, 500.0f ), ImGuiCond_FirstUseEver );

        if ( ImGui::Begin( "Physics Debug Lines", &open ) ) {
            auto imageID{ ImGuiService::Get()->GetTextureID( mColorImageLines ) };
            ImGui::Image( imageID, ImVec2{ 1280, 720 } );

            ImGui::Spacing();
            ImGui::Separator();

            ImGui::Text( "FPS: %.1f", ImGui::GetIO().Framerate );
            ImGui::Text( "Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate );

            ImGui::Spacing();
            ImGui::Separator();

            static f32 rotationSpeed{ 1.0f };
            static eastl::array<f32, 4> clearColor{ 0.10f, 0.10f, 0.12f, 1.0f };

            ImGui::SliderFloat( "Rotation Speed", &rotationSpeed, 0.0f, 10.0f );
            ImGui::ColorEdit4( "Clear Color", clearColor.data() );

            ImGui::Spacing();
            ImGui::Separator();
        }

        ImGui::End();
    }

    auto PhysicsDebugRendererSimple::Create( const PhysicsDebugRendererSimpleCreateInfo& spec ) -> eastl::unique_ptr<PhysicsDebugRendererSimple> {
        return eastl::make_unique<PhysicsDebugRendererSimple>( spec );
    }
}// namespace mikoto::renderer