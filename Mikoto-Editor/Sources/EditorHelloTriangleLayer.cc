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

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Core/RenderSystem.hh>

#include <Layers/EditorHelloTriangleLayer.hh>

namespace mikoto::editor {
    using namespace mikoto::core;
    using namespace mikoto::asset;
    using namespace mikoto::scene;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;
    using namespace mikoto::filesystem;

    EditorHelloTriangleLayer::EditorHelloTriangleLayer( platform::Window *window )
        : ILayer{ "EditorHelloTriangleLayer" }, mWindow{ window }
    {
        mDevice = RenderSystem::Get()->GetGpuDevice();
    }

    auto EditorHelloTriangleLayer::OnCreate() -> void {
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
        FileHandle vsShader{ FileService::Get()->LoadFile( "Resources/Shaders/slang/HelloWorld_Vert.slang" ) };
        auto vertexShaderDescription{ ShaderModuleCreateDescription{}
            .SetContents( vsShader )
            .SetModuleName( vsShader->GetName() )
            .SetModulePath( vsShader->GetPath() )
            .SetLanguage( ShaderLanguage::eSlang )
            .SetStage( ShaderType::eVertex ) };
        mVertexShader = mDevice->CreateShader( vertexShaderDescription );

        FileHandle pxShader{ FileService::Get()->LoadFile( "Resources/Shaders/slang/HelloWorld_Frag.slang" ) };
        auto fragmentShaderDescription{ ShaderModuleCreateDescription{}
            .SetContents( pxShader )
            .SetModuleName( pxShader->GetName() )
            .SetModulePath( pxShader->GetPath() )
            .SetLanguage( ShaderLanguage::eSlang )
            .SetStage( ShaderType::ePixel ) };
        mPixelShader = mDevice->CreateShader( fragmentShaderDescription );

        // Create pipeline

        // A pipeline that takes no resources
        mPipelineLayoutHandle = mDevice->CreatePipelineLayout( PipelineLayoutCreateDescription{} );

        auto graphicsPipelineDescription{ GraphicsPipelineDescription{}
            .AddShader( mPixelShader )
            .AddShader( mVertexShader )

            .SetDepthFormat( Format::eD32 )
            .AddColorFormat( Format::eBGRA8_UNORM )

            .SetUseReflection( false )

            .SetPolygonMode( PolygonMode::eFill )
            .SetCullMode( CullMode::eCullBack )
            .SetWindingOrder( WindingOrder::eCounterClockwise )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .SetPipelineLayout( mPipelineLayoutHandle ) };
        mPipeline = mDevice->CreatePipeline( graphicsPipelineDescription );
        mPipeline->SetDebugName( "HelloTriangleLayer Pipeline" );

        // Create the command list
        mCommandList = mDevice->CreateCommandList( QueueType::eGraphics );
        mCommandList->SetEnableAutomaticBarriers( true );
        mCommandList->SetDebugName( "HelloTriangleLayer CommandList" );
    }

    auto EditorHelloTriangleLayer::OnDestroy() -> void {
        mDevice->WaitIdle();

        mPipeline.Reset();
        mPipelineLayoutHandle.Reset();

        mVertexShader.Reset();
        mPixelShader.Reset();

        mColorImage.Reset();
        mDepthImage.Reset();

        mCommandList.Reset();
    }

    auto EditorHelloTriangleLayer::OnUpdate( float timeStep ) -> void {
        mCommandList->Begin( { .mScopeName = "EditorHelloTriangleLayer Render" } );

        // Set graphics state
        auto graphicsState{ GraphicsState{}
            .SetRenderArea( Rect{ 1920, 1080 } )
            .AddDepthTarget( mDepthImage )
            .AddRenderTarget( mColorImage, Color{ 1.0f, 0.2f, 0.4f, 1.0f } ) };
        mCommandList->BeginRendering( graphicsState );

        // No resources but still need to specify the layout for the following pipelines
        auto bindingDescription{ BindResourcesDescription{}
            .SetBindPoint( PipelineType::eGraphics )
            .SetPipelineLayout( mPipelineLayoutHandle.GetRaw() ) };
        mCommandList->BindPipelineResources( bindingDescription );
        mCommandList->BindPipeline( mPipeline.GetRaw() );

        mCommandList->SetViewportState( ViewportState{}
            .AddViewportAndScissorRect( Viewport( 1920, 1080 ) ) );

        constexpr auto drawArguments{ DrawArguments{}
            .SetInstanceCount( 1 )
            .SetVertexCount( 3 ) };
        mCommandList->Draw( drawArguments );

        mCommandList->EndRendering();

        mCommandList->SetTransition( mColorImage.GetRaw(), ResourceStates::eShaderResource );

        mCommandList->End();

        auto submitInfo{ SubmitInfo{}
            .AddCommandList( mCommandList ) };
        RenderSystem::Get()->BatchSubmission(eastl::move(submitInfo), QueueType::eGraphics);

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

            ImGui::TextDisabled( "Press H to hide/show this window." );
        }

        ImGui::End();
    }
}