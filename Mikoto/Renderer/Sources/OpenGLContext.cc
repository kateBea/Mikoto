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

#include <utility>

#include <EASTL/utility.h>
#include <EASTL/vector.h>

#include <Core/Core.hh>
#include <Core/CVar.hh>
#include <Core/Exception.hh>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

#include <Platform/Window.hh>

#include <Renderer/Rhi/OpenGL/OpenGLHelpers.hh>
#include <Renderer/Rhi/OpenGL/OpenGLContext.hh>
#include <Renderer/Rhi/OpenGL/OpenGLTexture.hh>

namespace mikoto::renderer::opengl {

    auto Context::Init() -> bool {
        mWindowContext = mWindow ? mWindow->GetOpenGLContext() : nullptr;
        if ( !mWindowContext ) {
            MKT_THROW_INVALID_ARGUMENT( "The window does not provide an OpenGL context." );
        }

        const eastl::string deviceTypeString{ core::CVarRegistry::Get()->GetValue<eastl::string>( "device_type" ) };
        const renderer::rhi::GpuDeviceType deviceType{ rhi::InferGpuDeviceType( deviceTypeString ) };

        MKT_ASSERT( deviceType != renderer::rhi::GpuDeviceType::eInvalid, "Invalid device type." );

        // Init the device when the context is ready
        mDevice = renderer::rhi::IGpuDevice::Create( {
            .mApi = renderer::rhi::GraphicsAPI::eOpenGL,
            .mDeviceType = deviceType,
            .mFeatureSupportFlags = GetRequestedDeviceFeatures(),
            .mWindow = mWindow
        } );

        if ( !mDevice ) {
            MKT_CORE_LOGGER_ERROR( "Could not initialize OpenGL GPU Device." );
            return false;
        }

        mDevice->Initialize();

        glCreateFramebuffers( 1, &mPresentFramebuffer );
        
        SetRefreshRate( mRefreshRate );
        SetupDebugCallback();

        return true;
    }

    auto Context::SetupDebugCallback() -> void {
#if MIKOTO_DEBUG
        glEnable( GL_DEBUG_OUTPUT );
        glEnable( GL_DEBUG_OUTPUT_SYNCHRONOUS );

        // https://registry.khronos.org/OpenGL-Refpages/gl4/html/glDebugMessageCallback.xhtml
        glDebugMessageCallback(
            []( GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam ) {
                const auto *context{ static_cast<const Context *>( userParam ) };
                if ( severity == GL_DEBUG_SEVERITY_NOTIFICATION ) {
                    MKT_CORE_LOGGER_DEBUG( "OpenGL Debug: {}", message );
                } else if ( severity == GL_DEBUG_SEVERITY_LOW ) {
                    MKT_CORE_LOGGER_INFO( "OpenGL Info: {}", message );
                } else if ( severity == GL_DEBUG_SEVERITY_MEDIUM ) {
                    MKT_CORE_LOGGER_WARN( "OpenGL Warn: {}", message );
                } else if ( severity == GL_DEBUG_SEVERITY_HIGH ) {
                    MKT_CORE_LOGGER_ERROR( "OpenGL Error: {}", message );
                }
            }, this );

        // https://registry.khronos.org/OpenGL-Refpages/gl4/html/glDebugMessageControl.xhtml
        glDebugMessageControl( GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE );

        // Debug test message
        glDebugMessageInsert( GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 1, GL_DEBUG_SEVERITY_HIGH, -1, "Hello from OpenGL debug output!" );
#endif
    }

    auto Context::Shutdown() -> void {
        if ( !mDevice ) {
            return;
        }

        MakeCurrent();
        mDevice->WaitIdle();
        {
            std::lock_guard lock{ mMutex };
            mSubmissions.clear();
            mOverlay = {};
        }

        mPresentTarget.Reset();

        glDeleteFramebuffers( 1, &mPresentFramebuffer );

        mPresentFramebuffer = 0;

        if ( mDevice ) {
            mDevice->Shutdown();
            mDevice.reset();
        }

        mWindowContext = nullptr;
    }

    auto Context::PrepareFrame() -> void {
        MakeCurrent();
        mDevice->RunGarbageCollection();
    }

    auto Context::Update() -> void {
    }

    auto Context::BatchSubmission( renderer::rhi::SubmitInfo &&info, renderer::rhi::QueueType queue ) -> void {
        std::lock_guard lock{ mMutex };
        mSubmissions.push_back( { eastl::move( info ), queue } );
    }

    auto Context::SetOverlay( eastl::function<void()> draw ) -> void {
        std::lock_guard lock{ mMutex };
        mOverlay = eastl::move( draw );
    }

    auto Context::SubmitFrame() -> void {
        MakeCurrent();
        eastl::vector<Submission> submissions{};
        eastl::function<void()> overlay{};

        {
            std::lock_guard lock{ mMutex };
            submissions.swap( mSubmissions );
            overlay = eastl::move( mOverlay );
        }

        // Preserve producer/consumer submission order across logical queues.

        for ( const auto &submission: submissions ) {
            auto *queue{ mDevice->GetQueue( submission.mQueue ) };

            if ( !queue ) {
                MKT_THROW_INVALID_ARGUMENT( "Unsupported OpenGL submission queue." );
            }

            queue->ExecuteCommandLists( submission.mInfo );
        }

        if ( overlay ) {
            overlay();
        }
    }

    auto Context::SetPresentTarget( renderer::rhi::TextureHandle texture ) -> void {
        mPresentTarget = eastl::move( texture );
    }

    auto Context::SetRefreshRate( renderer::rhi::RefreshRate rate ) -> void {
        mRefreshRate = rate;
        MakeCurrent();

        if ( !mWindowContext->SetSwapInterval( rate == rhi::RefreshRate::eSync ? 1 : 0 ) ) {
            MKT_CORE_LOGGER_WARN( "The window backend could not set the OpenGL swap interval." );
        }
    }

    auto Context::MakeCurrent() -> void {
        if ( !mWindowContext || !mWindowContext->MakeCurrent() ) {
            MKT_THROW_RUNTIME_ERROR( "The window backend could not make the OpenGL context current." );
        }
    }

    auto Context::Present() -> void {
        if ( mWindow->GetWidth() <= 0 || mWindow->GetHeight() <= 0 ) {
            return;
        }

        MakeCurrent();

        if ( mPresentTarget ) {
            const auto *texture{ dynamic_cast<const Texture *>( mPresentTarget.GetPtr() ) };

            if ( !texture || texture->GetDimension() != renderer::rhi::TextureDimension::eTexture2D || opengl::GetFormat( texture->GetFormat() ).mDepth ) {
                MKT_THROW_INVALID_ARGUMENT( "Presentation requires an OpenGL 2D color texture." );
            }

            glNamedFramebufferTexture( mPresentFramebuffer, GL_COLOR_ATTACHMENT0, texture->Name(), 0 );
            glNamedFramebufferReadBuffer( mPresentFramebuffer, GL_COLOR_ATTACHMENT0 );
            glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0 );
            glDrawBuffer( GL_BACK );
            glDisable( GL_SCISSOR_TEST );
            glDisable( GL_FRAMEBUFFER_SRGB );
            glBlitNamedFramebuffer( mPresentFramebuffer, 0, 0, 0, texture->GetWidth(), texture->GetHeight(), 0, 0, mWindow->GetWidth(),
                mWindow->GetHeight(), GL_COLOR_BUFFER_BIT,
                opengl::GetFormat( texture->GetFormat() ).mInteger ? GL_NEAREST : GL_LINEAR );
            CheckError( "presentation blit" );
        }

        if ( !mWindowContext->SwapBuffers() ) {
            MKT_THROW_RUNTIME_ERROR( "The window backend could not swap OpenGL buffers." );
        }
    }

    Context::Context( const RenderContextCreateInfo &info )
        : RenderContext{ info } {
    }
}// namespace mikoto::renderer::opengl
