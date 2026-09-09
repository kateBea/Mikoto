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

#include <EASTL/numeric_limits.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Event.hh>
#include <Core/CoreEvents.hh>
#include <Core/LayerStack.hh>
#include <Core/TimeService.hh>

#include <Assets/Image.hh>
#include <Assets/ImageProcessor.hh>

#include <Layers/EditorHelloCubeLayer.hh>

#include <Logging/Logger.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Core/RenderSystem.hh>

namespace mikoto::editor {

    using namespace mikoto::core;
    using namespace mikoto::asset;
    using namespace mikoto::scene;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;
    using namespace mikoto::filesystem;

    EditorHelloCubeLayer::EditorHelloCubeLayer( platform::Window *window )
        : ILayer{ "EditorHelloCubeLayer" }, mWindow{ window }
    {
        mDevice = RenderSystem::Get()->GetGpuDevice();
    }

    auto EditorHelloCubeLayer::OnCreate() -> void {
        // Construct geometry: Index and vertex buffer for the cube
        auto verticesDesc{ BufferCreateDescription{}
            .SetBufferUsage( BufferUsageFlagsBits::Vertex | BufferUsageFlagsBits::CopyDest )
            .SetHeapType( HeapType::eDeviceLocal )
            .SetCpuAccessType( AccessType::eRead )
            .SetInitialData( BufferSpanHandle::New( mVertices.data(), MKT_VECTOR_SIZE_BYTES(mVertices) ) ) };
        mVertexBuffer = mDevice->CreateBuffer( verticesDesc );

        // Create indices buffer
        auto indicesDesc{ BufferCreateDescription{}
            .SetBufferUsage( BufferUsageFlagsBits::Index | BufferUsageFlagsBits::CopyDest )
            .SetHeapType( HeapType::eDeviceLocal )
            .SetCpuAccessType( AccessType::eRead )
            .SetFormat( Format::eR32_UINT )
            .SetInitialData( BufferSpanHandle::New( mIndices.data(), MKT_VECTOR_SIZE_BYTES(mIndices) ) ) };
        mIndexBuffer = mDevice->CreateBuffer( indicesDesc );

        // Create color attachment
        auto colorDesc{ TextureCreateDescription{}
            .SetWidth( as<i32>( 1920 ) )
            .SetHeight( as<i32>( 1080 ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::RenderTarget | TextureUsageFlagsBits::ShaderResource )
            .SetFormat( Format::eBGRA8_UNORM ) };

        mColorImage = mDevice->CreateTexture( colorDesc );
        mColorImage->SetDebugName( "HelloCubeLayer Color image" );

        // Create depth attachment
        auto depthDesc{ TextureCreateDescription{}
            .SetWidth( as<i32>( 1920 ) )
            .SetHeight( as<i32>( 1080 ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::DepthTarget )
            .SetFormat( Format::eD32 ) };

        mDepthImage = mDevice->CreateTexture( depthDesc );
        mDepthImage->SetDebugName( "HelloCubeLayer Depth image" );

        // Create shaders
        FileHandle vsShader{ FileService::Get()->LoadFile( "Resources/Shaders/slang/HelloTriangleBasic_Vert.slang" ) };
        auto vertexShaderDescription{ ShaderModuleCreateDescription{}
            .SetContents( vsShader )
            .SetModuleName( vsShader->GetName() )
            .SetModulePath( vsShader->GetPath() )
            .SetLanguage( ShaderLanguage::eSlang )
            .SetStage( ShaderType::eVertex ) };
        mVertexShader = mDevice->CreateShader( vertexShaderDescription );

        FileHandle pxShader{ FileService::Get()->LoadFile( "Resources/Shaders/slang/HelloTriangleBasic_Frag.slang" ) };
        auto fragmentShaderDescription{ ShaderModuleCreateDescription{}
            .SetContents( pxShader )
            .SetModuleName( pxShader->GetName() )
            .SetModulePath( pxShader->GetPath() )
            .SetLanguage( ShaderLanguage::eSlang )
            .SetStage( ShaderType::ePixel ) };
        mPixelShader = mDevice->CreateShader( fragmentShaderDescription );

        // Create pipeline
        eastl::array<rhi::VertexBindingDescription, 1> bindings{
    rhi::VertexBindingDescription{}
            .SetBinding( 0 )
            .SetStride( sizeof( asset::VertexDescription_Std430Alignment ) )
            .SetInputRate( InputRate::ePerVertex ) };

        eastl::array<rhi::VertexAttributeDescription, 2> attributes{
    rhi::VertexAttributeDescription{}
            .SetName( "POSITION" )
            .SetLocation( 0 )
            .SetBinding( 0 )
            .SetFormat( rhi::Format::eRGB32_FLOAT )
            .SetOffset( offsetof( asset::VertexDescription_Std430Alignment, mPosition ) ),

            rhi::VertexAttributeDescription{}
            .SetName( "TEXCOORD" )
            .SetLocation( 1 )
            .SetBinding( 0 )
            .SetFormat( rhi::Format::eRG32_FLOAT )
            .SetOffset( offsetof( asset::VertexDescription_Std430Alignment, mUv0 ) ) };

        mVertexInputLayout = mDevice->CreateInputLayout( InputLayoutCreateDescription{}
            .SetBindings( bindings )
            .SetAttributes( attributes )
            .SetShader( mVertexShader ) );

        // Optional. Get the bindings ready to pass in the resources to the GPU
        asset::ImageHandle image{ asset::ProcessImage2D( "Resources/Textures/diffuse.jpg" ) };
        auto textureDbug{ TextureCreateDescription{}
            .SetImageData( image )
            .SetWidth( as<i32>( image->mWidth ) )
            .SetHeight( as<i32>( image->mHeight ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::ShaderResource )
            .SetFormat( Format::eRGBA8_UNORM ) };
        mSimpleTexture = mDevice->CreateTexture( textureDbug );

        auto constantBufferDesc{ BufferCreateDescription{}
            .SetCpuAccessType( AccessType::eWrite )
            .SetBufferUsage( BufferUsageFlagsBits::Constant | BufferUsageFlagsBits::CopyDest )
            .SetResourceType( ResourceType::eConstantBuffer )
            .SetByteSize(MKT_SIZEOF( MyData )) };
        mConstantBuffer = mDevice->CreateBuffer(constantBufferDesc);

        auto samplerDes{ SamplerCreateDescription{}
            .SetFilter( rhi::SamplerFilter::eNearest )
            .SetWrap( SamplerWrapMode::eRepeat )
            .SetBorderColor( kColorWhite ) };
        mSamplerState = mDevice->CreateSampler( samplerDes );

        // We will upload a texture and a buffer to do some effects, see Triangle_Frag
        // Ideally we want to automate this process by allowing each backend to be able to use shader reflection
        auto layoutDesc{ BindingLayoutDescription{}
            .SetRegisterSpace( 0 )
            .SetShaderVisibility(ShaderFlagsBits::All)
            .AddItem(BindingLayoutItem::Sampler(0))
            .AddItem(BindingLayoutItem::TextureSRV(1))
            .AddItem(BindingLayoutItem::ConstantBuffer(2)) };
        mBindingLayoutHandle = mDevice->CreateBindingLayout(layoutDesc);

        // A pipeline layout describes what kind of group of resources we can bind
        // To a specific bind point. We can bind resources for Compute pipelines or Graphics pipelines, etc
        // This is handy if we have too many pipelines that share same layout for group of resources
        // we can just bind the resources once for all subsequent draws as long as the pipelines use same layout.
        mPipelineLayoutHandle = mDevice->CreatePipelineLayout( PipelineLayoutCreateDescription{}
            .AddBindingLayout( mBindingLayoutHandle ) );

        auto graphicsPipelineDescription{ GraphicsPipelineDescription{}
            .AddShader( mPixelShader )
            .AddShader( mVertexShader )
            .SetInputLayout( mVertexInputLayout )

            .SetDepthFormat( Format::eD32 )
            .AddColorFormat( Format::eBGRA8_UNORM )

            .SetPolygonMode( PolygonMode::eFill )
            .SetWindingOrder( WindingOrder::eCounterClockwise )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .SetPipelineLayout( mPipelineLayoutHandle ) };
        mPipeline = mDevice->CreatePipeline( graphicsPipelineDescription );
        mPipeline->SetDebugName( "HelloCubeLayer Pipeline" );

        auto wireframePipelineDescription{
            GraphicsPipelineDescription{ graphicsPipelineDescription }
            .SetPolygonMode( PolygonMode::eLines ) };
        mPipelineWireframe = mDevice->CreatePipeline( wireframePipelineDescription );
        mPipelineWireframe->SetDebugName( "HelloCubeLayer PipelineWireframe" );

        auto bindingSetDesc{ BindingTableDescription{}
            .AddItem( BindingTableItem::Sampler( 0, mSamplerState.GetPtr() ) )
            .AddItem( BindingTableItem::TextureSRV( 1, mSimpleTexture.GetPtr() ) )
            .AddItem( BindingTableItem::ConstantBuffer( 2, mConstantBuffer.GetPtr() ) ) };
        mBindingTableHandle = mDevice->CreateBindingTable( bindingSetDesc, mBindingLayoutHandle );

        SceneCameraDescription cameraDescription{
            .mFov = 45.0,
            .mAspectRatio = as<float>( mWindow->GetWidth() ) / as<float>( mWindow->GetHeight() ),
            .mNearPlane = 0.1f,
            .mFarPlane = 3000.0f,
            .mWindow = mWindow };
        mEditorCamera = eastl::make_unique<SceneCamera>( cameraDescription );

                // Create the command list
        mCommandList = mDevice->CreateCommandList( QueueType::eGraphics );
        mCommandList->SetEnableAutomaticBarriers( true );
        mCommandList->SetDebugName( "HelloCubeLayer CommandList" );

        mCommandList->Begin( {} );

        // Resource barriers must be emitted before dynamic rendering begins.
        // BindVertexBuffer and BindIndexBuffer then see the already-correct
        // state while each mesh render scope is active.
        mCommandList->SetTransition( {mVertexBuffer.GetPtr(), ResourceStates::eVertexBuffer} );
        mCommandList->SetTransition( { mIndexBuffer.GetPtr(), ResourceStates::eIndexBuffer } );

        mCommandList->End();

        IQueue* graphicsQueue{ mDevice->GetQueue( QueueType::eGraphics ) };

        u64 kCompletionValue{ 0 };
        auto completionFence{ mDevice->CreateFence( kCompletionValue++ ) };
        if ( !completionFence.IsEmpty() ) {
            graphicsQueue->ExecuteCommandLists( SubmitInfo{}
            .AddCommandList( mCommandList )
            .AddSignal( completionFence, kCompletionValue ) );
        }

        // Wait only for this submission before mapping its readback buffer.
        if ( !completionFence->Wait( kCompletionValue, eastl::numeric_limits<u64>::max() ) ) {
            MKT_CORE_LOGGER_ERROR( "Timed out waiting for the shaderc debug render to complete." );
            return;
        }

#if MIKOTO_DEBUG
        DebugCompileGlsl();
#endif
    }

    auto EditorHelloCubeLayer::OnDestroy() -> void {
        mDevice->WaitIdle();

        mPipeline.Reset();
        mPipelineWireframe.Reset();
        mPipelineLayoutHandle.Reset();
        mBindingLayoutHandle.Reset();
        mVertexInputLayout.Reset();

        mVertexShader.Reset();
        mPixelShader.Reset();

        mConstantBuffer.Reset();

        mSimpleTexture.Reset();
        mColorImage.Reset();
        mDepthImage.Reset();

        mSamplerState.Reset();

        mBindingTableHandle.Reset();

        mCommandList.Reset();
    }

    auto EditorHelloCubeLayer::OnUpdate( float timeStep ) -> void {
        mCommandList->Begin( { .mScopeName = "EditorHelloCubeLayer Render" } );

        DrawWireframeMesh();

        DrawNormalMesh();

        mCommandList->SetTransition( TransitionDescription{}
            .AddTexture( mColorImage.GetPtr(), ResourceStates::eShaderResource ) );

        mCommandList->End();

        auto submitInfo{ SubmitInfo{}
            .AddCommandList( mCommandList ) };
        RenderSystem::Get()->BatchSubmission(eastl::move(submitInfo), QueueType::eGraphics);

        if (mIsImguiWindowActive) {
            DisplayImGuiWindow();
        }
    }

    auto EditorHelloCubeLayer::DrawNormalMesh() -> void {
        float angle{ as<f32>(core::TimeService::Get()->GetTime(TimeUnit::eSeconds)) * mRotationSpeed }; // seconds
        mShaderParameters.mModel = glm::rotate(
            math::constants::Identity<core::float4x4>(),
            angle,
            math::constants::kUnitVectorY
        );

        mShaderParameters.mView = glm::lookAt(
            glm::vec3{ 1.0f, 1.0f, 0.0f },// camera position
            glm::vec3{ 0.0f, 0.0f, 0.0f },// target (sphere center)
            glm::vec3{ 0.0f, 1.0f, 0.0f } // up direction
        );

        const f32 aspectRatio{ 1920.0f / 1080.0f };
        mShaderParameters.mProjection = glm::perspective(
            glm::radians( 60.0f ),// FOV
            aspectRatio,          // width / height
            0.1f,                 // near plane
            100.0f                // far plane
        );

        mCommandList->Write( mConstantBuffer.GetPtr(), MKT_ADDRESSOF( mShaderParameters ), MKT_SIZEOF( mShaderParameters ) );

        // Set graphics state
        auto graphicsState{ RenderPassDescription{}
            .SetRenderArea( Rect{ 1920, 1080 } )
            .AddDepthTarget( mDepthImage )
            .AddRenderTarget( mColorImage, Color{ 1.0f, 0.2f, 0.4f, 1.0f } ) };
        mCommandList->BeginRenderPass( graphicsState );

        auto bindingDescription{ BindResourcesDescription{}
            .SetBindPoint( PipelineType::eGraphics )
            .SetPipelineLayout( mPipelineLayoutHandle.GetPtr() )
            .AddResourceSet( 0, mBindingTableHandle.GetPtr() ) };
        mCommandList->BindPipelineResources( bindingDescription );

        mCommandList->BindPipeline( mPipeline.GetPtr() );

        auto vertexBufferDesc{ VertexBufferBinding{}
            .SetBufferBinding( 0 )
            .SetBuffer( mVertexBuffer.GetPtr() )
            .SetElementStride( MKT_SIZEOF( asset::VertexDescription_Std430Alignment ) ) };
        mCommandList->BindVertexBuffer( vertexBufferDesc );
        mCommandList->BindIndexBuffer(mIndexBuffer.GetPtr() );

        mCommandList->SetViewportState( ViewportState{}
            .AddViewportAndScissorRect( Viewport( 1920, 1080 ) ) );

        const auto drawArguments{ DrawArguments{}
            .SetInstanceCount( 1 )
            .SetIndexCount( mIndexBuffer->GetSizeBytes() / MKT_SIZEOF( u32 ) )
            .SetVertexCount( mVertexBuffer->GetSizeBytes() / MKT_SIZEOF( asset::VertexDescription_Std430Alignment ) ) };
        mCommandList->DrawIndexed( drawArguments );

        mCommandList->EndRenderPass();
    }

    auto EditorHelloCubeLayer::DrawWireframeMesh() -> void {
        float angle{ as<f32>(core::TimeService::Get()->GetTime(TimeUnit::eSeconds)) * mRotationSpeed }; // seconds

        mShaderParameters.mModel = glm::rotate(
            mShaderParameters.mModel,
            angle,
            math::constants::kUnitVectorY
        );

        mShaderParameters.mModel = glm::translate( mShaderParameters.mModel, float3{ 20.0f, 0.0f, 0.0f } );

        mShaderParameters.mView = glm::lookAt(
            glm::vec3{ 1.0f, 1.0f, 0.0f },// camera position
            glm::vec3{ 0.0f, 0.0f, 0.0f },// target (sphere center)
            glm::vec3{ 0.0f, 1.0f, 0.0f } // up direction
        );

        const f32 aspectRatio{ 1920.0f / 1080.0f };
        mShaderParameters.mProjection = glm::perspective(
            glm::radians( 60.0f ),// FOV
            aspectRatio,          // width / height
            0.1f,                 // near plane
            100.0f                // far plane
        );

        mCommandList->Write( mConstantBuffer.GetPtr(), MKT_ADDRESSOF( mShaderParameters ), MKT_SIZEOF( mShaderParameters ) );

        // Set graphics state
        auto graphicsState{ RenderPassDescription{}
            .SetRenderArea( Rect{ 1920, 1080 } )
            .AddDepthTarget( mDepthImage )
            .AddRenderTarget( mColorImage, Color{ 1.0f, 0.2f, 0.4f, 1.0f } ) };
        mCommandList->BeginRenderPass( graphicsState );

        auto bindingDescription{ BindResourcesDescription{}
            .SetBindPoint( PipelineType::eGraphics )
            .SetPipelineLayout( mPipelineLayoutHandle.GetPtr() )
            .AddResourceSet( 0, mBindingTableHandle.GetPtr() ) };
        mCommandList->BindPipelineResources( bindingDescription );
        mCommandList->BindPipeline( mPipelineWireframe.GetPtr() );

        auto vertexBufferDesc{ VertexBufferBinding{}
            .SetBufferBinding( 0 )
            .SetBuffer( mVertexBuffer.GetPtr() )
            .SetElementStride( MKT_SIZEOF( asset::VertexDescription_Std430Alignment ) ) };
        mCommandList->BindVertexBuffer( vertexBufferDesc );
        mCommandList->BindIndexBuffer(mIndexBuffer.GetPtr() );

        mCommandList->SetViewportState( ViewportState{}
            .AddViewportAndScissorRect( Viewport( 1920, 1080 ) ) );

        mCommandList->SetPolygonLineWidth( 3.0f );

        const auto drawArguments{ DrawArguments{}
            .SetInstanceCount( 1 )
            .SetIndexCount( mIndexBuffer->GetSizeBytes() / MKT_SIZEOF( u32 ) )
            .SetVertexCount( mVertexBuffer->GetSizeBytes() / MKT_SIZEOF( asset::VertexDescription_Std430Alignment ) ) };
        mCommandList->DrawIndexed( drawArguments );

        mCommandList->EndRenderPass();
    }

    auto EditorHelloCubeLayer::DebugCompileGlsl() -> void {
        if ( !mDevice->IsGraphicsApi( GraphicsAPI::eVulkan ) ) {
            MKT_CORE_LOGGER_WARN( "Skipping GLSL debug render because shaderc is currently only wired into the Vulkan backend." );
            return;
        }

        constexpr u32 kRenderWidth{ 512 };
        constexpr u32 kRenderHeight{ 512 };
        constexpr usize kBytesPerPixel{ 4 };

        // This intentionally has no vertex buffer. gl_VertexIndex selects one
        // of the three positions and colours, which keeps the shaderc smoke test
        // focused on the GLSL-to-SPIR-V path.
        constexpr char kVertexShaderSource[] = R"(
            #version 460

            layout(location = 0) out vec3 outColor;

            const vec2 kPositions[3] = vec2[](
                vec2(-0.75, -0.75),
                vec2( 0.75, -0.75),
                vec2( 0.00,  0.75)
            );

            const vec3 kColors[3] = vec3[](
                vec3(1.0, 0.0, 0.0),
                vec3(0.0, 1.0, 0.0),
                vec3(0.0, 0.0, 1.0)
            );

            void main() {
                gl_Position = vec4(kPositions[gl_VertexIndex], 0.0, 1.0);
                outColor = kColors[gl_VertexIndex];
            }
            )";

        constexpr char kPixelShaderSource[] = R"(
            #version 460

            layout(location = 0) in vec3 inColor;
            layout(location = 0) out vec4 outColor;

            void main() {
                outColor = vec4(inColor, 1.0);
            }
            )";

        auto vertexShader{ mDevice->CreateShader( ShaderModuleCreateDescription{}
            .SetContents( const_cast<char*>( kVertexShaderSource ), sizeof( kVertexShaderSource ) - 1 )
            .SetModuleName( "ShadercDebugTriangle.vert" )
            .SetLanguage( ShaderLanguage::eGLSL )
            .SetStage( ShaderType::eVertex ) ) };
        auto pixelShader{ mDevice->CreateShader( ShaderModuleCreateDescription{}
            .SetContents( const_cast<char*>( kPixelShaderSource ), sizeof( kPixelShaderSource ) - 1 )
            .SetModuleName( "ShadercDebugTriangle.frag" )
            .SetLanguage( ShaderLanguage::eGLSL )
            .SetStage( ShaderType::ePixel ) ) };

        if ( vertexShader.IsEmpty() || pixelShader.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to create shaderc debug shader resources." );
            return;
        }

        auto colorTarget{ mDevice->CreateTexture( TextureCreateDescription{}
            .SetName( "ShadercDebugTriangle_ColorTarget" )
            .SetWidth( kRenderWidth )
            .SetHeight( kRenderHeight )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::RenderTarget | TextureUsageFlagsBits::CopySource )
            .SetFormat( Format::eRGBA8_UNORM ) ) };

        auto readbackBuffer{ mDevice->CreateBuffer( BufferCreateDescription{}
            .SetName( "ShadercDebugTriangle_Readback" )
            .SetByteSize( kRenderWidth * kRenderHeight * kBytesPerPixel )
            .SetBufferUsage( BufferUsageFlagsBits::CopyDest )
            .SetHeapType( HeapType::eReadback ) ) };

        auto pipeline{ mDevice->CreatePipeline( GraphicsPipelineDescription{}
            .AddShader( pixelShader )
            .AddShader( vertexShader )
            .AddColorFormat( Format::eRGBA8_UNORM )
            .SetUseReflection( false )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .SetCullMode( CullMode::eNone )
            .SetPolygonMode( PolygonMode::eFill )
            .SetWindingOrder( WindingOrder::eCounterClockwise )
            .SetTopology( PrimitiveTopology::eTriangleList ) ) };

        auto commandList{ mDevice->CreateCommandList( QueueType::eGraphics ) };
        if ( colorTarget.IsEmpty() || readbackBuffer.IsEmpty() || pipeline.IsEmpty() || commandList.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to create resources for the shaderc debug render." );
            return;
        }

        commandList->SetEnableAutomaticBarriers( true );
        commandList->Begin( { .mScopeName = "Shaderc GLSL Debug Triangle" } );
        commandList->BeginRenderPass( RenderPassDescription{}
            .SetRenderArea( Rect{ as<i32>( kRenderWidth ), as<i32>( kRenderHeight ) } )
            .AddRenderTarget( colorTarget, kColorBlack ) );
        commandList->BindPipeline( pipeline.GetPtr() );
        commandList->SetViewportState( ViewportState{}
            .AddViewportAndScissorRect( Viewport( kRenderWidth, kRenderHeight ) ) );
        commandList->Draw( DrawArguments{}
            .SetVertexCount( 3 )
            .SetInstanceCount( 1 ) );
        commandList->EndRenderPass();
        commandList->Copy( readbackBuffer.GetPtr(), colorTarget.GetPtr() );
        commandList->End();

        IQueue* graphicsQueue{ mDevice->GetQueue( QueueType::eGraphics ) };
        if ( !graphicsQueue ) {
            MKT_CORE_LOGGER_ERROR( "The Vulkan device does not expose a graphics queue for the shaderc debug render." );
            return;
        }

        constexpr u64 kCompletionValue{ 1 };
        auto completionFence{ mDevice->CreateFence( 0 ) };
        if ( completionFence.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to create a completion fence for the shaderc debug render." );
            return;
        }

        graphicsQueue->ExecuteCommandLists( SubmitInfo{}
            .AddCommandList( commandList )
            .AddSignal( completionFence, kCompletionValue ) );

        // Wait only for this submission before mapping its readback buffer.
        if ( !completionFence->Wait( kCompletionValue, eastl::numeric_limits<u64>::max() ) ) {
            MKT_CORE_LOGGER_ERROR( "Timed out waiting for the shaderc debug render to complete." );
            return;
        }

        void* mappedImage{ mDevice->Map( readbackBuffer.GetPtr() ) };
        if ( !mappedImage ) {
            MKT_CORE_LOGGER_ERROR( "Could not map the shaderc debug render readback buffer." );
            return;
        }

        constexpr const char* kOutputPath{ "ShadercGlslTriangle.png" };
        asset::WriteImage( Path{ kOutputPath }, mappedImage, kRenderWidth, kRenderHeight, ImageFormat::eRGBA8_UINT );
        mDevice->UnMap( readbackBuffer.GetPtr() );

        MKT_CORE_LOGGER_INFO( "Wrote shaderc GLSL debug triangle to '{}'.", kOutputPath );
    }

    auto EditorHelloCubeLayer::OnEvent( core::IEvent &event ) -> void {
        if (event.IsType( EventType::KEY_PRESSED_EVENT )) {
            if (const auto *keyPressed{ dynamic_cast<core::KeyPressedEvent *>( MKT_ADDRESSOF( event ) ) }) {
                if (keyPressed->GetKeyCode() == KeyCode::Key_Y) {
                    mIsImguiWindowActive = !mIsImguiWindowActive;
                }
            }
        }
    }

    auto EditorHelloCubeLayer::DisplayImGuiWindow() -> void {
        ImGui::SetNextWindowSize( ImVec2( 420.0f, 500.0f ), ImGuiCond_FirstUseEver );

        if ( ImGui::Begin( "Hello Cube Tests", &mIsImguiWindowActive ) ) {
            auto imageID{ ImGuiService::Get()->GetTextureID( mColorImage ) };
            ImGui::Image( imageID, ImVec2{ 1280, 720 } );

            ImGui::Spacing();
            ImGui::Separator();

            ImGui::Text( "FPS: %.1f", ImGui::GetIO().Framerate );
            ImGui::Text( "Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate );

            ImGui::Spacing();
            ImGui::Separator();

            static eastl::array<f32, 4> clearColor{ 0.10f, 0.10f, 0.12f, 1.0f };

            ImGui::SliderFloat( "Rotation Speed", &mRotationSpeed, 1.0f, 50.0f );
            ImGui::ColorEdit4( "Clear Color", clearColor.data() );

            ImGui::Spacing();
            ImGui::Separator();

            ImGui::TextDisabled( "Press Y to hide/show this window." );
        }

        ImGui::End();
    }
}// namespace mikoto::editor
