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

// ImGuizmo requires ImGui declarations before its header.
#include <imgui.h>
#include <SDL3/SDL_video.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>

#include <ImGuizmo.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Exception.hh>

#include <Renderer/Core/RenderSystem.hh>
#include <Renderer/Rhi/OpenGL/OpenGLContext.hh>
#include <Renderer/Rhi/OpenGL/OpenGLTexture.hh>

#include <ImGui/ImGuiOpenGLBackend.hh>

namespace mikoto::imgui {

    using namespace mikoto::core;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;

    ImGuiOpenGLBackend::ImGuiOpenGLBackend( const ImGuiBackendCreateInfo& info )
        : ImGuiBackend{ info } {
    }

    auto ImGuiOpenGLBackend::Init() -> void {
        auto& context{ dynamic_cast<renderer::opengl::Context&>( *renderer::RenderSystem::Get()->GetContext() ) };
        context.MakeCurrent();
        auto* window{ GetSDLWindow() };
        auto glContext{ SDL_GL_GetCurrentContext() };

        // The renderer made the window's context current for SDL's viewport setup.
        // https://github.com/ocornut/imgui/blob/v1.92.9b-docking/examples/example_sdl3_opengl3/main.cpp
        if ( !glContext || SDL_GL_GetCurrentWindow() != window ||
             !ImGui_ImplSDL3_InitForOpenGL( window, glContext ) ) {
            MKT_THROW_RUNTIME_ERROR( "Failed to initialize ImGui SDL for OpenGL." );
        }

        if ( !ImGui_ImplOpenGL3_Init( "#version 460 core" ) ) {
            ImGui_ImplSDL3_Shutdown();
            MKT_THROW_RUNTIME_ERROR( "Cannot initialize the ImGui OpenGL renderer." );
        }

        glCreateFramebuffers( 1, &mFramebuffer );

        try {
            OnResize();
        } catch ( ... ) {
            glDeleteFramebuffers( 1, &mFramebuffer );
            mFramebuffer = 0;
            mColor.Reset();
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplSDL3_Shutdown();
            throw;
        }

        mIsInitialized = true;
    }

    auto ImGuiOpenGLBackend::OnResize() -> void {
        const auto width{ mWindow->GetWidth() };
        const auto height{ mWindow->GetHeight() };

        if ( width <= 0 || height <= 0 ) {
            return;
        }

        if ( mColor && mColor->GetWidth() == static_cast<core::u32>( width ) && mColor->GetHeight() == static_cast<core::u32>( height ) ) {
            return;
        }

        mColor = mDevice->CreateTexture( renderer::rhi::TextureCreateDescription{}
            .SetName( "ImGui OpenGL composition" )
            .SetWidth( width )
            .SetHeight( height )
            .SetFormat( renderer::rhi::Format::eRGBA8_UNORM )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget |
                           renderer::rhi::TextureUsageFlagsBits::ShaderResource |
                           renderer::rhi::TextureUsageFlagsBits::CopySource ) );

        glNamedFramebufferTexture( mFramebuffer, GL_COLOR_ATTACHMENT0,
            static_cast<GLuint>( mColor->GetNativeHandle( renderer::rhi::ObjectType::GL_Texture ).mInteger ), 0 );
        glNamedFramebufferDrawBuffer( mFramebuffer, GL_COLOR_ATTACHMENT0 );

        if ( glCheckNamedFramebufferStatus( mFramebuffer, GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
            MKT_THROW_RUNTIME_ERROR( "ImGui OpenGL framebuffer is incomplete." );
        }
    }

    auto ImGuiOpenGLBackend::Shutdown() -> void {
        if ( !mIsInitialized ) {
            return;
        }

        auto& context{ dynamic_cast<renderer::opengl::Context&>( *renderer::RenderSystem::Get()->GetContext() ) };
        context.SetOverlay( {} );
        context.MakeCurrent();

        mDevice->WaitIdle();

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();

        glDeleteFramebuffers( 1, &mFramebuffer );

        mFramebuffer = 0;
        mColor.Reset();
        mFrameTextures.clear();
        mIsInitialized = false;
    }

    auto ImGuiOpenGLBackend::BeginFrame() -> void {
        auto& context{ dynamic_cast<renderer::opengl::Context&>( *renderer::RenderSystem::Get()->GetContext() ) };
        context.MakeCurrent();

        mFrameTextures.clear();

        ImGui_ImplOpenGL3_NewFrame();

        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();
    }

    auto ImGuiOpenGLBackend::EndFrame() -> void {
        ImGui::Render();

        OnResize();

        auto& context{ dynamic_cast<renderer::opengl::Context&>( *renderer::RenderSystem::Get()->GetContext() ) };
        context.SetPresentTarget( mColor );
        context.SetOverlay( [this, context = &context] {
            if ( !mColor || mWindow->GetWidth() <= 0 || mWindow->GetHeight() <= 0 ) {
                return;
            }

            glBindFramebuffer( GL_FRAMEBUFFER, mFramebuffer );
            glDisable( GL_SCISSOR_TEST );
            glDisable( GL_FRAMEBUFFER_SRGB );
            glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
            glClipControl( GL_LOWER_LEFT, GL_ZERO_TO_ONE );
            glClearNamedFramebufferfv( mFramebuffer, GL_COLOR, 0, &mClearColor.x );

            ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData() );

            if ( ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable ) {
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
                context->MakeCurrent();
            }
        } );
    }

    auto ImGuiOpenGLBackend::GetFinalComposition() -> renderer::rhi::TextureHandle {
        return mColor;
    }

    auto ImGuiOpenGLBackend::ConstructImGuiTextureID( const renderer::rhi::ITexture* texture ) -> ImTextureID {
        if ( !texture ) {
            return 0;
        }

        if ( texture->GetDimension() != renderer::rhi::TextureDimension::eTexture2D ) {
            MKT_THROW_INVALID_ARGUMENT( "ImGui requires a non-multisampled 2D texture." );
        }

        // Keep sampled images alive until the queued ImGui draw has executed.
        mFrameTextures.emplace_back( const_cast<renderer::rhi::ITexture*>( texture ) );
        return static_cast<ImTextureID>( texture->GetNativeHandle( renderer::rhi::ObjectType::GL_Texture ).mInteger );
    }

    auto ImGuiOpenGLBackend::ConstructImGuiTextureID( renderer::rhi::TextureHandle texture ) -> ImTextureID {
        return ConstructImGuiTextureID( texture.GetPtr() );
    }
}