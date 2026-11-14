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
#include <Core/Layer.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/TimeService.hh>
#include <Core/LocalizationService.hh>

#include <Platform/Event.hh>
#include <Platform/CoreEvents.hh>

#include <Assets/Image.hh>
#include <Assets/Utility.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Core/RenderSystem.hh>

#include <ImGui/Utility.hh>
#include <ImGui/ImGuiWidget.hh>

#include <Layers/EditorHelloTriangleLayer.hh>

namespace mikoto::editor {
    using namespace mikoto::core;
    using namespace mikoto::asset;
    using namespace mikoto::scene;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;
    using namespace mikoto::filesystem;

    EditorHelloTriangleLayer::EditorHelloTriangleLayer( platform::IWindow *window )
        : mName{ "EditorHelloTriangleLayer" }, mWindow{ window } {
        MKT_BEGIN_PROFILER();
        mDevice = renderer::RenderSystem::Get()->GetGpuDevice();
    }

    auto EditorHelloTriangleLayer::OnCreate() -> void {
        MKT_BEGIN_PROFILER();
        // Create color attachment
        auto colorDesc{ renderer::rhi::TextureCreateDescription{}
            .SetWidth( core::as<core::i32>( 1920 ) )
            .SetHeight( core::as<core::i32>( 1080 ) )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetMultisampling( renderer::rhi::Multisampling::eMsaaX1 )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget | renderer::rhi::TextureUsageFlagsBits::ShaderResource )
            .SetFormat( renderer::rhi::Format::eBGRA8_UNORM ) };
        mColorImage = mDevice->CreateTexture( colorDesc );
        mColorImage->SetDebugName( "HelloTriangleLayer Color image" );

        // Create depth attachment
        auto depthDesc{ renderer::rhi::TextureCreateDescription{}
            .SetWidth( core::as<core::i32>( 1920 ) )
            .SetHeight( core::as<core::i32>( 1080 ) )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetMultisampling( renderer::rhi::Multisampling::eMsaaX1 )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::DepthTarget )
            .SetFormat( renderer::rhi::Format::eD32 ) };
        mDepthImage = mDevice->CreateTexture( depthDesc );
        mDepthImage->SetDebugName( "HelloTriangleLayer Depth image" );

        // Create shaders
        filesystem::FileHandle vsShader{ filesystem::FileService::Get()->LoadFile( "Resources/Shaders/slang/HelloWorld_Vert.slang" ) };
        auto vertexShaderDescription{ renderer::rhi::ShaderModuleCreateDescription{}
            .SetContents( vsShader )
            .SetModuleName( vsShader->GetName() )
            .SetModulePath( vsShader->GetPath() )
            .SetLanguage( renderer::rhi::ShaderLanguage::eSlang )
            .SetStage( renderer::rhi::ShaderType::eVertex ) };
        mVertexShader = mDevice->CreateShader( vertexShaderDescription );

        filesystem::FileHandle pxShader{ filesystem::FileService::Get()->LoadFile( "Resources/Shaders/slang/HelloWorld_Frag.slang" ) };
        auto fragmentShaderDescription{ renderer::rhi::ShaderModuleCreateDescription{}
            .SetContents( pxShader )
            .SetModuleName( pxShader->GetName() )
            .SetModulePath( pxShader->GetPath() )
            .SetLanguage( renderer::rhi::ShaderLanguage::eSlang )
            .SetStage( renderer::rhi::ShaderType::ePixel ) };
        mPixelShader = mDevice->CreateShader( fragmentShaderDescription );

        // Create pipeline

        auto graphicsPipelineDescription{ renderer::rhi::GraphicsPipelineDescription{}
            .AddShader( mPixelShader )
            .AddShader( mVertexShader )

            .SetDepthFormat( renderer::rhi::Format::eD32 )
            .AddColorFormat( renderer::rhi::Format::eBGRA8_UNORM )

            .SetUseReflection( false )

            .SetPolygonMode( renderer::rhi::PolygonMode::eFill )
            .SetWindingOrder( renderer::rhi::WindingOrder::eCounterClockwise )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList ) };
        mPipeline = mDevice->CreatePipeline( graphicsPipelineDescription );
        mPipeline->SetDebugName( "HelloTriangleLayer Pipeline" );

        // Create the command list
        mCommandList = mDevice->CreateCommandList( renderer::rhi::QueueType::eGraphics );
        mCommandList->SetEnableAutomaticBarriers( true );
        mCommandList->SetDebugName( "HelloTriangleLayer CommandList" );
    }

    auto EditorHelloTriangleLayer::OnDestroy() -> void {
        MKT_BEGIN_PROFILER();
        mDevice->WaitIdle();

        mPipeline.Reset();

        mVertexShader.Reset();
        mPixelShader.Reset();

        mColorImage.Reset();
        mDepthImage.Reset();

        mCommandList.Reset();
    }

    auto EditorHelloTriangleLayer::OnUpdate( float timeStep ) -> void {
        MKT_BEGIN_PROFILER();
        mCommandList->Open( { .mScopeName = "EditorHelloTriangleLayer Render" } );

        // Set graphics state
        auto graphicsState{ renderer::rhi::RenderPassDescription{}
            .SetRenderArea( renderer::rhi::Rect{ 1920, 1080 } )
            .AddDepthTarget( mDepthImage )
            .AddRenderTarget( mColorImage, renderer::rhi::Color{ 1.0f, 0.2f, 0.4f, 1.0f } ) };
        mCommandList->BeginRenderPass( graphicsState );

        mCommandList->BindPipeline( mPipeline.GetPtr() );

        mCommandList->SetViewportState( renderer::rhi::ViewportState{}
            .AddViewportAndScissorRect( Viewport( 1920, 1080 ) ) );

        constexpr auto drawArguments{ renderer::rhi::DrawArguments{}
            .SetInstanceCount( 1 )
            .SetVertexCount( 3 ) };
        mCommandList->Draw( drawArguments );

        mCommandList->EndRenderPass();

        mCommandList->SetTransition( { mColorImage.GetPtr(), renderer::rhi::ResourceStates::eShaderResource } );

        mCommandList->Close();

        auto submitInfo{ renderer::rhi::SubmitInfo{}
            .AddCommandList( mCommandList ) };
        renderer::RenderSystem::Get()->BatchSubmission( eastl::move( submitInfo ), renderer::rhi::QueueType::eGraphics );

        if ( mIsImguiWindowActive ) {
            DisplayImGuiWindow();
        }
    }

    auto EditorHelloTriangleLayer::OnRender( float ) -> void {
    }

    auto EditorHelloTriangleLayer::GetName() const -> eastl::string_view {
        return mName;
    }

    auto EditorHelloTriangleLayer::OnEvent( platform::Event &event ) -> void {
        MKT_BEGIN_PROFILER();

        if ( event.IsType( platform::EventType::KEY_PRESSED_EVENT ) ) {
            if ( const auto *keyPressed{ dynamic_cast<platform::KeyPressedEvent *>( MKT_ADDRESSOF( event ) ) } ) {
                if ( keyPressed->GetKeyCode() == platform::KeyCode::Key_H ) {
                    mIsImguiWindowActive = !mIsImguiWindowActive;
                }
            }
        }
    }

    auto EditorHelloTriangleLayer::DisplayImGuiWindow() -> void {
        MKT_BEGIN_PROFILER();
        ImGui::SetNextWindowSize( ImVec2( 420.0f, 500.0f ), ImGuiCond_FirstUseEver );

        if ( imgui::BeginWindow( imgui::widget::LocalizedLabel( "editor_hello_triangle_layer_hello_triangle_tests", "Hello Triangle Tests" ).c_str(), &mIsImguiWindowActive ) ) {
            auto imageID{ imgui::ImGuiService::Get()->GetTextureID( mColorImage ) };
            ImGui::Image( imageID, ImVec2{ 1280, 720 } );

            ImGui::Spacing();
            ImGui::Separator();

            ImGui::Text( "FPS: %.1f", ImGui::GetIO().Framerate );
            ImGui::Text( MKT_LOC( "editor_hello_triangle_layer_frame_time_ms" ).c_str(), 1000.0f / ImGui::GetIO().Framerate );

            ImGui::Spacing();
            ImGui::Separator();

            static core::f32 rotationSpeed{ 1.0f };
            static eastl::array<core::f32, 4> clearColor{ 0.10f, 0.10f, 0.12f, 1.0f };

            ImGui::SliderFloat( imgui::widget::LocalizedLabel( "editor_hello_triangle_layer_rotation_speed", "Rotation Speed" ).c_str(), &rotationSpeed, 0.0f, 10.0f );
            ImGui::ColorEdit4( imgui::widget::LocalizedLabel( "editor_hello_triangle_layer_clear_color", "Clear Color" ).c_str(), clearColor.data() );

            ImGui::Spacing();
            ImGui::Separator();

            ImGui::TextDisabled( MKT_LOC( "editor_hello_triangle_layer_press_h_to_hide_show_this_window" ).c_str() );
        }

        ImGui::End();
    }
}
