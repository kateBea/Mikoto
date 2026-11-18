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
#include <array>
#include <thread>
#include <algorithm>
#include <exception>

#include <slang.h>
#include <slang-com-ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Logging/Logger.hh>

#include <Threading/TaskService.hh>

#include <Renderer/Core/RenderSystem.hh>
#include <Renderer/Core/SlangCompiler.hh>

#include <ImGui/ImGuiService.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;

    // Hash the actual session configuration, including target, defines and options.
    // https://docs.shader-slang.org/en/latest/compilation-api.html
    static auto ShaderSessionKey( slang::IGlobalSession &global, slang::SessionDesc &description ) -> eastl::string {
        Slang::ComPtr<slang::IBlob> digest{};
        if ( !SLANG_SUCCEEDED( global.getSessionDescDigest( &description, digest.writeRef() ) ) || !digest ) {
            MKT_THROW_RUNTIME_ERROR( "Cannot fingerprint the shader compiler configuration." );
        }
        eastl::string key{ global.getBuildTagString() };
        constexpr char digits[]{ "0123456789abcdef" };
        const auto *bytes{ static_cast<const unsigned char *>( digest->getBufferPointer() ) };
        for ( usize i{}; i < digest->getBufferSize(); ++i ) {
            key += digits[bytes[i] >> 4];
            key += digits[bytes[i] & 15];
        }
        return key;
    }


    RenderSystem::RenderSystem( const RenderSystemCreateInfo& options )
        : mWindow{ options.mWindow }, mApi{ options.mApi }, mRefreshRate{ options.mRefreshRate }, mEnableImGui{ options.mEnableImGui } {
    }

    auto RenderSystem::Initialize() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_INFO( "Initializing RenderSystem..." );

        // Backends create compiler contexts lazily when they acquire a lease.
        InitializeSlang();

        InitContext();


        mIsInitialized = true;
    }

    auto RenderSystem::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !mIsInitialized ) {
            return;
        }

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down RenderService..." );

        {
            std::unique_lock lock{ mSlangMutex };
            mSlangStopping = true;
            mSlangAvailable.notify_all();
            mSlangAvailable.wait( lock, [this] {
                return std::none_of( mSlangCompilers.begin(), mSlangCompilers.end(),
                    []( const auto& context ) {
                    return context->mInUse;
                } );
            } );
        }

        GetGpuDevice()->WaitIdle();

        if ( mImguiService && mEnableImGui ) {
            mImguiService->Shutdown();
            mImguiService.reset();
        }

        mContext->Shutdown();
        mContext = nullptr;

        // No backend retains Slang objects. Destroy sessions and their globals
        // before unloading the compiler's process-wide state.
        mSlangCompilers.clear();
        slang::shutdown();

        mIsInitialized = false;
    }

    auto RenderSystem::Update( float ts ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        mContext->Update();
    }

    auto RenderSystem::BatchSubmission( rhi::SubmitInfo&& submitInfo, rhi::QueueType queue ) -> void {
        MKT_BEGIN_PROFILER();
        mContext->BatchSubmission( eastl::move( submitInfo ), queue );
    }

    auto RenderSystem::PrepareFrame() -> void {
        MKT_BEGIN_PROFILER();

        mContext->PrepareFrame();

        // Using the GUI library is optional, we
        // need to check for availability
        if ( mEnableImGui && mImguiService ) {
            mImguiService->PrepareFrame();
        }
    }

    auto RenderSystem::SubmitFrame() -> void {
        MKT_BEGIN_PROFILER();

        if ( mEnableImGui && mImguiService ) {
            mImguiService->EndFrame();
        }

        mContext->SubmitFrame();
    }

    auto RenderSystem::PresentFrame() -> void {
        MKT_BEGIN_PROFILER();
        mContext->Present();
    }

    auto RenderSystem::SetPresentTarget( renderer::rhi::TextureHandle texture ) -> void {
        MKT_BEGIN_PROFILER();
        return mContext->SetPresentTarget( texture );
    }

    auto RenderSystem::AcquireSlangSession() -> SlangSessionLease {
        MKT_BEGIN_PROFILER();
        SlangCompilerContext* available{};
        {
            std::unique_lock lock{ mSlangMutex };
            mSlangAvailable.wait( lock, [this] {
                return mSlangStopping || mSlangCompilers.size() < mSlangSessionLimit ||
                       std::any_of( mSlangCompilers.begin(), mSlangCompilers.end(),
                           []( const auto& context ) {
                    return !context->mInUse;
                } );
            } );

            if ( mSlangStopping ) {
                MKT_THROW_RUNTIME_ERROR( "The Slang session pool is stopped." );
            }

            for ( const auto& context: mSlangCompilers ) {
                if ( !context->mInUse ) {
                    available = context.get();
                    break;
                }
            }

            if ( !available ) {
                mSlangCompilers.push_back( eastl::make_unique<SlangCompilerContext>() );
                available = mSlangCompilers.back().get();
            }

            available->mInUse = true;
        }

        // Session creation and compilation never hold the availability mutex.
        SlangSessionLease lease{ *this, *available };

        if ( !available->mSession ) {
            PrepareSlangSession( *available );
        }

        return lease;
    }

    auto RenderSystem::ReleaseSlangSession( SlangCompilerContext& context ) -> void {
        const std::lock_guard lock{ mSlangMutex };
        context.mInUse = false;

        if ( mSlangStopping ) {
            mSlangAvailable.notify_all();
        }
        else {
            mSlangAvailable.notify_one();
        }
    }

    auto RenderSystem::GetContext() -> RenderContext* {
        MKT_BEGIN_PROFILER();
        return mContext.get();
    }

    auto RenderSystem::GetContext() const -> const RenderContext* {
        MKT_BEGIN_PROFILER();
        return mContext.get();
    }

    auto RenderSystem::GetGpuDevice() -> renderer::rhi::IGpuDevice* {
        MKT_BEGIN_PROFILER();
        return mContext->GetGpuDevice();
    }

    auto RenderSystem::GetGpuDevice() const -> const renderer::rhi::IGpuDevice* {
        MKT_BEGIN_PROFILER();
        return mContext->GetGpuDevice();
    }

    auto RenderSystem::GetActiveGraphicsApi() const -> renderer::rhi::GraphicsAPI {
        MKT_BEGIN_PROFILER();
        return mApi;
    }

    auto RenderSystem::InitializeSlang() -> void {
        MKT_BEGIN_PROFILER();
        const auto* tasks{ threading::TaskService::GetPtr() };
        const auto concurrency{ tasks && tasks->IsInitialized()
                                    ? tasks->GetWorkersCount()
                                    : std::thread::hardware_concurrency() };
        const std::lock_guard lock{ mSlangMutex };
        mSlangSessionLimit = std::max<core::usize>( 1, concurrency );
        mSlangCompilers.reserve( mSlangSessionLimit );
        mSlangStopping = false;
    }

    auto RenderSystem::IsApiActive( const renderer::rhi::GraphicsAPI api ) const -> bool {
        MKT_BEGIN_PROFILER();
        return mApi == api;
    }

    auto RenderSystem::InitContext() -> void {
        MKT_BEGIN_PROFILER();
        const RenderContextCreateInfo createInfo{
            .mWindow = mWindow,
            .mRefreshRate = mRefreshRate,
            .mApi = mApi
        };

        mContext = RenderContext::Create( createInfo );

        if ( !mContext ) {
            MKT_THROW_RUNTIME_ERROR( "No valid context could be selected" );
        }

        if ( !mContext->Init() ) {
            MKT_CORE_LOGGER_CRITICAL( "RenderSystem::Init - Could not initialize Render context." );
        }
        else {
            // If we managed to create a valid API
            // context we can initialize the GUI library
            if ( mEnableImGui && mWindow ) {
                InitGuiService();
            }
        }
    }

    auto RenderSystem::InitGuiService() -> void {
        MKT_BEGIN_PROFILER();
        // Imgui service
        imgui::ImGuiServiceDescription imguiServiceCreateInfo{
            .mWindow = mWindow,
            .mDevice = GetGpuDevice(),
            .mApi = mApi
        };
        mImguiService = eastl::make_unique<imgui::ImGuiService>( imguiServiceCreateInfo );
        mImguiService->Initialize();
    }

    auto RenderSystem::PrepareSlangSession( SlangCompilerContext& context ) -> void {
        MKT_BEGIN_PROFILER();

        if ( SLANG_FAILED( slang::createGlobalSession( context.mGlobal.writeRef() ) ) ) {
            MKT_THROW_RUNTIME_ERROR( "Cannot create the Slang global session." );
        }

        const char* profile{};
        const char* apiMacro{};

        switch ( mApi ) {
            case rhi::GraphicsAPI::eVulkan:
                context.mTargets[0].format = SLANG_SPIRV;
                profile = "spirv_1_6";
                apiMacro = "MKT_SUPPORT_VULKAN";
                break;
            case rhi::GraphicsAPI::eOpenGL:
                // Translate SPIR-V through SPIRV-Cross for OpenGL samplers.
                context.mTargets[0].format = SLANG_SPIRV;
                profile = "spirv_1_5";
                apiMacro = "MKT_SUPPORT_OPENGL";
                break;
            case rhi::GraphicsAPI::eD3D11:
                context.mTargets[0].format = SLANG_HLSL;
                profile = "sm_5_0";
                apiMacro = "MKT_SUPPORT_D3D11";
                break;
            case rhi::GraphicsAPI::eD3D12:
                context.mTargets[0].format = SLANG_HLSL;
                profile = "sm_6_8";
                apiMacro = "MKT_SUPPORT_D3D12";
                break;
            default:
                MKT_THROW_RUNTIME_ERROR( "No Slang target for the selected graphics API." );
        }

        // Find profile
        context.mTargets[0].profile = context.mGlobal->findProfile( profile );

        // Setup debug info level, for developement purposes we include debug info in the shaders
        // Might consided stripping from distribution builds.
#if defined( NDEBUG )
        constexpr auto debugInfoLevel{ SLANG_DEBUG_INFO_LEVEL_STANDARD };// SLANG_DEBUG_INFO_LEVEL_NONE
#else
        constexpr auto debugInfoLevel{ SLANG_DEBUG_INFO_LEVEL_MAXIMAL };
#endif
        context.mOptions[0] = { slang::CompilerOptionName::ForceCLayout, { slang::CompilerOptionValueKind::Int, 1 } };
        context.mOptions[1] = { slang::CompilerOptionName::Optimization, { slang::CompilerOptionValueKind::Int, SLANG_OPTIMIZATION_LEVEL_MAXIMAL } };
        context.mOptions[2] = { slang::CompilerOptionName::DebugInformation, { slang::CompilerOptionValueKind::Int, debugInfoLevel } };

        // Index into last option
        core::u32 optionCount{ 3 };

        // For vulkan spirv is emitted from slang shaders
        if ( mApi == rhi::GraphicsAPI::eVulkan ) {
            context.mOptions[optionCount++] = { slang::CompilerOptionName::EmitSpirvDirectly, { slang::CompilerOptionValueKind::Int, 1 } };
        }

        context.mMacros[0] = { apiMacro, "1" };
        SlangInt macroCount{ 1 };
#if !defined( NDEBUG )
        if ( mApi != rhi::GraphicsAPI::eOpenGL ) {
            context.mMacros[macroCount++] = { "MKT_DEBUG", "1" };
        }
#endif

        slang::SessionDesc description{
            .targets = context.mTargets.data(),
            .targetCount = core::as<SlangInt>( context.mTargets.size() ),
            .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
            .preprocessorMacros = context.mMacros.data(),
            .preprocessorMacroCount = macroCount,
            .compilerOptionEntries = context.mOptions.data(),
            .compilerOptionEntryCount = optionCount
        };

        context.mCacheKeys[0] = ShaderSessionKey( *context.mGlobal, description );
        if ( SLANG_FAILED( context.mGlobal->createSession( description, context.mSession.writeRef() ) ) ) {
            MKT_THROW_RUNTIME_ERROR( "Cannot create the Slang compilation session." );
        }

        if ( mApi == rhi::GraphicsAPI::eVulkan ) {
            // Balance compilation time and GPU code quality for the large ray program.
            // No optimization caused long driver compilation and slow tracing;
            // maximal optimization also expands this program excessively.
            // https://docs.shader-slang.org/en/latest/compilation-api.html

            context.mRayOptions = context.mOptions;
            context.mRayOptions[1].value.intValue0 = SLANG_OPTIMIZATION_LEVEL_DEFAULT;
            context.mRayOptions[2].value.intValue0 = SLANG_DEBUG_INFO_LEVEL_NONE;

            description.compilerOptionEntries = context.mRayOptions.data();
            
            context.mCacheKeys[2] = ShaderSessionKey( *context.mGlobal, description );
            if ( !SLANG_SUCCEEDED( context.mGlobal->createSession( description, context.mRaySession.writeRef() ) ) ) {
                MKT_THROW_RUNTIME_ERROR( "Cannot create the optimized Slang ray-generation session." );
            }

            description.compilerOptionEntries = context.mOptions.data();

            // Set precision before loading/optimizing modules for consistent
            // vertex positions in the depth, shadow and color passes.
            // https://github.com/shader-slang/slang/issues/12198
            context.mVertexTargets = context.mTargets;
            context.mVertexTargets[0].floatingPointMode = SLANG_FLOATING_POINT_MODE_PRECISE;
            description.targets = context.mVertexTargets.data();

            context.mCacheKeys[1] = ShaderSessionKey( *context.mGlobal, description );
            if ( SLANG_FAILED( context.mGlobal->createSession( description, context.mVertexSession.writeRef() ) ) ) {
                MKT_THROW_RUNTIME_ERROR( "Cannot create the precise Slang vertex session." );
            }
        }
    }
}// namespace mikoto::renderer