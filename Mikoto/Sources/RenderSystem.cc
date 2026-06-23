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

#include <slang.h>
#include <slang-com-ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Logging/Logger.hh>

#include <ImGui/ImGuiService.hh>

#include <Renderer/Core/RenderSystem.hh>

namespace mikoto::renderer {

    RenderSystem::RenderSystem(const RenderSystemCreateInfo& options)
        : mWindow{ options.mWindow }, mApi{ options.mApi },
        mRefreshRate{ options.mRefreshRate },
        mEnableImGui{ options.mEnableImGui }
    {}

    auto RenderSystem::Initialize() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_INFO("Initializing RenderSystem...");


        // Still need to decide whether this will also be available for HLSL
        // right is being used mostly for Vulkan, but it is also possible to use it for DX12, DX11, etc
        // https://github.com/shader-slang/slang
        InitializeSlang();

        InitContext();

        mIsInitialized = true;
    }

    auto RenderSystem::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!mIsInitialized) {
            return;
        }

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down RenderService..." );

        if (mImguiService && mEnableImGui) {
            mImguiService->Shutdown();
            mImguiService.reset();
        }

        mContext->Shutdown();
        mContext = nullptr;

        // TODO(kate): Test with memory leaks reports
        slang::shutdown();

        // ComPtr will may outlive the context
        mSlangCurrentSession = nullptr;
        mSlangGlobalSession = nullptr;

        mIsInitialized = false;
    }

    auto RenderSystem::Update(float ts) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        mContext->Update();
    }

    auto RenderSystem::PrepareFrame() const -> void {
        //MKT_BEGIN_PROFILER_NAMED();
        //MKT_PROFILE_SCOPE_MARKED( "RenderSystem::PrepareFrame" );

        mContext->PrepareFrame();

        // Using the GUI library is optional, we
        // need to check for availability
        if (mEnableImGui && mImguiService) {
            mImguiService->PrepareFrame();
        }
    }

    auto RenderSystem::SubmitFrame() -> void {
        //MKT_BEGIN_PROFILER_NAMED();

        //MKT_PROFILE_SCOPE_MARKED( "RenderSystem::SubmitFrame" );
        if (mEnableImGui && mImguiService) {
            mImguiService->EndFrame();
        }

        mContext->SubmitFrame();
    }

    auto RenderSystem::PresentFrame() -> void {
        //MKT_PROFILE_SCOPE_MARKED( "RenderSystem::PresentFrame" );
        mContext->Present();
    }

    auto RenderSystem::SetPresentTarget( TextureHandle texture ) -> void {
        return mContext->SetPresentTarget( texture );
    }

    auto RenderSystem::GetSlangCurrentSession() const -> Slang::ComPtr<slang::ISession> {
        return mSlangCurrentSession;
    }

    auto RenderSystem::GetContext() -> RenderContext * {
        return mContext.get();
    }

    auto RenderSystem::GetContext() const -> const RenderContext * {
        return mContext.get();
    }

    auto RenderSystem::GetGpuDevice() -> GpuDevice * {
        return mContext->GetGpuDevice();
    }

    auto RenderSystem::GetGpuDevice() const -> const GpuDevice * {
        return mContext->GetGpuDevice();
    }

    auto RenderSystem::GetActiveGraphicsApi() const -> GraphicsAPI {
        return mApi;
    }

    auto RenderSystem::InitializeSlang() -> void {
        switch (mApi) {
            case GraphicsAPI::eVulkan:
                PrepareSlangForVulkan();
                break;
            case GraphicsAPI::eD3D12:
                PrepareSlangForD3D12();
                break;
            case GraphicsAPI::eD3D11:
                PrepareSlangForD3D11();
                break;
            default:;
        }
    }

    auto RenderSystem::IsApiActive( const GraphicsAPI api ) const -> bool {
        return mApi == api;
    }

    auto RenderSystem::InitContext() -> void {
        const RenderContextCreateInfo createInfo{
            .mWindow = mWindow,
            .mRefreshRate = mRefreshRate,
            .mApi = mApi,
        };

        mContext = RenderContext::Create(createInfo);
        if (!mContext->Init()) {
            MKT_CORE_LOGGER_CRITICAL( "RenderSystem::Init - Could not initialize Render context." );
        } else {
            // If we managed to create a valid API
            // context we can initialize the GUI library
            if (mEnableImGui && mWindow) {
                InitGuiService();
            }
        }
    }

    auto RenderSystem::InitGuiService() -> void {
        // Imgui service
        ImGuiServiceDescription imguiServiceCreateInfo{
            .mWindow = mWindow,
            .mDevice = GetGpuDevice(),
            .mApi = mApi,
        };

        mImguiService = eastl::make_unique<ImGuiService>( imguiServiceCreateInfo );
        mImguiService->Initialize();
    }

    auto RenderSystem::PrepareSlangForD3D11() -> void {
        slang::createGlobalSession(mSlangGlobalSession.writeRef());

        static auto slangTargets{ eastl::to_array<slang::TargetDesc>( { {
            .format = SLANG_HLSL,
            .profile = mSlangGlobalSession->findProfile("sm_5_0")
        } } ) };

        static auto slangOptions{ eastl::to_array<slang::CompilerOptionEntry>( {
            { slang::CompilerOptionName::ForceCLayout, { slang::CompilerOptionValueKind::Int, 1 } }
        } ) };

        static slang::SessionDesc sessionDesc{
            .targets = slangTargets.data(),
            .targetCount = as<SlangInt>( slangTargets.size() ),
            .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
            .compilerOptionEntries = slangOptions.data(),
            .compilerOptionEntryCount = as<u32>( slangOptions.size() ),
        };

        // Macros
        slang::PreprocessorMacroDesc macros[] = {
            { "MKT_SUPPORT_D3D11", "1" },
#if !defined(NDEBUG)
            { "MKT_DEBUG", "1" }
#endif
        };

        sessionDesc.preprocessorMacros = macros;
        sessionDesc.preprocessorMacroCount = std::size(macros);

        SlangResult result{ mSlangGlobalSession->createSession( sessionDesc, mSlangCurrentSession.writeRef()) };

        if (SLANG_FAILED(result)) {
            MKT_CORE_LOGGER_ERROR("Failed to initialize Slang session for D3D11.");
        }
    }

    auto RenderSystem::PrepareSlangForD3D12() -> void {
        slang::createGlobalSession(mSlangGlobalSession.writeRef());

        static auto slangTargets{ eastl::to_array<slang::TargetDesc>( { {
            .format = SLANG_HLSL,
            .profile = mSlangGlobalSession->findProfile("sm_5_1")
        } } ) };

        static auto slangOptions{ eastl::to_array<slang::CompilerOptionEntry>( {
            { slang::CompilerOptionName::ForceCLayout, { slang::CompilerOptionValueKind::Int, 1 } }
        } ) };

        static slang::SessionDesc sessionDesc{
            .targets = slangTargets.data(),
            .targetCount = as<SlangInt>( slangTargets.size() ),
            .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
            .compilerOptionEntries = slangOptions.data(),
            .compilerOptionEntryCount = as<u32>( slangOptions.size() ),
        };

        // Macros
        slang::PreprocessorMacroDesc macros[] = {
            { "MKT_SUPPORT_D3D12", "1" },
#if !defined(NDEBUG)
            { "MKT_DEBUG", "1" }
#endif
        };

        sessionDesc.preprocessorMacros = macros;
        sessionDesc.preprocessorMacroCount = std::size(macros);

        SlangResult result{ mSlangGlobalSession->createSession( sessionDesc, mSlangCurrentSession.writeRef()) };

        if (SLANG_FAILED(result)) {
            MKT_CORE_LOGGER_ERROR("Failed to initialize Slang session for D3D11.");
        }
    }

    auto RenderSystem::PrepareSlangForVulkan() -> void {
        // Need to be static I think session still references these when you shut it down
        slang::createGlobalSession( mSlangGlobalSession.writeRef() );
        static auto slangTargets{ eastl::to_array<slang::TargetDesc>( { {
            .format = SLANG_SPIRV,
            .profile = mSlangGlobalSession->findProfile( "spirv_1_4" )
        } } ) };

        static auto slangOptions{ eastl::to_array<slang::CompilerOptionEntry>( {
            { slang::CompilerOptionName::EmitSpirvDirectly, { slang::CompilerOptionValueKind::Int, 1 } },
            { slang::CompilerOptionName::ForceCLayout, { slang::CompilerOptionValueKind::Int, 1 } },
        } ) };

        static slang::SessionDesc sessionDesc{
            .targets = slangTargets.data(),
            .targetCount = as<SlangInt>( slangTargets.size() ),
            .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
            .compilerOptionEntries = slangOptions.data(),
            .compilerOptionEntryCount = as<u32>( slangOptions.size() ),
        };

        // Macros
        slang::PreprocessorMacroDesc macros[] = {
            { "MKT_SUPPORT_VULKAN", "1" },
#if !defined(NDEBUG)
            { "MKT_DEBUG", "1" }
#endif
        };

        sessionDesc.preprocessorMacros = macros;
        sessionDesc.preprocessorMacroCount = std::size(macros);

        SlangResult result{ mSlangGlobalSession->createSession( sessionDesc, mSlangCurrentSession.writeRef() ) };
        if (SLANG_FAILED(result)) {
            MKT_CORE_LOGGER_ERROR( "Failed to initialize slang global session for Vulkan." );
        }
    }
}// namespace Mikoto
