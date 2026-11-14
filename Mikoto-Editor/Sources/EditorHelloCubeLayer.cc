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
#include <Core/Layer.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/TimeService.hh>
#include <Core/LocalizationService.hh>

#include <Logging/Logger.hh>

#include <Math/Math.hh>

#include <Platform/Event.hh>
#include <Platform/CoreEvents.hh>

#include <Assets/Image.hh>
#include <Assets/Utility.hh>
#include <Assets/ImageProcessing.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Core/RenderSystem.hh>

#include <ImGui/Utility.hh>
#include <ImGui/ImGuiWidget.hh>

#include <Layers/EditorHelloCubeLayer.hh>

namespace mikoto::editor {

    using namespace mikoto::core;
    using namespace mikoto::asset;
    using namespace mikoto::scene;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;
    using namespace mikoto::filesystem;

    EditorHelloCubeLayer::EditorHelloCubeLayer( platform::IWindow *window )
        : mName{ "EditorHelloCubeLayer" }, mWindow{ window } {
        MKT_BEGIN_PROFILER();
        mDevice = renderer::RenderSystem::Get()->GetGpuDevice();
    }

    auto EditorHelloCubeLayer::OnCreate() -> void {
        MKT_BEGIN_PROFILER();
        // Construct geometry: Index and vertex buffer for the cube
        auto verticesDesc{ renderer::rhi::BufferCreateDescription{}
            .SetBufferUsage( renderer::rhi::BufferUsageFlagsBits::Vertex | renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetHeapType( renderer::rhi::HeapType::eDeviceLocal )
            .SetCpuAccessType( renderer::rhi::AccessType::eRead )
            .SetInitialData( memory::BufferSpanHandle::New( mVertices.data(), MKT_VECTOR_SIZE_BYTES( mVertices ) ) ) };
        mVertexBuffer = mDevice->CreateBuffer( verticesDesc );

        // Create indices buffer
        auto indicesDesc{ renderer::rhi::BufferCreateDescription{}
            .SetBufferUsage( renderer::rhi::BufferUsageFlagsBits::Index | renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetHeapType( renderer::rhi::HeapType::eDeviceLocal )
            .SetCpuAccessType( renderer::rhi::AccessType::eRead )
            .SetFormat( renderer::rhi::Format::eR32_UINT )
            .SetInitialData( memory::BufferSpanHandle::New( mIndices.data(), MKT_VECTOR_SIZE_BYTES( mIndices ) ) ) };
        mIndexBuffer = mDevice->CreateBuffer( indicesDesc );

        // Create color attachment
        auto colorDesc{ renderer::rhi::TextureCreateDescription{}
            .SetWidth( core::as<core::i32>( 1920 ) )
            .SetHeight( core::as<core::i32>( 1080 ) )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetMultisampling( renderer::rhi::Multisampling::eMsaaX1 )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget | renderer::rhi::TextureUsageFlagsBits::ShaderResource )
            .SetFormat( renderer::rhi::Format::eBGRA8_UNORM ) };

        mColorImage = mDevice->CreateTexture( colorDesc );
        mColorImage->SetDebugName( "HelloCubeLayer Color image" );

        // Create depth attachment
        auto depthDesc{ renderer::rhi::TextureCreateDescription{}
            .SetWidth( core::as<core::i32>( 1920 ) )
            .SetHeight( core::as<core::i32>( 1080 ) )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetMultisampling( renderer::rhi::Multisampling::eMsaaX1 )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::DepthTarget )
            .SetFormat( renderer::rhi::Format::eD32 ) };

        mDepthImage = mDevice->CreateTexture( depthDesc );
        mDepthImage->SetDebugName( "HelloCubeLayer Depth image" );

        // Create shaders
        filesystem::FileHandle vsShader{ filesystem::FileService::Get()->LoadFile( "Resources/Shaders/slang/HelloTriangleBasic_Vert.slang" ) };
        auto vertexShaderDescription{ renderer::rhi::ShaderModuleCreateDescription{}
            .SetContents( vsShader )
            .SetModuleName( vsShader->GetName() )
            .SetModulePath( vsShader->GetPath() )
            .SetLanguage( renderer::rhi::ShaderLanguage::eSlang )
            .SetStage( renderer::rhi::ShaderType::eVertex ) };
        mVertexShader = mDevice->CreateShader( vertexShaderDescription );

        filesystem::FileHandle pxShader{ filesystem::FileService::Get()->LoadFile( "Resources/Shaders/slang/HelloTriangleBasic_Frag.slang" ) };
        auto fragmentShaderDescription{ renderer::rhi::ShaderModuleCreateDescription{}
            .SetContents( pxShader )
            .SetModuleName( pxShader->GetName() )
            .SetModulePath( pxShader->GetPath() )
            .SetLanguage( renderer::rhi::ShaderLanguage::eSlang )
            .SetStage( renderer::rhi::ShaderType::ePixel ) };
        mPixelShader = mDevice->CreateShader( fragmentShaderDescription );

        // Create pipeline
        eastl::array<renderer::rhi::VertexBindingDescription, 1> bindings{
            rhi::VertexBindingDescription{}
                .SetBinding( 0 )
                .SetStride( sizeof( asset::VertexDescription_Std430Alignment ) )
                .SetInputRate( renderer::rhi::InputRate::ePerVertex )
        };

        eastl::array<renderer::rhi::VertexAttributeDescription, 2> attributes{
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
                .SetOffset( offsetof( asset::VertexDescription_Std430Alignment, mUv0 ) )
        };

        mVertexInputLayout = mDevice->CreateInputLayout( renderer::rhi::InputLayoutCreateDescription{}
            .SetBindings( bindings )
            .SetAttributes( attributes )
            .SetShader( mVertexShader ) );

        // Optional. Get the bindings ready to pass in the resources to the GPU
        asset::ImageHandle image{ asset::ProcessImage2D( "Resources/Textures/diffuse.jpg" ) };
        auto textureDbug{ renderer::rhi::TextureCreateDescription{}
            .SetImageData( image )
            .SetWidth( core::as<core::i32>( image->mWidth ) )
            .SetHeight( core::as<core::i32>( image->mHeight ) )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetMultisampling( renderer::rhi::Multisampling::eMsaaX1 )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::ShaderResource )
            .SetFormat( renderer::rhi::Format::eRGBA8_UNORM ) };
        mSimpleTexture = mDevice->CreateTexture( textureDbug );

        auto constantBufferDesc{ renderer::rhi::BufferCreateDescription{}
            .SetCpuAccessType( renderer::rhi::AccessType::eWrite )
            .SetBufferUsage( renderer::rhi::BufferUsageFlagsBits::Constant | renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetResourceType( renderer::rhi::ResourceType::eConstantBuffer )
            .SetByteSize( MKT_SIZEOF( MyData ) ) };
        mConstantBuffer = mDevice->CreateBuffer( constantBufferDesc );

        auto samplerDes{ renderer::rhi::SamplerCreateDescription{}
            .SetFilter( renderer::rhi::SamplerFilter::eNearest )
            .SetWrap( renderer::rhi::SamplerWrapMode::eRepeat )
            .SetBorderColor( renderer::rhi::kColorWhite ) };
        mSamplerState = mDevice->CreateSampler( samplerDes );

        // We will upload a texture and a buffer to do some effects, see Triangle_Frag
        // Ideally we want to automate this process by allowing each backend to be able to use shader reflection
        auto layoutDesc{ renderer::rhi::BindingLayoutDescription{}
            .SetRegisterSpace( 0 )
            .SetShaderVisibility( renderer::rhi::ShaderFlagsBits::All )
            .AddItem( renderer::rhi::BindingLayoutItem::Sampler( 0 ) )
            .AddItem( renderer::rhi::BindingLayoutItem::TextureSRV( 1 ) )
            .AddItem( renderer::rhi::BindingLayoutItem::ConstantBuffer( 2 ) ) };
        mBindingLayoutHandle = mDevice->CreateBindingLayout( layoutDesc );

        // A pipeline layout describes what kind of group of resources we can bind
        // To a specific bind point. We can bind resources for Compute pipelines or Graphics pipelines, etc
        // This is handy if we have too many pipelines that share same layout for group of resources
        // we can just bind the resources once for all subsequent draws as long as the pipelines use same layout.
        mPipelineLayoutHandle = mDevice->CreatePipelineLayout( renderer::rhi::PipelineLayoutCreateDescription{}
            .AddBindingLayout( mBindingLayoutHandle ) );

        auto graphicsPipelineDescription{ renderer::rhi::GraphicsPipelineDescription{}
            .AddShader( mPixelShader )
            .AddShader( mVertexShader )
            .SetInputLayout( mVertexInputLayout )

            .SetDepthFormat( renderer::rhi::Format::eD32 )
            .AddColorFormat( renderer::rhi::Format::eBGRA8_UNORM )

            .SetPolygonMode( renderer::rhi::PolygonMode::eFill )
            .SetWindingOrder( renderer::rhi::WindingOrder::eCounterClockwise )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .SetPipelineLayout( mPipelineLayoutHandle ) };
        mPipeline = mDevice->CreatePipeline( graphicsPipelineDescription );
        mPipeline->SetDebugName( "HelloCubeLayer Pipeline" );

        auto wireframePipelineDescription{
            renderer::rhi::GraphicsPipelineDescription{ graphicsPipelineDescription }
                .SetPolygonMode( renderer::rhi::PolygonMode::eLines )
        };
        mPipelineWireframe = mDevice->CreatePipeline( wireframePipelineDescription );
        mPipelineWireframe->SetDebugName( "HelloCubeLayer PipelineWireframe" );

        auto bindingSetDesc{ renderer::rhi::BindingTableDescription{}
            .AddItem( renderer::rhi::BindingTableItem::Sampler( 0, mSamplerState.GetPtr() ) )
            .AddItem( renderer::rhi::BindingTableItem::TextureSRV( 1, mSimpleTexture.GetPtr() ) )
            .AddItem( renderer::rhi::BindingTableItem::ConstantBuffer( 2, mConstantBuffer.GetPtr() ) ) };
        mBindingTableHandle = mDevice->CreateBindingTable( bindingSetDesc, mBindingLayoutHandle );

        scene::SceneCameraDescription cameraDescription{
            .mFov = 45.0,
            .mAspectRatio = core::as<float>( mWindow->GetWidth() ) / core::as<float>( mWindow->GetHeight() ),
            .mNearPlane = 0.1f,
            .mFarPlane = 3000.0f,
            .mWindow = mWindow
        };
        mEditorCamera = eastl::make_unique<scene::SceneCamera>( cameraDescription );

        // Create the command list
        mCommandList = mDevice->CreateCommandList( renderer::rhi::QueueType::eGraphics );
        mCommandList->SetEnableAutomaticBarriers( true );
        mCommandList->SetDebugName( "HelloCubeLayer CommandList" );

        mCommandList->Open( {} );

        // Resource barriers must be emitted before dynamic rendering begins.
        // BindVertexBuffer and BindIndexBuffer then see the already-correct
        // state while each mesh render scope is active.
        mCommandList->SetTransition( { mVertexBuffer.GetPtr(), renderer::rhi::ResourceStates::eVertexBuffer } );
        mCommandList->SetTransition( { mIndexBuffer.GetPtr(), renderer::rhi::ResourceStates::eIndexBuffer } );

        mCommandList->Close();

        renderer::rhi::IQueue *graphicsQueue{ mDevice->GetQueue( renderer::rhi::QueueType::eGraphics ) };

        core::u64 kCompletionValue{ 0 };
        auto completionFence{ mDevice->CreateFence( kCompletionValue++ ) };

        if ( !completionFence.IsEmpty() ) {
            graphicsQueue->ExecuteCommandLists( renderer::rhi::SubmitInfo{}
                .AddCommandList( mCommandList )
                .AddSignal( completionFence, kCompletionValue ) );
        }

        // Wait only for this submission before mapping its readback buffer.
        if ( !completionFence->Wait( kCompletionValue, eastl::numeric_limits<core::u64>::max() ) ) {
            MKT_CORE_LOGGER_ERROR( "Timed out waiting for the shaderc debug render to complete." );
            return;
        }

#if MIKOTO_DEBUG
        DebugCompileGlsl();
#endif
    }

    auto EditorHelloCubeLayer::OnDestroy() -> void {
        MKT_BEGIN_PROFILER();
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
        MKT_BEGIN_PROFILER();
        mCommandList->Open( { .mScopeName = "EditorHelloCubeLayer Render" } );

        //DrawWireframeMesh();

        DrawNormalMesh();

        mCommandList->SetTransition( renderer::rhi::TransitionDescription{}
            .AddTexture( mColorImage.GetPtr(), renderer::rhi::ResourceStates::eShaderResource ) );

        mCommandList->Close();

        auto submitInfo{ renderer::rhi::SubmitInfo{}
            .AddCommandList( mCommandList ) };
        renderer::RenderSystem::Get()->BatchSubmission( eastl::move( submitInfo ), renderer::rhi::QueueType::eGraphics );

        if ( mIsImguiWindowActive ) {
            DisplayImGuiWindow();
        }
    }

    auto EditorHelloCubeLayer::DrawNormalMesh() -> void {
        MKT_BEGIN_PROFILER();
        float angle{ core::as<core::f32>( core::TimeService::Get()->GetTime( core::TimeUnit::eSeconds ) ) * mRotationSpeed };// seconds
        mShaderParameters.mModel = glm::rotate(
            math::constants::Identity<core::float4x4>(),
            angle,
            math::constants::kUnitVectorY );

        mShaderParameters.mView = glm::lookAt(
            glm::vec3{ 1.0f, 1.0f, 0.0f },// camera position
            glm::vec3{ 0.0f, 0.0f, 0.0f },// target (sphere center)
            glm::vec3{ 0.0f, 1.0f, 0.0f } // up direction
        );

        const core::f32 aspectRatio{ 1920.0f / 1080.0f };
        mShaderParameters.mProjection = glm::perspective(
            glm::radians( 60.0f ),// FOV
            aspectRatio,          // width / height
            0.1f,                 // near plane
            100.0f                // far plane
        );

        mCommandList->Write( mConstantBuffer.GetPtr(), MKT_ADDRESSOF( mShaderParameters ), MKT_SIZEOF( mShaderParameters ) );

        // Set graphics state
        auto graphicsState{ renderer::rhi::RenderPassDescription{}
            .SetRenderArea( renderer::rhi::Rect{ 1920, 1080 } )
            .AddDepthTarget( mDepthImage )
            .AddRenderTarget( mColorImage, renderer::rhi::Color{ 1.0f, 0.2f, 0.4f, 1.0f } ) };
        mCommandList->BeginRenderPass( graphicsState );

        auto bindingDescription{ renderer::rhi::BindResourcesDescription{}
            .SetBindPoint( renderer::rhi::PipelineType::eGraphics )
            .SetPipelineLayout( mPipelineLayoutHandle.GetPtr() )
            .AddResourceSet( 0, mBindingTableHandle.GetPtr() ) };
        mCommandList->BindPipelineResources( bindingDescription );

        mCommandList->BindPipeline( mPipeline.GetPtr() );

        auto vertexBufferDesc{ renderer::rhi::VertexBufferBinding{}
            .SetBufferBinding( 0 )
            .SetBuffer( mVertexBuffer.GetPtr() )
            .SetElementStride( MKT_SIZEOF( asset::VertexDescription_Std430Alignment ) ) };
        mCommandList->BindVertexBuffer( vertexBufferDesc );
        mCommandList->BindIndexBuffer( mIndexBuffer.GetPtr() );

        mCommandList->SetViewportState( renderer::rhi::ViewportState{}
            .AddViewportAndScissorRect( Viewport( 1920, 1080 ) ) );

        const auto drawArguments{ renderer::rhi::DrawArguments{}
            .SetInstanceCount( 1 )
            .SetIndexCount( mIndexBuffer->GetSizeBytes() / MKT_SIZEOF( core::u32 ) )
            .SetVertexCount( mVertexBuffer->GetSizeBytes() / MKT_SIZEOF( asset::VertexDescription_Std430Alignment ) ) };
        mCommandList->DrawIndexed( drawArguments );

        mCommandList->EndRenderPass();
    }

    auto EditorHelloCubeLayer::DrawWireframeMesh() -> void {
        MKT_BEGIN_PROFILER();
        float angle{ core::as<core::f32>( core::TimeService::Get()->GetTime( core::TimeUnit::eSeconds ) ) * mRotationSpeed };// seconds

        mShaderParameters.mModel = glm::rotate(
            mShaderParameters.mModel,
            angle,
            math::constants::kUnitVectorY );

        mShaderParameters.mModel = glm::translate( mShaderParameters.mModel, core::float3{ 20.0f, 0.0f, 0.0f } );

        mShaderParameters.mView = glm::lookAt(
            glm::vec3{ 1.0f, 1.0f, 0.0f },// camera position
            glm::vec3{ 0.0f, 0.0f, 0.0f },// target (sphere center)
            glm::vec3{ 0.0f, 1.0f, 0.0f } // up direction
        );

        const core::f32 aspectRatio{ 1920.0f / 1080.0f };
        mShaderParameters.mProjection = glm::perspective(
            glm::radians( 60.0f ),// FOV
            aspectRatio,          // width / height
            0.1f,                 // near plane
            100.0f                // far plane
        );

        mCommandList->Write( mConstantBuffer.GetPtr(), MKT_ADDRESSOF( mShaderParameters ), MKT_SIZEOF( mShaderParameters ) );

        // Set graphics state
        auto graphicsState{ renderer::rhi::RenderPassDescription{}
            .SetRenderArea( renderer::rhi::Rect{ 1920, 1080 } )
            .AddDepthTarget( mDepthImage )
            .AddRenderTarget( mColorImage, renderer::rhi::Color{ 1.0f, 0.2f, 0.4f, 1.0f } ) };
        mCommandList->BeginRenderPass( graphicsState );

        auto bindingDescription{ renderer::rhi::BindResourcesDescription{}
            .SetBindPoint( renderer::rhi::PipelineType::eGraphics )
            .SetPipelineLayout( mPipelineLayoutHandle.GetPtr() )
            .AddResourceSet( 0, mBindingTableHandle.GetPtr() ) };
        mCommandList->BindPipelineResources( bindingDescription );
        mCommandList->BindPipeline( mPipelineWireframe.GetPtr() );

        auto vertexBufferDesc{ renderer::rhi::VertexBufferBinding{}
            .SetBufferBinding( 0 )
            .SetBuffer( mVertexBuffer.GetPtr() )
            .SetElementStride( MKT_SIZEOF( asset::VertexDescription_Std430Alignment ) ) };
        mCommandList->BindVertexBuffer( vertexBufferDesc );
        mCommandList->BindIndexBuffer( mIndexBuffer.GetPtr() );

        mCommandList->SetViewportState( renderer::rhi::ViewportState{}
            .AddViewportAndScissorRect( Viewport( 1920, 1080 ) ) );

        mCommandList->SetPolygonLineWidth( 3.0f );

        const auto drawArguments{ renderer::rhi::DrawArguments{}
            .SetInstanceCount( 1 )
            .SetIndexCount( mIndexBuffer->GetSizeBytes() / MKT_SIZEOF( core::u32 ) )
            .SetVertexCount( mVertexBuffer->GetSizeBytes() / MKT_SIZEOF( asset::VertexDescription_Std430Alignment ) ) };
        mCommandList->DrawIndexed( drawArguments );

        mCommandList->EndRenderPass();
    }

    auto EditorHelloCubeLayer::DebugCompileGlsl() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mDevice->IsGraphicsApi( renderer::rhi::GraphicsAPI::eVulkan ) ) {
            MKT_CORE_LOGGER_WARN( "Skipping GLSL debug render because shaderc is currently only wired into the Vulkan backend." );
            return;
        }

        constexpr core::u32 kRenderWidth{ 512 };
        constexpr core::u32 kRenderHeight{ 512 };
        constexpr core::usize kBytesPerPixel{ 4 };

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

        auto vertexShader{ mDevice->CreateShader( renderer::rhi::ShaderModuleCreateDescription{}
            .SetContents( const_cast<char *>( kVertexShaderSource ), sizeof( kVertexShaderSource ) - 1 )
            .SetModuleName( "ShadercDebugTriangle.vert" )
            .SetLanguage( renderer::rhi::ShaderLanguage::eGLSL )
            .SetStage( renderer::rhi::ShaderType::eVertex ) ) };
        auto pixelShader{ mDevice->CreateShader( renderer::rhi::ShaderModuleCreateDescription{}
            .SetContents( const_cast<char *>( kPixelShaderSource ), sizeof( kPixelShaderSource ) - 1 )
            .SetModuleName( "ShadercDebugTriangle.frag" )
            .SetLanguage( renderer::rhi::ShaderLanguage::eGLSL )
            .SetStage( renderer::rhi::ShaderType::ePixel ) ) };

        if ( vertexShader.IsEmpty() || pixelShader.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to create shaderc debug shader resources." );
            return;
        }

        auto colorTarget{ mDevice->CreateTexture( renderer::rhi::TextureCreateDescription{}
            .SetName( "ShadercDebugTriangle_ColorTarget" )
            .SetWidth( kRenderWidth )
            .SetHeight( kRenderHeight )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetMultisampling( renderer::rhi::Multisampling::eMsaaX1 )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget | renderer::rhi::TextureUsageFlagsBits::CopySource )
            .SetFormat( renderer::rhi::Format::eRGBA8_UNORM ) ) };

        auto readbackBuffer{ mDevice->CreateBuffer( renderer::rhi::BufferCreateDescription{}
            .SetName( "ShadercDebugTriangle_Readback" )
            .SetByteSize( kRenderWidth * kRenderHeight * kBytesPerPixel )
            .SetBufferUsage( renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetHeapType( renderer::rhi::HeapType::eReadback ) ) };

        auto pipeline{ mDevice->CreatePipeline( renderer::rhi::GraphicsPipelineDescription{}
            .AddShader( pixelShader )
            .AddShader( vertexShader )
            .AddColorFormat( renderer::rhi::Format::eRGBA8_UNORM )
            .SetUseReflection( false )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .SetCullMode( renderer::rhi::CullMode::eNone )
            .SetPolygonMode( renderer::rhi::PolygonMode::eFill )
            .SetWindingOrder( renderer::rhi::WindingOrder::eCounterClockwise )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList ) ) };

        auto commandList{ mDevice->CreateCommandList( renderer::rhi::QueueType::eGraphics ) };

        if ( colorTarget.IsEmpty() || readbackBuffer.IsEmpty() || pipeline.IsEmpty() || commandList.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to create resources for the shaderc debug render." );
            return;
        }

        commandList->SetEnableAutomaticBarriers( true );

        commandList->Open( { .mScopeName = "Shaderc GLSL Debug Triangle" } );

        commandList->BeginRenderPass( renderer::rhi::RenderPassDescription{}
            .SetRenderArea( renderer::rhi::Rect{ core::as<core::i32>( kRenderWidth ),
                    core::as<core::i32>( kRenderHeight ) } )
            .AddRenderTarget( colorTarget, renderer::rhi::kColorBlack ) );

        commandList->BindPipeline( pipeline.GetPtr() );

        commandList->SetViewportState( renderer::rhi::ViewportState{}
            .AddViewportAndScissorRect( renderer::rhi::Viewport( kRenderWidth, kRenderHeight ) ) );

        commandList->Draw( renderer::rhi::DrawArguments{}
            .SetVertexCount( 3 )
            .SetInstanceCount( 1 ) );

        commandList->EndRenderPass();

        commandList->Copy( readbackBuffer.GetPtr(), colorTarget.GetPtr() );

        commandList->Close();

        renderer::rhi::IQueue *graphicsQueue{ mDevice->GetQueue( renderer::rhi::QueueType::eGraphics ) };

        if ( !graphicsQueue ) {
            MKT_CORE_LOGGER_ERROR( "The Vulkan device does not expose a graphics queue for the shaderc debug render." );
            return;
        }

        constexpr core::u64 kCompletionValue{ 1 };
        auto completionFence{ mDevice->CreateFence( 0 ) };

        if ( completionFence.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to create a completion fence for the shaderc debug render." );
            return;
        }

        graphicsQueue->ExecuteCommandLists( renderer::rhi::SubmitInfo{}
            .AddCommandList( commandList )
            .AddSignal( completionFence, kCompletionValue ) );

        // Wait only for this submission before mapping its readback buffer.
        if ( !completionFence->Wait( kCompletionValue, eastl::numeric_limits<core::u64>::max() ) ) {
            MKT_CORE_LOGGER_ERROR( "Timed out waiting for the shaderc debug render to complete." );
            return;
        }

        void *mappedImage{ mDevice->Map( readbackBuffer.GetPtr() ) };

        if ( !mappedImage ) {
            MKT_CORE_LOGGER_ERROR( "Could not map the shaderc debug render readback buffer." );
            return;
        }

        constexpr const char *kOutputPath{ "ShadercGlslTriangle.png" };
        asset::WriteImage( filesystem::Path{ kOutputPath }, mappedImage, kRenderWidth, kRenderHeight, asset::ImageFormat::eRGBA8_UINT );
        mDevice->UnMap( readbackBuffer.GetPtr() );

        MKT_CORE_LOGGER_INFO( "Wrote shaderc GLSL debug triangle to '{}'.", kOutputPath );
    }

    auto EditorHelloCubeLayer::OnRender( float ) -> void {
    }

    auto EditorHelloCubeLayer::GetName() const -> eastl::string_view {
        return mName;
    }

    auto EditorHelloCubeLayer::OnEvent( platform::Event &event ) -> void {
        MKT_BEGIN_PROFILER();

        if ( event.IsType( platform::EventType::KEY_PRESSED_EVENT ) ) {
            if ( const auto *keyPressed{ dynamic_cast<platform::KeyPressedEvent *>( MKT_ADDRESSOF( event ) ) } ) {
                if ( keyPressed->GetKeyCode() == platform::KeyCode::Key_F6 ) {
                    mIsImguiWindowActive = !mIsImguiWindowActive;
                }
            }
        }
    }

    auto EditorHelloCubeLayer::DisplayImGuiWindow() -> void {
        MKT_BEGIN_PROFILER();
        ImGui::SetNextWindowSize( ImVec2( 420.0f, 500.0f ), ImGuiCond_FirstUseEver );

        if ( imgui::BeginWindow( imgui::widget::LocalizedLabel( "editor_hello_cube_layer_hello_cube_tests", "Hello Cube Tests" ).c_str(), &mIsImguiWindowActive ) ) {
            auto imageID{ imgui::ImGuiService::Get()->GetTextureID( mColorImage ) };
            ImGui::Image( imageID, ImVec2{ 1280, 720 } );

            ImGui::Spacing();
            ImGui::Separator();

            ImGui::Text( "FPS: %.1f", ImGui::GetIO().Framerate );
            ImGui::Text( MKT_LOC( "editor_hello_cube_layer_frame_time_ms" ).c_str(), 1000.0f / ImGui::GetIO().Framerate );

            ImGui::Spacing();
            ImGui::Separator();

            static eastl::array<core::f32, 4> clearColor{ 0.10f, 0.10f, 0.12f, 1.0f };

            ImGui::SliderFloat( imgui::widget::LocalizedLabel( "editor_hello_cube_layer_rotation_speed", "Rotation Speed" ).c_str(), &mRotationSpeed, 1.0f, 50.0f );
            ImGui::ColorEdit4( imgui::widget::LocalizedLabel( "editor_hello_cube_layer_clear_color", "Clear Color" ).c_str(), clearColor.data() );

            ImGui::Spacing();
            ImGui::Separator();

            ImGui::TextDisabled( MKT_LOC( "editor_hello_cube_layer_press_y_to_hide_show_this_window" ).c_str() );
        }

        ImGui::End();
    }
}// namespace mikoto::editor
