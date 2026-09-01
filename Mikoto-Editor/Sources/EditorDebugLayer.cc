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

#include <EASTL/array.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Event.hh>
#include <Core/CoreEvents.hh>
#include <Core/InputSystem.hh>
#include <Core/MouseCodes.hh>

#include <Math/Math.hh>

#include <Assets/Model.hh>
#include <Assets/Importer.hh>
#include <Assets/AssetsService.hh>
#include <Assets/ImageProcessor.hh>

#include <Memory/Allocator.hh>

#include <Filesystem/File.hh>
#include <Filesystem/Path.hh>
#include <Filesystem/FileService.hh>

#include <Scene/SceneCamera.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Buffer.hh>
#include <Renderer/Rhi/Texture.hh>
#include <Renderer/Rhi/GpuDevice.hh>

#include <Renderer/Core/RenderSystem.hh>

#include <Layers/EditorDebugLayer.hh>

namespace mikoto::editor {

    using namespace mikoto::core;
    using namespace mikoto::asset;
    using namespace mikoto::scene;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;
    using namespace mikoto::filesystem;

    EditorDebugLayer::EditorDebugLayer( Window *window )
        : ILayer{ "GameLayer" }, mWindow{ window } {
        mDevice = RenderSystem::Get()->GetGpuDevice();
    }

    auto EditorDebugLayer::OnUpdate( float deltaTime ) -> void {
        // D3D11 does not implement bindless yet
        if (mDevice->IsGraphicsApi( GraphicsAPI::eD3D11 )) {
            return;
        }

        UpdateCameraState( deltaTime );

        mCommandList->Begin( { .mScopeName = "EditorDebug Render" } );

        // Fill the constant buffer
        mCameraProps.mView = glm::lookAt(
            glm::vec3{ 0.0f, 0.0f, -50.0f },// camera position
            glm::vec3{ 0.0f, 0.0f, 0.0f },// target (sphere center)
            glm::vec3{ 0.0f, 1.0f, 0.0f } // up direction
        );

        const f32 aspectRatio{ 1920.0f / 1080.0f };
        mCameraProps.mProjection = glm::perspective(
            glm::radians( 60.0f ),// FOV
            aspectRatio,          // width / height
            0.1f,                 // near plane
            100.0f                // far plane
        );

        // Renders to the whole render area and spikes GPU usage
        // mCameraProps.mView = mEditorCamera->GetViewMatrix();
        // mCameraProps.mProjection = mEditorCamera->GetProjection();

        mCommandList->Write( mConstantBuffer.GetRaw(), MKT_ADDRESSOF( mCameraProps ), MKT_SIZEOF( mCameraProps ) );

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
            .AddResourceSet( 0, mBindingSetHandle.GetRaw() )
            .AddResourceSet( 1, mDescriptorTable.GetRaw() ) };
        mCommandList->BindPipelineResources( bindingDescription );

        for (u32 meshIndex{}; meshIndex < mModelHandle->GetMeshNodeCount(); ++meshIndex) {
            asset::MeshNode* mesh{ MKT_ADDRESSOF( mModelHandle->GetMeshNode( meshIndex ) ) };
            mCommandList->BindIndexBuffer( mesh->GetIndexBuffer().GetRaw() );
            mCommandList->BindVertexBuffer( VertexBufferBinding{}
                .SetBufferBinding( 0 )
                .SetBuffer( mesh->GetVertexBuffer().GetRaw() )
                .SetElementStride( MKT_SIZEOF( asset::VertexDescription_Std430Alignment ) ) );

            mCommandList->SetViewportState( ViewportState{}
                .AddViewportAndScissorRect( Viewport( 1920, 1080 ) ) );

            // Issue draw call
            const auto drawArguments{ DrawArguments{}
                .SetInstanceCount( 1000 )
                .SetIndexCount( mesh->GetIndexBuffer()->GetSizeBytes() / MKT_SIZEOF( u32 ) )
                .SetVertexCount( mesh->GetVertexBuffer()->GetSizeBytes() / MKT_SIZEOF( asset::VertexDescription_Std430Alignment ) ) };
            mCommandList->DrawIndexed( drawArguments );
        }

        mCommandList->EndRendering();

        mCommandList->End();

        auto submitInfo{ SubmitInfo{}
            .AddCommandList( mCommandList ) };
        RenderSystem::Get()->BatchSubmission(eastl::move(submitInfo), QueueType::eGraphics);
    }

    auto EditorDebugLayer::OnCreate() -> void {
        // D3D11 does not implement bindless yet
        if (mDevice->IsGraphicsApi( GraphicsAPI::eD3D11 )) {
            return;
        }

        // Create color attachment
        auto colorDesc{ TextureCreateDescription{}
            .SetWidth( as<i32>( 1920 ) )
            .SetHeight( as<i32>( 1080 ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kShaderResource )
            .SetFormat( Format::eBGRA8_UNORM ) };

        mColorImage = mDevice->CreateTexture( colorDesc );
        mColorImage->SetDebugName( "GameLayer Color image" );

        // Create depth attachment
        auto depthDesc{ TextureCreateDescription{}
            .SetWidth( as<i32>( 1920 ) )
            .SetHeight( as<i32>( 1080 ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kDepthTarget )
            .SetFormat( Format::eD32 ) };

        mDepthImage = mDevice->CreateTexture( depthDesc );
        mDepthImage->SetDebugName( "GameLayer Depth image" );

        // Create shaders
        FileHandle vsShader{ FileService::Get()->LoadFile( "Resources/Shaders/slang/HelloTriangle_Vert.slang" ) };
        auto vertexShaderDescription{ ShaderModuleCreateDescription{}
            .SetContents( vsShader )
            .SetModuleName( vsShader->GetName() )
            .SetModulePath( vsShader->GetPath() )
            .SetLanguage( ShaderLanguage::eSlang )
            .SetStage( ShaderType::eVertex ) };
        mVertexShader = mDevice->CreateShader( vertexShaderDescription );
        mVertexShader->DumpShaderCode();

        FileHandle pxShader{ FileService::Get()->LoadFile( "Resources/Shaders/slang/HelloTriangle_Frag.slang" ) };
        auto fragmentShaderDescription{ ShaderModuleCreateDescription{}
            .SetContents( pxShader )
            .SetModuleName( pxShader->GetName() )
            .SetModulePath( pxShader->GetPath() )
            .SetLanguage( ShaderLanguage::eSlang )
            .SetStage( ShaderType::ePixel ) };
        mPixelShader = mDevice->CreateShader( fragmentShaderDescription );
        mPixelShader->DumpShaderCode();

        // Create pipeline
        eastl::array bindings{
    rhi::VertexBindingDescription{}
            .SetBinding( 0 )
            .SetStride( sizeof( asset::VertexDescription_Std430Alignment ) )
            .SetInputRate( InputRate::ePerVertex ) };

        eastl::array attributes{
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
            .SetUsage( TextureUsageFlagsBits::kShaderResource )
            .SetFormat( Format::eRGBA8_UNORM ) };
        mSimpleTexture = mDevice->CreateTexture( textureDbug );

        auto constantBufferDesc{ BufferCreateDescription{}
            .SetCpuAccessType( CpuAccessType::eWrite )
            .SetBufferUsage( BufferUsageFlagsBits::kConstant | BufferUsageFlagsBits::kCopyDst )
            .SetResourceType( ResourceType::eConstantBuffer )
            .SetByteSize(MKT_SIZEOF( ConstantBuffer )) };
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

        // Bindless setup
        auto bindlessLayout{ BindlessLayoutDescription{}
            .SetVisibility(ShaderFlagsBits::kAll)
            .SetRegisterSpace( 1 )
            .AddBindlessItem(BindlessLayoutItem::Texture_SRV(0, 1024))
            .AddBindlessItem(BindlessLayoutItem::Samplers(1, 1024)) };
        mBindlessLayout = mDevice->CreateBindlessLayout( bindlessLayout );

        // A pipeline layout describes what kind of group of resources we can bind
        // To a specific bind point. We can bind resources for Compute pipelines or Graphics pipelines, etc
        // This is handy if we have too many pipelines that share same layout for group of resources
        // we can just bind the resources once for all subsequent draws as long as the pipelines use same layout.
        mPipelineLayoutHandle = mDevice->CreatePipelineLayout( PipelineLayoutCreateDescription{}
            .AddBindingLayout( mBindingLayoutHandle )
            .AddBindingLayout( mBindlessLayout ));

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

        mDescriptorTable = mDevice->CreateDescriptorTable( mBindlessLayout );

        auto bindingSetDesc{ BindingSetDescription{}
            .AddItem( BindingSetItem::Sampler( 0, mSamplerState.GetRaw() ) )
            .AddItem( BindingSetItem::Texture_SRV( 1, mSimpleTexture.GetRaw() ) )
            .AddItem( BindingSetItem::ConstantBuffer( 2, mConstantBuffer.GetRaw() ) ) };
        mBindingSetHandle = mDevice->CreateBindingSet( bindingSetDesc, mBindingLayoutHandle );

        (void)mDevice->WriteDescriptorTable( mDescriptorTable, BindingSetItem::Sampler( 0, mSamplerState.GetRaw() ) );
        (void)mDevice->WriteDescriptorTable( mDescriptorTable, BindingSetItem::Texture_SRV( 0, mSimpleTexture.GetRaw() ) );

        SceneCameraDescription cameraDescription{
            .mFov = 45.0,
            .mAspectRatio = as<float>( mWindow->GetWidth() ) / as<float>( mWindow->GetHeight() ),
            .mNearPlane = 0.1f,
            .mFarPlane = 3000.0f,
            .mWindow = mWindow };

        mEditorCamera = Ref<SceneCamera>::New( cameraDescription );

        asset::AssetsService::Get()->LoadAssetAsync<asset::Model>( "Resources/Prefabs/deadpool/scene.gltf" );
        asset::AssetsService::Get()->LoadAssetAsync<asset::Model>( "Resources/Prefabs/miss_galaxy/scene.gltf" );
        asset::AssetsService::Get()->LoadAssetAsync<asset::Model>( "Resources/Prefabs/bee/scene.gltf" );

        asset::AssetsService::Get()->LoadAssetAsync<asset::Model>( "Resources/Prefabs/robot/gltf/scene.gltf" );

        mModelHandle = asset::AssetsService::Get()->LoadAsset<asset::Model>( "Resources/Models/Prefabs/cone/gltf/scene.gltf" );
    }

    auto EditorDebugLayer::UpdateCameraState( float ts  ) -> void {

        mEditorCamera->SetMovementSpeed( 30 );
        mEditorCamera->SetRotationSpeed( 30 );

        mEditorCamera->SetFarPlane( 0.01 );
        mEditorCamera->SetNearPlane( 2000 );

        mEditorCamera->WantRotation( true, true );

        mEditorCamera->SetFieldOfView( 45 );

        // Set viewport to the currently active window we can either expand
        // the final composition to occupy the whole screen or just an ImGui viewport
        mEditorCamera->SetViewportSize( mWindow->GetWidth(), mWindow->GetHeight() );

        if (InputSystem::Get()->IsMouseKeyPressed( Mouse_Button_Right ) ) {
            mEditorCamera->EnableCamera( true );
        } else {
            mEditorCamera->EnableCamera( false );
        }

        mEditorCamera->Update( ts );
    }

    auto EditorDebugLayer::OnDestroy() -> void {
        // Ensure GPU is done
        mDevice->WaitIdle();

        mPipeline.Release();
        mPipelineLayoutHandle.Release();
        mBindingLayoutHandle.Release();
        mVertexInputLayout.Release();

        mBindlessLayout.Release();
        mDescriptorTable.Release();

        mVertexShader.Release();
        mPixelShader.Release();

        mConstantBuffer.Release();

        mSimpleTexture.Release();
        mColorImage.Release();
        mDepthImage.Release();

        mSamplerState.Release();

        mBindingSetHandle.Release();

        mModelHandle.Release();

        mCommandList.Release();
    }

    auto EditorDebugLayer::OnEvent(IEvent& event) -> void {
        if (event.IsType(EventType::MOUSE_BUTTON_PRESSED_EVENT)) {
            auto* mouseButtonEvent{
                as<MouseButtonPressedEvent*>(MKT_ADDRESSOF(event))
            };

            if (mouseButtonEvent->GetMouseButton() == Mouse_Button_Right) {
                const float angularSpeed{15.0f};

                // Example: rotate around Y (local)
                mRotation.y += angularSpeed;
                mCameraProps.mModel = math::RecomputeTransform( mPosition, mScale, mRotation, mPivot );
            }
        }
    }
}// namespace mikoto::editor
