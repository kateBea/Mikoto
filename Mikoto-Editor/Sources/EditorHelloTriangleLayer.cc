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

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Event.hh>
#include <Core/CoreEvents.hh>
#include <Core/LayerStack.hh>
#include <Core/TimeService.hh>

#include <Assets/Image.hh>
#include <Assets/ImageProcessor.hh>

#include <Renderer/Core/Rhi.hh>
#include <Renderer/Core/RenderSystem.hh>

#include <Layers/EditorHelloTriangleLayer.hh>

namespace mikoto::editor {

    using namespace mikoto::renderer;

    EditorHelloTriangleLayer::EditorHelloTriangleLayer( platform::Window *window )
        : ILayer{ "EditorHelloTriangleLayer" }, mWindow{ window }
    {
        mDevice = renderer::RenderSystem::Get()->GetGpuDevice();
    }

    auto EditorHelloTriangleLayer::OnCreate() -> void {
        // Construct geometry: Index and vertex buffer for the triangle
        auto verticesDesc{ BufferCreateDescription{}
            .SetBufferUsage( BufferUsageFlagsBits::kVertex | BufferUsageFlagsBits::kCopyDst )
            .SetHeapType( HeapType::eDeviceLocal )
            .SetCpuAccessType( CpuAccessType::eRead )
            .SetInitialData( BufferSpanHandle::Spawn( mVertices.data(), MKT_VECTOR_SIZE_BYTES(mVertices) ) ) };
        mVertexBuffer = mDevice->CreateBuffer( verticesDesc );

        // Create indices buffer
        auto indicesDesc{ BufferCreateDescription{}
            .SetBufferUsage( BufferUsageFlagsBits::kIndex | BufferUsageFlagsBits::kCopyDst )
            .SetHeapType( HeapType::eDeviceLocal )
            .SetCpuAccessType( CpuAccessType::eRead )
            .SetFormat( Format::eR32_UINT )
            .SetInitialData( BufferSpanHandle::Spawn( mIndices.data(), MKT_VECTOR_SIZE_BYTES(mIndices) ) ) };
        mIndexBuffer = mDevice->CreateBuffer( indicesDesc );

        // Create color attachment
        auto colorDesc{ TextureCreateDescription{}
            .SetWidth( as<i32>( 1920 ) )
            .SetHeight( as<i32>( 1080 ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kShaderResource )
            .SetFormat( Format::eBGRA8_UNORM ) };

        mColorImage = mDevice->CreateTexture( colorDesc );
        mColorImage->SetDebugName( "HelloTriangleLayer Color image" );

        // Create depth attachment
        auto depthDesc{ TextureCreateDescription{}
            .SetWidth( as<i32>( 1920 ) )
            .SetHeight( as<i32>( 1080 ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kDepthTarget )
            .SetFormat( Format::eD32 ) };

        mDepthImage = mDevice->CreateTexture( depthDesc );
        mDepthImage->SetDebugName( "HelloTriangleLayer Depth image" );

        // Create shaders
        auto vertexShaderDescription{ ShaderModuleCreateDescription{}
            .SetFile( FileService::Get()->LoadFile( "Resources/Shaders/slang/HelloTriangleBasic_Vert.slang" ) )
            .SetStage( ShaderType::eVertex ) };
        mVertexShader = mDevice->CreateShader( vertexShaderDescription );
        mVertexShader->DumpShaderCode();

        auto fragmentShaderDescription{ ShaderModuleCreateDescription{}
            .SetFile( FileService::Get()->LoadFile( "Resources/Shaders/slang/HelloTriangleBasic_Frag.slang" ) )
            .SetStage( ShaderType::ePixel ) };
        mPixelShader = mDevice->CreateShader( fragmentShaderDescription );
        mPixelShader->DumpShaderCode();

        // Create pipeline
        eastl::array<rhi::VertexBindingDescription, 1> bindings{
    rhi::VertexBindingDescription{}
            .SetBinding( 0 )
            .SetStride( sizeof( asset::VertexDescription ) )
            .SetInputRate( InputRate::ePerVertex ) };

        eastl::array<rhi::VertexAttributeDescription, 9> attributes{
    rhi::VertexAttributeDescription{}
            .SetName( "POSITION" )
            .SetLocation( 0 )
            .SetBinding( 0 )
            .SetFormat( rhi::Format::eRGB32_FLOAT )
            .SetOffset( offsetof( asset::VertexDescription, mPosition ) ),

            rhi::VertexAttributeDescription{}
            .SetName( "NORMAL" )
            .SetLocation( 1 )
            .SetBinding( 0 )
            .SetFormat( rhi::Format::eRGB32_FLOAT )
            .SetOffset( offsetof( asset::VertexDescription, mNormals ) ),

    rhi::VertexAttributeDescription{}
            .SetName( "COLOR" )
            .SetLocation( 2 )
            .SetBinding( 0 )
            .SetFormat( rhi::Format::eRGBA32_FLOAT )
            .SetOffset( offsetof( asset::VertexDescription, mColors ) ),

            rhi::VertexAttributeDescription{}
            .SetName( "TEXCOORD" )
            .SetLocation( 3 )
            .SetBinding( 0 )
            .SetFormat( rhi::Format::eRG32_FLOAT )
            .SetOffset( offsetof( asset::VertexDescription, mUv0 ) ),

            rhi::VertexAttributeDescription{}
            .SetName( "TEXCOORD" )
            .SetLocation( 4 )
            .SetBinding( 0 )
            .SetFormat( rhi::Format::eRG32_FLOAT )
            .SetOffset( offsetof( asset::VertexDescription, mUv1 ) ),

    rhi::VertexAttributeDescription{}
            .SetName( "BLENDINDICES" )
            .SetLocation( 5 )
            .SetBinding( 0 )
            .SetFormat( rhi::Format::eRGBA32_FLOAT )
            .SetOffset( offsetof( asset::VertexDescription, mJoints0 ) ),

            rhi::VertexAttributeDescription{}
            .SetName( "BLENDWEIGHT" )
            .SetLocation( 6 )
            .SetBinding( 0 )
            .SetFormat( rhi::Format::eRGBA32_FLOAT )
            .SetOffset( offsetof( asset::VertexDescription, mWeights0 ) ),

            rhi::VertexAttributeDescription{}
            .SetName( "BLENDINDICES" )
            .SetLocation( 7 )
            .SetBinding( 0 )
            .SetFormat( rhi::Format::eRGBA32_FLOAT )
            .SetOffset( offsetof( asset::VertexDescription, mJoints1 ) ),

    rhi::VertexAttributeDescription{}
            .SetName( "BLENDWEIGHT" )
            .SetLocation( 8 )
            .SetBinding( 0 )
            .SetFormat( rhi::Format::eRGBA32_FLOAT )
            .SetOffset( offsetof( asset::VertexDescription, mWeights1 ) ), };

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
            .SetUsage( TextureUsageFlagsBits::kShaderResource )
            .SetFormat( Format::eRGBA8_UNORM ) };
        mSimpleTexture = mDevice->CreateTexture( textureDbug );

        auto constantBufferDesc{ BufferCreateDescription{}
            .SetCpuAccessType( CpuAccessType::eWrite )
            .SetBufferUsage( BufferUsageFlagsBits::kConstant | BufferUsageFlagsBits::kCopyDst )
            .SetResourceType( ResourceType::eConstantBuffer )
            .SetByteSize(MKT_SIZEOF( MyData )) };
        mConstantBuffer = mDevice->CreateBuffer(constantBufferDesc);

        auto samplerDes{ SamplerCreateDescription{}
            .SetFilter( rhi::SamplerFilter::eNearest )
            .SetWrap( SamplerWrapMode::eRepeat )
            .SetBorderColor( kColorWhite ) };
        mSamplerState = mDevice->CreateSampler( samplerDes );

        // Create the command list
        mCommandList = mDevice->CreateCommandList( QueueType::eGraphics );
        mCommandList->SetEnableAutomaticBarriers( true );
        mCommandList->SetDebugName( "GameLayer CommandList" );

        // We will upload a texture and a buffer to do some effects, see Triangle_Frag
        // Ideally we want to automate this process by allowing each backend to be able to use shader reflection
        auto layoutDesc{ BindingLayoutDescription{}
            .SetRegisterSpace( 0 )
            .SetShaderVisibility(ShaderFlagsBits::kAll)
            .AddItem(BindingLayoutItem::Sampler(0))
            .AddItem(BindingLayoutItem::Texture_SRV(1))
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

            .SetUseReflection( false )

            .SetPolygonMode( PolygonMode::eFill )
            .SetCullMode( CullMode::eCullBack )
            .SetWindingOrder( WindingOrder::eCounterClockwise )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .SetPipelineLayout( mPipelineLayoutHandle ) };
        mPipeline = mDevice->CreatePipeline( graphicsPipelineDescription );

        auto bindingSetDesc{ BindingSetDescription{}
            .AddItem( BindingSetItem::Sampler( 0, mSamplerState.GetRaw() ) )
            .AddItem( BindingSetItem::Texture_SRV( 1, mSimpleTexture.GetRaw() ) )
            .AddItem( BindingSetItem::ConstantBuffer( 2, mConstantBuffer.GetRaw() ) ) };
        mBindingSetHandle = mDevice->CreateBindingSet( bindingSetDesc, mBindingLayoutHandle );

        SceneCameraDescription cameraDescription{
            .mFov = 45.0,
            .mAspectRatio = as<float>( mWindow->GetWidth() ) / as<float>( mWindow->GetHeight() ),
            .mNearPlane = 0.1f,
            .mFarPlane = 3000.0f,
            .mWindow = mWindow };
        mEditorCamera = eastl::make_unique<SceneCamera>( cameraDescription );
    }

    auto EditorHelloTriangleLayer::OnDestroy() -> void {
        mDevice->WaitIdle();

        mPipeline.Reset();
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

        mBindingSetHandle.Reset();

        mCommandList.Reset();
    }

    auto EditorHelloTriangleLayer::OnUpdate( float timeStep ) -> void {
        mCommandList->Begin( { .mScopeName = "EditorHelloTriangleLayer Render" } );

        float angle{ as<f32>(core::TimeService::Get()->GetTime(TimeUnit::eSeconds)) }; // seconds
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

        mCommandList->Write( mConstantBuffer.GetRaw(), MKT_ADDRESSOF( mShaderParameters ), MKT_SIZEOF( mShaderParameters ) );

        // Set graphics state
        auto graphicsState{ GraphicsState{}
            .SetRenderArea( Rect{ 1920, 1080 } )
            .AddDepthTarget( mDepthImage )
            .AddRenderTarget( mColorImage, Color{ 1.0f, 0.2f, 0.4f, 1.0f } ) };
        mCommandList->BeginRendering( graphicsState );

        mCommandList->BindPipeline( mPipeline.GetRaw() );

        auto bindingDescription{ BindResourcesDescription{}
            .SetBindPoint( PipelineType::eGraphics )
            .SetPipelineLayout( mPipelineLayoutHandle.GetRaw() )
            .AddResourceSet( 0, mBindingSetHandle.GetRaw() ) };
        mCommandList->BindPipelineResources( bindingDescription );
        mCommandList->BindIndexBuffer(mIndexBuffer.GetRaw() );

        auto vertexBufferDesc{ VertexBufferBinding{}
            .SetBufferBinding( 0 )
            .SetBuffer( mVertexBuffer.GetRaw() )
            .SetElementStride( MKT_SIZEOF( asset::VertexDescription ) ) };
        mCommandList->BindVertexBuffer( vertexBufferDesc );

        mCommandList->SetViewportState( ViewportState{}
            .AddViewportAndScissorRect( Viewport( 1920, 1080 ) ) );

        const auto drawArguments{ DrawArguments{}
            .SetInstanceCount( 1 )
            .SetIndexCount( mIndexBuffer->GetSizeBytes() / MKT_SIZEOF( u32 ) )
            .SetVertexCount( mVertexBuffer->GetSizeBytes() / MKT_SIZEOF( asset::VertexDescription ) ) };
        mCommandList->DrawIndexed( drawArguments );

        mCommandList->EndRendering();

        mCommandList->SetBarrier( mColorImage.GetRaw(), ResourceStates::eShaderResource );

        mCommandList->End();

        // Not executed immediately, cached to do one BIG submission.
        mDevice->ExecuteCommands( mCommandList );

        if (mIsImguiWindowActive) {
            DisplayImGuiWindow();
        }
    }

    auto EditorHelloTriangleLayer::OnEvent( core::IEvent &event ) -> void {
        if (event.IsType( EventType::KEY_PRESSED_EVENT )) {
            if (const auto *keyPressed{ dynamic_cast<core::KeyPressedEvent *>( MKT_ADDRESSOF( event ) ) }) {
                if (keyPressed->GetKeyCode() == KeyCode::Key_H) {
                    mIsImguiWindowActive = !mIsImguiWindowActive;
                }
            }
        }
    }

    auto EditorHelloTriangleLayer::DisplayImGuiWindow() -> void {
        ImGui::SetNextWindowSize( ImVec2( 420.0f, 500.0f ), ImGuiCond_FirstUseEver );

        if ( ImGui::Begin( "Hello Triangle Tests", &mIsImguiWindowActive ) ) {
            auto imageID{ ImGuiService::Get()->GetTextureID( mColorImage ) };
            ImGui::Image( imageID, ImVec2{ 128, 128 } );

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

            ImGui::TextDisabled( "Press H to hide/show this window." );
        }

        ImGui::End();
    }
}// namespace mikoto::editor