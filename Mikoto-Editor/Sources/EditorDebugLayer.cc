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
#include <Core/Profiler.hh>

#include <Math/Math.hh>

#include <Memory/Allocator.hh>

#include <Filesystem/File.hh>
#include <Filesystem/Path.hh>
#include <Filesystem/FileService.hh>

#include <Platform/Event.hh>
#include <Platform/KeyCodes.hh>
#include <Platform/CoreEvents.hh>
#include <Platform/InputSystem.hh>

#include <Assets/Utility.hh>
#include <Assets/Importer.hh>
#include <Assets/AssetModel.hh>
#include <Assets/ImageProcessing.hh>
#include <Assets/AssetRegistryService.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Buffer.hh>
#include <Renderer/Rhi/Texture.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Core/RenderSystem.hh>

#include <Scene/SceneCamera.hh>

#include <Layers/EditorDebugLayer.hh>


namespace mikoto::editor {

    using namespace mikoto::core;
    using namespace mikoto::asset;
    using namespace mikoto::scene;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;
    using namespace mikoto::filesystem;

    EditorDebugLayer::EditorDebugLayer( platform::IWindow* window )
        : mName{ "GameLayer" }, mWindow{ window } {
        MKT_BEGIN_PROFILER();
        mDevice = renderer::RenderSystem::Get()->GetGpuDevice();
    }

    auto EditorDebugLayer::OnUpdate( float deltaTime ) -> void {
        MKT_BEGIN_PROFILER();
        // D3D11 does not implement bindless yet
        if ( mDevice->IsGraphicsApi( renderer::rhi::GraphicsAPI::eD3D11 ) ) {
            return;
        }

        UpdateCameraState( deltaTime );

        mCommandList->Open( { .mScopeName = "EditorDebug Render" } );

        // Fill the constant buffer
        mCameraProps.mView = glm::lookAt(
            glm::vec3{ 0.0f, 0.0f, -50.0f },// camera position
            glm::vec3{ 0.0f, 0.0f, 0.0f },  // target (sphere center)
            glm::vec3{ 0.0f, 1.0f, 0.0f }   // up direction
        );

        const core::f32 aspectRatio{ 1920.0f / 1080.0f };
        mCameraProps.mProjection = glm::perspective(
            glm::radians( 60.0f ),// FOV
            aspectRatio,          // width / height
            0.1f,                 // near plane
            100.0f                // far plane
        );

        // Renders to the whole render area and spikes GPU usage
        // mCameraProps.mView = mEditorCamera->GetViewMatrix();
        // mCameraProps.mProjection = mEditorCamera->GetProjection();

        mCommandList->Write( mConstantBuffer.GetPtr(), MKT_ADDRESSOF( mCameraProps ), MKT_SIZEOF( mCameraProps ) );

        // Set graphics state
        auto graphicsState{ renderer::rhi::RenderPassDescription{}
            .SetRenderArea( renderer::rhi::Rect{ 1920, 1080 } )
            .AddDepthTarget( mDepthImage )
            .AddRenderTarget( mColorImage, renderer::rhi::Color{ 1.0f, 0.2f, 0.4f, 1.0f } ) };

        // Resource barriers must be recorded before dynamic rendering begins.
        for ( core::u32 meshIndex{}; meshIndex < mModelHandle->GetMeshNodeCount(); ++meshIndex ) {
            asset::MeshNode* mesh{ MKT_ADDRESSOF( mModelHandle->GetMeshNode( meshIndex ) ) };
            mCommandList->SetTransition( renderer::rhi::TransitionDescription{}
                .AddBuffer( mesh->GetIndexBuffer().GetPtr(), renderer::rhi::ResourceStates::eIndexBuffer )
                .AddBuffer( mesh->GetVertexBuffer().GetPtr(), renderer::rhi::ResourceStates::eVertexBuffer ) );
        }

        mCommandList->BeginRenderPass( graphicsState );

        mCommandList->BindPipeline( mPipeline.GetPtr() );

        auto bindingDescription{ renderer::rhi::BindResourcesDescription{}
            .SetBindPoint( renderer::rhi::PipelineType::eGraphics )
            .SetPipelineLayout( mPipelineLayoutHandle.GetPtr() )
            .AddResourceSet( 0, mBindingSetHandle.GetPtr() )
            .AddResourceSet( 1, mDescriptorTable.GetPtr() ) };
        mCommandList->BindPipelineResources( bindingDescription );

        for ( core::u32 meshIndex{}; meshIndex < mModelHandle->GetMeshNodeCount(); ++meshIndex ) {
            asset::MeshNode* mesh{ MKT_ADDRESSOF( mModelHandle->GetMeshNode( meshIndex ) ) };
            mCommandList->BindIndexBuffer( mesh->GetIndexBuffer().GetPtr() );
            mCommandList->BindVertexBuffer( renderer::rhi::VertexBufferBinding{}
                .SetBufferBinding( 0 )
                .SetBuffer( mesh->GetVertexBuffer().GetPtr() )
                .SetElementStride( MKT_SIZEOF( asset::VertexDescription_Std430Alignment ) ) );

            mCommandList->SetViewportState( renderer::rhi::ViewportState{}
                .AddViewportAndScissorRect( Viewport( 1920, 1080 ) ) );

            // Issue draw call
            const auto drawArguments{ renderer::rhi::DrawArguments{}
                .SetInstanceCount( 1000 )
                .SetIndexCount( mesh->GetIndexBuffer()->GetSizeBytes() / MKT_SIZEOF( core::u32 ) )
                .SetVertexCount( mesh->GetVertexBuffer()->GetSizeBytes() / MKT_SIZEOF( asset::VertexDescription_Std430Alignment ) ) };
            mCommandList->DrawIndexed( drawArguments );
        }

        mCommandList->EndRenderPass();

        mCommandList->Close();

        auto submitInfo{ renderer::rhi::SubmitInfo{}
            .AddCommandList( mCommandList ) };
        renderer::RenderSystem::Get()->BatchSubmission( eastl::move( submitInfo ), renderer::rhi::QueueType::eGraphics );
    }

    auto EditorDebugLayer::OnCreate() -> void {
        MKT_BEGIN_PROFILER();
        // D3D11 does not implement bindless yet
        if ( mDevice->IsGraphicsApi( renderer::rhi::GraphicsAPI::eD3D11 ) ) {
            return;
        }

        // Create color attachment
        auto colorDesc{ renderer::rhi::TextureCreateDescription{}
            .SetWidth( core::as<core::i32>( 1920 ) )
            .SetHeight( core::as<core::i32>( 1080 ) )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetMultisampling( renderer::rhi::Multisampling::eMsaaX1 )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget | renderer::rhi::TextureUsageFlagsBits::ShaderResource )
            .SetFormat( renderer::rhi::Format::eBGRA8_UNORM ) };

        mColorImage = mDevice->CreateTexture( colorDesc );
        mColorImage->SetDebugName( "GameLayer Color image" );

        // Create depth attachment
        auto depthDesc{ renderer::rhi::TextureCreateDescription{}
            .SetWidth( core::as<core::i32>( 1920 ) )
            .SetHeight( core::as<core::i32>( 1080 ) )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetMultisampling( renderer::rhi::Multisampling::eMsaaX1 )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::DepthTarget )
            .SetFormat( renderer::rhi::Format::eD32 ) };

        mDepthImage = mDevice->CreateTexture( depthDesc );
        mDepthImage->SetDebugName( "GameLayer Depth image" );

        // Create shaders
        filesystem::FileHandle vsShader{ filesystem::FileService::Get()->LoadFile( "Resources/Shaders/slang/HelloTriangle_Vert.slang" ) };
        auto vertexShaderDescription{ renderer::rhi::ShaderModuleCreateDescription{}
            .SetContents( vsShader )
            .SetModuleName( vsShader->GetName() )
            .SetModulePath( vsShader->GetPath() )
            .SetLanguage( renderer::rhi::ShaderLanguage::eSlang )
            .SetStage( renderer::rhi::ShaderType::eVertex ) };
        mVertexShader = mDevice->CreateShader( vertexShaderDescription );
        mVertexShader->DumpShaderCode();

        filesystem::FileHandle pxShader{ filesystem::FileService::Get()->LoadFile( "Resources/Shaders/slang/HelloTriangle_Frag.slang" ) };
        auto fragmentShaderDescription{ renderer::rhi::ShaderModuleCreateDescription{}
            .SetContents( pxShader )
            .SetModuleName( pxShader->GetName() )
            .SetModulePath( pxShader->GetPath() )
            .SetLanguage( renderer::rhi::ShaderLanguage::eSlang )
            .SetStage( renderer::rhi::ShaderType::ePixel ) };
        mPixelShader = mDevice->CreateShader( fragmentShaderDescription );
        mPixelShader->DumpShaderCode();

        // Create pipeline
        eastl::array bindings{
            rhi::VertexBindingDescription{}
                .SetBinding( 0 )
                .SetStride( sizeof( asset::VertexDescription_Std430Alignment ) )
                .SetInputRate( renderer::rhi::InputRate::ePerVertex )
        };

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
            .SetByteSize( MKT_SIZEOF( ConstantBuffer ) ) };
        mConstantBuffer = mDevice->CreateBuffer( constantBufferDesc );

        auto samplerDes{ renderer::rhi::SamplerCreateDescription{}
            .SetFilter( renderer::rhi::SamplerFilter::eNearest )
            .SetWrap( renderer::rhi::SamplerWrapMode::eRepeat )
            .SetBorderColor( renderer::rhi::kColorWhite ) };
        mSamplerState = mDevice->CreateSampler( samplerDes );

        // Create the command list
        mCommandList = mDevice->CreateCommandList( renderer::rhi::QueueType::eGraphics );
        mCommandList->SetEnableAutomaticBarriers( true );
        mCommandList->SetDebugName( "GameLayer CommandList" );

        // We will upload a texture and a buffer to do some effects, see Triangle_Frag
        // Ideally we want to automate this process by allowing each backend to be able to use shader reflection
        auto layoutDesc{ renderer::rhi::BindingLayoutDescription{}
            .SetRegisterSpace( 0 )
            .SetShaderVisibility( renderer::rhi::ShaderFlagsBits::All )
            .AddItem( renderer::rhi::BindingLayoutItem::Sampler( 0 ) )
            .AddItem( renderer::rhi::BindingLayoutItem::TextureSRV( 1 ) )
            .AddItem( renderer::rhi::BindingLayoutItem::ConstantBuffer( 2 ) ) };
        mBindingLayoutHandle = mDevice->CreateBindingLayout( layoutDesc );

        // Bindless setup
        auto bindlessLayout{ renderer::rhi::DescriptorTableLayoutDescription{}
            .SetVisibility( renderer::rhi::ShaderFlagsBits::All )
            .SetRegisterSpace( 1 )
            .AddBindlessItem( renderer::rhi::BindlessLayoutItem::TextureSRV( 0, 1024 ) )
            .AddBindlessItem( renderer::rhi::BindlessLayoutItem::Samplers( 1, 1024 ) ) };
        mBindlessLayout = mDevice->CreateDescriptorTableLayout( bindlessLayout );

        // A pipeline layout describes what kind of group of resources we can bind
        // To a specific bind point. We can bind resources for Compute pipelines or Graphics pipelines, etc
        // This is handy if we have too many pipelines that share same layout for group of resources
        // we can just bind the resources once for all subsequent draws as long as the pipelines use same layout.
        mPipelineLayoutHandle = mDevice->CreatePipelineLayout( renderer::rhi::PipelineLayoutCreateDescription{}
            .AddBindingLayout( mBindingLayoutHandle )
            .AddBindingLayout( mBindlessLayout ) );

        auto graphicsPipelineDescription{ renderer::rhi::GraphicsPipelineDescription{}
            .AddShader( mPixelShader )
            .AddShader( mVertexShader )
            .SetInputLayout( mVertexInputLayout )

            .SetDepthFormat( renderer::rhi::Format::eD32 )
            .AddColorFormat( renderer::rhi::Format::eBGRA8_UNORM )

            .SetUseReflection( false )

            .SetPolygonMode( renderer::rhi::PolygonMode::eFill )
            .SetCullMode( renderer::rhi::CullMode::eCullBack )
            .SetWindingOrder( renderer::rhi::WindingOrder::eCounterClockwise )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .SetPipelineLayout( mPipelineLayoutHandle ) };
        mPipeline = mDevice->CreatePipeline( graphicsPipelineDescription );

        mDescriptorTable = mDevice->CreateDescriptorTable( mBindlessLayout );

        auto bindingSetDesc{ renderer::rhi::BindingTableDescription{}
            .AddItem( renderer::rhi::BindingTableItem::Sampler( 0, mSamplerState.GetPtr() ) )
            .AddItem( renderer::rhi::BindingTableItem::TextureSRV( 1, mSimpleTexture.GetPtr() ) )
            .AddItem( renderer::rhi::BindingTableItem::ConstantBuffer( 2, mConstantBuffer.GetPtr() ) ) };
        mBindingSetHandle = mDevice->CreateBindingTable( bindingSetDesc, mBindingLayoutHandle );

        ( void )mDevice->WriteDescriptorTable( mDescriptorTable, renderer::rhi::BindingTableItem::Sampler( 0, mSamplerState.GetPtr() ) );
        ( void )mDevice->WriteDescriptorTable( mDescriptorTable, renderer::rhi::BindingTableItem::TextureSRV( 1, mSimpleTexture.GetPtr() ) );

        scene::SceneCameraDescription cameraDescription{
            .mFov = 45.0,
            .mAspectRatio = core::as<float>( mWindow->GetWidth() ) / core::as<float>( mWindow->GetHeight() ),
            .mNearPlane = 0.1f,
            .mFarPlane = 3000.0f,
            .mWindow = mWindow
        };

        mEditorCamera = core::Ref<scene::SceneCamera>::New( cameraDescription );

        asset::AssetRegistryService::Get()->LoadAssetAsync<asset::Model>( "Resources/Prefabs/deadpool/scene.gltf" );
        asset::AssetRegistryService::Get()->LoadAssetAsync<asset::Model>( "Resources/Prefabs/miss_galaxy/scene.gltf" );
        asset::AssetRegistryService::Get()->LoadAssetAsync<asset::Model>( "Resources/Prefabs/bee/scene.gltf" );

        asset::AssetRegistryService::Get()->LoadAssetAsync<asset::Model>( "Resources/Prefabs/robot/gltf/scene.gltf" );

        mModelHandle = asset::AssetRegistryService::Get()->LoadAsset<asset::Model>( "Resources/Models/Prefabs/cone/gltf/scene.gltf" );
    }

    auto EditorDebugLayer::UpdateCameraState( float ts ) -> void {
        MKT_BEGIN_PROFILER();

        mEditorCamera->SetMovementSpeed( 30 );
        mEditorCamera->SetRotationSpeed( 30 );

        mEditorCamera->SetFarPlane( 0.01 );
        mEditorCamera->SetNearPlane( 2000 );

        mEditorCamera->WantRotation( true, true );

        mEditorCamera->SetFieldOfView( 45 );

        // Set viewport to the currently active window we can either expand
        // the final composition to occupy the whole screen or just an ImGui viewport
        mEditorCamera->SetViewportSize( mWindow->GetWidth(), mWindow->GetHeight() );

        if ( platform::InputSystem::Get()->IsMouseKeyPressed( platform::Mouse_Button_Right ) ) {
            mEditorCamera->EnableCamera( true );
        }
        else {
            mEditorCamera->EnableCamera( false );
        }

        mEditorCamera->Update( ts );
    }

    auto EditorDebugLayer::OnDestroy() -> void {
        MKT_BEGIN_PROFILER();
        // Ensure GPU is done
        mDevice->WaitIdle();

        mPipeline.Reset();
        mPipelineLayoutHandle.Reset();
        mBindingLayoutHandle.Reset();
        mVertexInputLayout.Reset();

        mBindlessLayout.Reset();
        mDescriptorTable.Reset();

        mVertexShader.Reset();
        mPixelShader.Reset();

        mConstantBuffer.Reset();

        mSimpleTexture.Reset();
        mColorImage.Reset();
        mDepthImage.Reset();

        mSamplerState.Reset();

        mBindingSetHandle.Reset();

        mModelHandle.Reset();

        mCommandList.Reset();
    }

    auto EditorDebugLayer::OnRender( float ) -> void {
    }

    auto EditorDebugLayer::GetName() const -> eastl::string_view {
        return mName;
    }

    auto EditorDebugLayer::OnEvent( platform::Event& event ) -> void {
        MKT_BEGIN_PROFILER();

        if ( event.IsType( platform::EventType::MOUSE_BUTTON_PRESSED_EVENT ) ) {
            auto* mouseButtonEvent{
                core::as<platform::MouseButtonPressedEvent*>( MKT_ADDRESSOF( event ) )
            };

            if ( mouseButtonEvent->GetMouseButton() == platform::Mouse_Button_Right ) {
                const float angularSpeed{ 15.0f };

                // Example: rotate around Y (local)
                mRotation.y += angularSpeed;
                mCameraProps.mModel = math::RecomputeTransform( mPosition, mScale, mRotation, mPivot );
            }
        }
    }
}// namespace mikoto::editor
