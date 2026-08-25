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

#ifndef MIKOTO_D3D12CONTEXT_HH
#define MIKOTO_D3D12CONTEXT_HH

#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/unique_ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>
#include <Core/Service.hh>
#include <Core/Singleton.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Rhi/CommandQueue.hh>

#include <Renderer/Core/RenderContext.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

// D3D12 extension library.
#include <directx/d3d12.h>
#include <directx/d3dx12.h>

#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <wrl.h>
#include <dxcapi.h>

#include <Renderer/Rhi/D3D12/D3D12SwapChain.hh>
#include <Renderer/Rhi/D3D12/Direct3D12Libraries.hh>

namespace mikoto::renderer::d3d12 {

    struct ShaderCompileDescription {
        eastl::string_view mSource{};
        eastl::string_view mSourceName{};

        eastl::string_view mEntryPoint{ "main" };
        rhi::ShaderType mShaderType{};

        ankerl::unordered_dense::map<eastl::string, eastl::string> mDefinitions;

        bool mEnableDebug{ false };
        bool mSkipOptimizations{ false };
        bool mOptimizationLevel3{ true };

        bool mStripReflection{ false };
        bool mStripDebug{ false };
        bool mWarningsAsErrors{ false };

        auto SetSource( eastl::string_view value ) -> ShaderCompileDescription&;
        auto SetSourceName( eastl::string_view value ) -> ShaderCompileDescription&;

        auto SetEntryPoint( eastl::string_view value ) -> ShaderCompileDescription&;
        auto SetProfile( rhi::ShaderType value ) -> ShaderCompileDescription&;

        auto AddDefinition( eastl::string_view key, eastl::string_view value )
                -> ShaderCompileDescription&;

        auto EnableDebug( bool value = true ) -> ShaderCompileDescription&;
        auto SkipOptimizations( bool value = true ) -> ShaderCompileDescription&;
        auto OptimizationLevel3( bool value = true ) -> ShaderCompileDescription&;

        auto StripReflection( bool value = true ) -> ShaderCompileDescription&;
        auto StripDebug( bool value = true ) -> ShaderCompileDescription&;
        auto WarningsAsErrors( bool value = true ) -> ShaderCompileDescription&;
    };

    // https://github.com/microsoft/DirectXShaderCompiler/wiki/Using-dxc.exe-and-dxcompiler.dll
    class ShaderCompiler final : public core::Singleton<ShaderCompiler>, public core::IService {
    public:
        explicit ShaderCompiler() = default;

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        auto Compile(const ShaderCompileDescription& desc) -> Microsoft::WRL::ComPtr<IDxcBlob>;

        operator IDxcUtils*() const;
        operator IDxcCompiler3*() const;
        operator IDxcIncludeHandler*() const;

        DISABLE_COPY_AND_MOVE_FOR( ShaderCompiler );

    private:
        Microsoft::WRL::ComPtr<IDxcCompiler3> mCompiler{};
        Microsoft::WRL::ComPtr<IDxcUtils> mCompilerUtils{};
        Microsoft::WRL::ComPtr<IDxcIncludeHandler> mCompilerIncHeader{};

        // Shader profiles
        ankerl::unordered_dense::map<rhi::ShaderType, eastl::wstring> mProfiles{};
    };

    // Refs: https://alain.xyz/blog/raw-directx12
    class Context final : public RenderContext {
    public:
        explicit Context(const RenderContextCreateInfo& createInfo);

        auto Init() -> bool override;
        auto Shutdown() -> void override;

        auto SubmitFrame() -> void override;
        auto PrepareFrame() -> void override;

        auto Update() -> void override;

        auto Present() -> void override;
        auto SetPresentTarget( rhi::TextureHandle texture ) -> void override;
        auto SetRefreshRate( rhi::RefreshRate rate ) -> void override;

        auto BatchSubmission( rhi::SubmitInfo&& submitInfo, rhi::QueueType queue ) -> void override;

        // D3D12 Specifics
        MKT_NODISCARD auto GetSwapChain() const -> SwapChainHandle;
        MKT_NODISCARD auto GetDxGIFactory() const -> IDXGIFactory4*;
        MKT_NODISCARD auto GetShaderCompiler() const -> ShaderCompiler*;

        MKT_NODISCARD auto GetBackBufferCount() const -> UINT;

        MKT_NODISCARD auto GetDxGIDebug() const -> IDXGIDebug1*;
        MKT_NODISCARD auto GetDebugController() const -> ID3D12Debug6*;

        ~Context() override = default;

    private:
        // [Internal usage]
        auto InitializeSwapchain() -> void;
        auto InitSwapchainRender() -> void;
        auto InitializeShaderCompiler() -> void;

    private:
        core::u64 mFenceValue{};
        rhi::FenceHandle mFence{};

        rhi::CommandListHandle mCommandList{};
        rhi::TextureHandle mPresentTarget{};

        SwapChainHandle mSwapChain{};
        Microsoft::WRL::ComPtr<IDXGIFactory4> mDxgiFactory{};

        static constexpr UINT kBackBufferCount{ 2 };

        eastl::unique_ptr<ShaderCompiler> mShaderCompiler{};

        IQueue* mGraphicsQueue{};

        using SubmitInfoMap = ankerl::unordered_dense::map<IQueue*, SubmitInfo>;

        std::mutex mBatchedSubmissionProcessMutex{};
        std::mutex mBatchedSubmissionEmplaceMutex{};
        SubmitInfoMap mBatchedSubmissions{};

        // Swapchain blit objects

        // A descriptor table is used instead of a BindingSet because descriptor tables can be updated
        // a BindingSet once created it cannot be updated, so whatever resources specified during creation
        // stay for how long the BindingSet stays alive
        bool mTableUpdateRequired{};

        rhi::SamplerHandle mSamplerState{};
        rhi::ShaderModuleHandle mVertexShader{};
        rhi::ShaderModuleHandle mPixelShader{};
        rhi::PipelineHandle mPipeline{};
        rhi::BindingSetHandle mBindingSetHandle{};
        rhi::BindingLayoutHandle mBindlessLayout{};
        rhi::DescriptorTableHandle mDescriptorTable{};
        rhi::BindingLayoutHandle mBindingLayoutHandle{};
        rhi::PipelineLayoutHandle mPipelineLayoutHandle{};

#if !defined(NDEBUG)
        Microsoft::WRL::ComPtr<IDXGIDebug1> mDxGIDebug{};
        Microsoft::WRL::ComPtr<ID3D12Debug6> mDebugController{};
#endif
    };
}

#endif


#endif//MIKOTO_D3D12CONTEXT_HH
