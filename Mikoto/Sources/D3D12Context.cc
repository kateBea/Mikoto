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

#include <ranges>

#include <Core/Exception.hh>
#include <Core/Platform.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

#include <Renderer/D3D12/D3D12Device.hh>
#include <Renderer/D3D12/D3D12Context.hh>
#include <Renderer/D3D12/Direct3D12Helpers.hh>

namespace mikoto::renderer::d3d12 {

    auto ShaderCompileDescription::SetSource( eastl::string_view value ) -> ShaderCompileDescription& {
        mSource = value;
        return *this;
    }

    auto ShaderCompileDescription::SetSourceName( eastl::string_view value ) -> ShaderCompileDescription& {
        mSourceName = value;
        return *this;
    }

    auto ShaderCompileDescription::SetEntryPoint( eastl::string_view value ) -> ShaderCompileDescription& {
        mEntryPoint = value;
        return *this;
    }

    auto ShaderCompileDescription::SetProfile( ShaderType value ) -> ShaderCompileDescription& {
        mShaderType = value;
        return *this;
    }

    auto ShaderCompileDescription::AddDefinition(
            eastl::string_view key,
            eastl::string_view value )
            -> ShaderCompileDescription& {
        mDefinitions[eastl::string{ key }] = eastl::string{ value };
        return *this;
    }

    auto ShaderCompileDescription::EnableDebug( bool value ) -> ShaderCompileDescription& {
        mEnableDebug = value;
        return *this;
    }

    auto ShaderCompileDescription::SkipOptimizations( bool value ) -> ShaderCompileDescription& {
        mSkipOptimizations = value;
        return *this;
    }

    auto ShaderCompileDescription::OptimizationLevel3( bool value ) -> ShaderCompileDescription& {
        mOptimizationLevel3 = value;
        return *this;
    }

    auto ShaderCompileDescription::StripReflection( bool value ) -> ShaderCompileDescription& {
        mStripReflection = value;
        return *this;
    }

    auto ShaderCompileDescription::StripDebug( bool value ) -> ShaderCompileDescription& {
        mStripDebug = value;
        return *this;
    }

    auto ShaderCompileDescription::WarningsAsErrors( bool value ) -> ShaderCompileDescription& {
        mWarningsAsErrors = value;
        return *this;
    }

    auto ShaderCompiler::Initialize() -> void {
        ThrowIfFailed( DxcCreateInstance( CLSID_DxcCompiler, IID_PPV_ARGS( &mCompiler ) ) );
        ThrowIfFailed( DxcCreateInstance( CLSID_DxcUtils, IID_PPV_ARGS( &mCompilerUtils ) ) );

        ThrowIfFailed( mCompilerUtils->CreateDefaultIncludeHandler( &mCompilerIncHeader ) );

        mProfiles = {
            { ShaderType::eVertex,   L"vs_6_6" },
            { ShaderType::ePixel,    L"ps_6_6" },
            { ShaderType::eGeometry, L"gs_6_6" },
            { ShaderType::eHull,     L"hs_6_6" },
            { ShaderType::eDomain,   L"ds_6_6" },
            { ShaderType::eCompute,  L"cs_6_6" }
        };
    }

    auto ShaderCompiler::Shutdown() -> void {

    }

    auto ShaderCompiler::Compile( const ShaderCompileDescription& desc ) -> Microsoft::WRL::ComPtr<IDxcBlob> {
        DxcBuffer sourceBuffer{};
        sourceBuffer.Ptr = desc.mSource.data();
        sourceBuffer.Size = desc.mSource.size();
        sourceBuffer.Encoding = DXC_CP_UTF8;

        // Owns the UTF-16 strings so the LPCWSTR pointers stored in
        // 'arguments' remain valid for the duration of the Compile() call.
        eastl::vector<eastl::wstring> argsStorage{};

        // Array of pointers passed to IDxcCompiler3::Compile().
        eastl::vector<LPCWSTR> arguments{};

        auto AddArgument = [&]( eastl::wstring arg ) {
            argsStorage.emplace_back( eastl::move( arg ) );
        };

        // Source name (optional) for debug output
        if ( !desc.mSourceName.empty() ) {
            AddArgument( string::ToWide( desc.mSourceName ) );
        }

        // Entry point name
        AddArgument( L"-E" );
        AddArgument( string::ToWide( desc.mEntryPoint ) );

        // Shader profile
        AddArgument( L"-T" );
        AddArgument( mProfiles[desc.mShaderType].c_str() );

        // Macro definitions
        for ( const auto& [key, value]: desc.mDefinitions ) {
            AddArgument( L"-D" );
            AddArgument( string::ToWide( key + "=" + value ) );
        }

        // Debug stuff
        if ( desc.mEnableDebug )
            AddArgument( DXC_ARG_DEBUG );

        if ( desc.mSkipOptimizations )
            AddArgument( DXC_ARG_SKIP_OPTIMIZATIONS );

        if ( desc.mOptimizationLevel3 )
            AddArgument( DXC_ARG_OPTIMIZATION_LEVEL3 );

        if ( desc.mStripReflection )
            AddArgument( L"-Qstrip_reflect" );

        if ( desc.mStripDebug )
            AddArgument( L"-Qstrip_debug" );

        if ( desc.mWarningsAsErrors )
            AddArgument( L"-WX" );

        for (const auto& item : argsStorage ) {
            arguments.emplace_back( item.data() );
        }

        Microsoft::WRL::ComPtr<IDxcResult> result{};
        HRESULT hr{ mCompiler->Compile(
                &sourceBuffer,
                arguments.data(),
                as<UINT>( arguments.size() ),
                mCompilerIncHeader.Get(),
                IID_PPV_ARGS( &result ) ) };

        Microsoft::WRL::ComPtr<IDxcBlobUtf8> errors{};
        result->GetOutput(
            DXC_OUT_ERRORS,
            IID_PPV_ARGS(&errors),
            nullptr);

        if (errors && errors->GetStringLength() != 0) {
            eastl::string msg{ errors->GetStringPointer() };
            MKT_CORE_LOGGER_ERROR( "DxC compile errors: {}", msg.c_str() );
        }

        MKT_ASSERT( SUCCEEDED( hr ), "Failed to invoke DXC compiler." );

        Microsoft::WRL::ComPtr<IDxcBlob> shader{};
        result->GetOutput(
            DXC_OUT_OBJECT,
            IID_PPV_ARGS(&shader),
            nullptr);

        return shader;
    }

    ShaderCompiler::operator IDxcUtils*() const {
        return mCompilerUtils.Get();
    }

    ShaderCompiler::operator IDxcCompiler3*() const {
        return mCompiler.Get();
    }

    ShaderCompiler::operator IDxcIncludeHandler*() const {
        return mCompilerIncHeader.Get();
    }

    Context::Context( const RenderContextCreateInfo& createInfo )
        :  RenderContext{ createInfo }
    { }

    auto Context::Init() -> bool {
#if !defined(NDEBUG)
        ThrowIfFailed( D3D12GetDebugInterface( IID_PPV_ARGS( &mDebugController ) ) );

        mDebugController->EnableDebugLayer();
        mDebugController->SetEnableGPUBasedValidation( true );
        mDebugController->SetEnableSynchronizedCommandQueueValidation( true );

        ThrowIfFailed( DXGIGetDebugInterface1( 0, IID_PPV_ARGS( &mDxGIDebug ) ) );
#endif

        const UINT dxgiFactoryFlags{ DXGI_CREATE_FACTORY_DEBUG };
        ThrowIfFailed( CreateDXGIFactory2( dxgiFactoryFlags, IID_PPV_ARGS( &mDxgiFactory ) ) );

        // Init the device when the context is ready
        mDevice = IGpuDevice::Create( { .mApi = GraphicsAPI::eD3D12 } );
        if ( !mDevice ) {
            MKT_THROW_RUNTIME_ERROR( "Could not initialize D3D12 GPU Device." );
        }
        mDevice->Init();

        // If no window is provided we use D3D12 headless
        if (mWindow) {
            InitializeSwapchain();
        }

        InitializeShaderCompiler();

        return true;
    }

    auto Context::Shutdown() -> void {
        mShaderCompiler->Shutdown();
        mShaderCompiler.reset();
    }

    auto Context::SubmitFrame() -> void {

    }

    auto Context::PrepareFrame() -> void {

    }

    auto Context::Update() -> void {

    }

    auto Context::Present() -> void {

    }

    auto Context::SetPresentTarget( TextureHandle texture ) -> void {
        mPresentTarget = texture;
    }

    auto Context::SetRefreshRate( RefreshRate rate ) -> void {
        mRefreshRate = rate;
    }

    auto Context::GetSwapChain() const -> SwapChainHandle {
        return mSwapChain;
    }

    auto Context::GetDxGIFactory() const -> IDXGIFactory4* {
        return mDxgiFactory.Get();
    }

    auto Context::GetShaderCompiler() const -> ShaderCompiler* {
        return mShaderCompiler.get();
    }

    auto Context::GetBackBufferCount() const -> UINT {
        return kBackBufferCount;
    }

    auto Context::GetDxGIDebug() const -> IDXGIDebug1* {
#if !defined(NDEBUG)
        return mDxGIDebug.Get();
#endif
    }

    auto Context::GetDebugController() const -> ID3D12Debug6* {
#if !defined(NDEBUG)
        return mDebugController.Get();
#endif
    }

    auto Context::InitializeSwapchain() -> void {
        mSwapChain = checked_cast<Device*>( GetGpuDevice() )->CreateSwapChain( mWindow, mDxgiFactory );
        if (!mSwapChain.IsEmpty()) {
            mSwapChain->SetRefreshRate( mRefreshRate );
        }
    }

    auto Context::InitializeShaderCompiler() -> void {
        mShaderCompiler = eastl::make_unique<ShaderCompiler>();
        mShaderCompiler->Initialize();
    }
}// namespace Mikoto

#endif
