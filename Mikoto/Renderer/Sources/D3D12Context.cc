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
#include <ranges>
#include <fstream>
#include <filesystem>

#include <Core/CVar.hh>
#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Logging/Logger.hh>

#include <Filesystem/File.hh>
#include <Filesystem/Path.hh>
#include <Filesystem/FileService.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )

#include <Platform/Windows/Windows.hh>

#include <directx/d3d12.h>
#include <directx/d3dx12.h>

#include <Renderer/Rhi/D3D12/D3D12Device.hh>
#include <Renderer/Rhi/D3D12/D3D12Context.hh>
#include <Renderer/Rhi/D3D12/Direct3D12Helpers.hh>

namespace mikoto::renderer::d3d12 {

    using namespace mikoto::core;
    using namespace mikoto::filesystem;
    using namespace mikoto::renderer::rhi;

    auto ShaderCompileDescription::SetSource( eastl::string_view value ) -> ShaderCompileDescription& {
        MKT_BEGIN_PROFILER();
        mSource = value;
        return *this;
    }

    auto ShaderCompileDescription::SetSourceName( eastl::string_view value ) -> ShaderCompileDescription& {
        MKT_BEGIN_PROFILER();
        mSourceName = value;
        return *this;
    }

    auto ShaderCompileDescription::SetDebugSymbolsDirectory( eastl::string_view value ) -> ShaderCompileDescription& {
        mDebugSymbolsDirectory = value;
        return *this;
    }

    auto ShaderCompileDescription::SetEntryPoint( eastl::string_view value ) -> ShaderCompileDescription& {
        MKT_BEGIN_PROFILER();
        mEntryPoint = value;
        return *this;
    }

    auto ShaderCompileDescription::SetProfile( renderer::rhi::ShaderType value ) -> ShaderCompileDescription& {
        MKT_BEGIN_PROFILER();
        mShaderType = value;
        return *this;
    }

    auto ShaderCompileDescription::AddDefinition(
        eastl::string_view key,
        eastl::string_view value )
        -> ShaderCompileDescription& {
        MKT_BEGIN_PROFILER();
        mDefinitions[eastl::string{ key }] = eastl::string{ value };
        return *this;
    }

    auto ShaderCompileDescription::EnableDebug( bool value ) -> ShaderCompileDescription& {
        MKT_BEGIN_PROFILER();
        mEnableDebug = value;
        return *this;
    }

    auto ShaderCompileDescription::SkipOptimizations( bool value ) -> ShaderCompileDescription& {
        MKT_BEGIN_PROFILER();
        mSkipOptimizations = value;
        return *this;
    }

    auto ShaderCompileDescription::OptimizationLevel3( bool value ) -> ShaderCompileDescription& {
        MKT_BEGIN_PROFILER();
        mOptimizationLevel3 = value;
        return *this;
    }

    auto ShaderCompileDescription::StripReflection( bool value ) -> ShaderCompileDescription& {
        MKT_BEGIN_PROFILER();
        mStripReflection = value;
        return *this;
    }

    auto ShaderCompileDescription::StripDebug( bool value ) -> ShaderCompileDescription& {
        MKT_BEGIN_PROFILER();
        mStripDebug = value;
        return *this;
    }

    auto ShaderCompileDescription::WarningsAsErrors( bool value ) -> ShaderCompileDescription& {
        MKT_BEGIN_PROFILER();
        mWarningsAsErrors = value;
        return *this;
    }

    auto ShaderCompiler::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        ThrowIfFailed( DxcCreateInstance( CLSID_DxcCompiler, IID_PPV_ARGS( &mCompiler ) ) );
        ThrowIfFailed( DxcCreateInstance( CLSID_DxcUtils, IID_PPV_ARGS( &mCompilerUtils ) ) );

        ThrowIfFailed( mCompilerUtils->CreateDefaultIncludeHandler( &mCompilerIncHeader ) );

        // Shader model 6.6 used for bindless
        mProfiles = {
            { renderer::rhi::ShaderType::eVertex, L"vs_6_8" },
            { renderer::rhi::ShaderType::ePixel, L"ps_6_8" },
            { renderer::rhi::ShaderType::eGeometry, L"gs_6_8" },
            { renderer::rhi::ShaderType::eHull, L"hs_6_8" },
            { renderer::rhi::ShaderType::eDomain, L"ds_6_8" },
            { renderer::rhi::ShaderType::eCompute, L"cs_6_8" },
            { renderer::rhi::ShaderType::eRayGeneration, L"lib_6_8" },
            { renderer::rhi::ShaderType::eMiss, L"lib_6_8" },
            { renderer::rhi::ShaderType::eClosestHit, L"lib_6_8" }
        };
    }

    auto ShaderCompiler::Shutdown() -> void {
    }

    auto ShaderCompiler::Compile( const ShaderCompileDescription& desc ) -> Microsoft::WRL::ComPtr<IDxcBlob> {
        MKT_BEGIN_PROFILER();
        // DXC objects must be created and used on the compiling thread.
        // The context-owned compiler is reserved for callers on the owner thread.
        // https://github.com/microsoft/DirectXShaderCompiler/wiki/Using-dxc.exe-and-dxcompiler.dll#dxcompiler-dll-interface
        Microsoft::WRL::ComPtr<IDxcCompiler3> compiler{};
        Microsoft::WRL::ComPtr<IDxcUtils> utilities{};
        Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler{};
        ThrowIfFailed( DxcCreateInstance( CLSID_DxcCompiler, IID_PPV_ARGS( &compiler ) ) );
        ThrowIfFailed( DxcCreateInstance( CLSID_DxcUtils, IID_PPV_ARGS( &utilities ) ) );
        ThrowIfFailed( utilities->CreateDefaultIncludeHandler( &includeHandler ) );

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
        const bool library{ desc.mShaderType == renderer::rhi::ShaderType::eRayGeneration ||
                            desc.mShaderType == renderer::rhi::ShaderType::eMiss || desc.mShaderType == renderer::rhi::ShaderType::eClosestHit };

        if ( !library ) {
            AddArgument( L"-E" );
            AddArgument( string::ToWide( desc.mEntryPoint ) );
        }

        // Shader profile
        AddArgument( L"-T" );
        AddArgument( std::as_const( mProfiles ).at( desc.mShaderType ).c_str() );

        // Macro definitions
        for ( const auto& [key, value]: desc.mDefinitions ) {
            AddArgument( L"-D" );
            AddArgument( string::ToWide( key + "=" + value ) );
        }

        // Save symbols separately: embedded ILDB triggers invalid debug-scope
        // diagnostics during PSO creation on the current D3D12 runtime.
        // https://github.com/microsoft/DirectXShaderCompiler/wiki/Using-dxc.exe-and-dxcompiler.dll#pdbs
        if ( desc.mEnableDebug ) {
            AddArgument( DXC_ARG_DEBUG );
            AddArgument( L"-Fd" );
            AddArgument( string::ToWide( desc.mDebugSymbolsDirectory ) + L"\\" );
        }

        if ( desc.mSkipOptimizations ) {
            AddArgument( DXC_ARG_SKIP_OPTIMIZATIONS );
        }

        else if ( desc.mOptimizationLevel3 ) {
            AddArgument( DXC_ARG_OPTIMIZATION_LEVEL3 );
        }

        if ( desc.mStripReflection ) {
            AddArgument( L"-Qstrip_reflect" );
        }

        if ( desc.mStripDebug ) {
            AddArgument( L"-Qstrip_debug" );
        }

        if ( desc.mWarningsAsErrors ) {
            AddArgument( L"-WX" );
        }

        for ( const auto& item: argsStorage ) {
            arguments.emplace_back( item.data() );
        }

        Microsoft::WRL::ComPtr<IDxcResult> result{};
        HRESULT hr{ compiler->Compile(
            &sourceBuffer,
            arguments.data(),
            core::as<UINT>( arguments.size() ),
            includeHandler.Get(),
            IID_PPV_ARGS( &result ) ) };

        ThrowIfFailed( hr );
        ThrowIfFailed( result->GetStatus( &hr ) );

        Microsoft::WRL::ComPtr<IDxcBlobUtf8> errors{};
        result->GetOutput(
            DXC_OUT_ERRORS,
            IID_PPV_ARGS( &errors ),
            nullptr );

        if ( errors && errors->GetStringLength() != 0 ) {
            eastl::string msg{ errors->GetStringPointer() };
            MKT_CORE_LOGGER_ERROR( "DxC compile errors: {}", msg.c_str() );
        }

        Microsoft::WRL::ComPtr<IDxcBlob> shader{};

        if ( SUCCEEDED( hr ) ) {
            result->GetOutput( DXC_OUT_OBJECT, IID_PPV_ARGS( &shader ), nullptr );
        }
        else {
            MKT_CORE_LOGGER_ERROR( "Failed to invoke DXC compiler." );
        }

        ThrowIfFailed( hr );

        if ( !shader ) {
            MKT_THROW_RUNTIME_ERROR( "DXC did not produce shader bytecode." );
        }

        // Use DXC's generated name so PIX can locate the matching PDB from ILDN.
        // Symbol I/O must not discard a successfully compiled shader.
        if ( desc.mEnableDebug ) {
            Microsoft::WRL::ComPtr<IDxcBlob> symbols{};
            Microsoft::WRL::ComPtr<IDxcBlobUtf16> symbolsName{};
            ThrowIfFailed( result->GetOutput( DXC_OUT_PDB, IID_PPV_ARGS( &symbols ), &symbolsName ) );

            if ( symbols && symbolsName ) {
                const auto symbolsPath{ Path{ desc.mDebugSymbolsDirectory }.GetPathTyped<std::filesystem::path>() /
                    std::filesystem::path{ symbolsName->GetStringPointer() }.filename() };
                std::error_code error{};
                std::filesystem::create_directories( symbolsPath.parent_path(), error );
                std::ofstream file{ symbolsPath, std::ios::binary | std::ios::trunc };
                file.write( static_cast<const char*>( symbols->GetBufferPointer() ), static_cast<std::streamsize>( symbols->GetBufferSize() ) );
                file.close();

                if ( error || !file ) {
                    MKT_CORE_LOGGER_WARN( "Cannot store shader symbols '{}'.", symbolsPath.string() );
                }
            }
        }

        return shader;
    }

    ShaderCompiler::operator IDxcUtils*() const {
        MKT_BEGIN_PROFILER();
        return mCompilerUtils.Get();
    }

    ShaderCompiler::operator IDxcCompiler3*() const {
        MKT_BEGIN_PROFILER();
        return mCompiler.Get();
    }

    ShaderCompiler::operator IDxcIncludeHandler*() const {
        MKT_BEGIN_PROFILER();
        return mCompilerIncHeader.Get();
    }

    Context::Context( const RenderContextCreateInfo& createInfo )
        : RenderContext{ createInfo } {
    }

    auto Context::Init() -> bool {
        MKT_BEGIN_PROFILER();
#if !defined( NDEBUG )
        ThrowIfFailed( D3D12GetDebugInterface( IID_PPV_ARGS( &mDebugController ) ) );

        mDebugController->EnableDebugLayer();
        mDebugController->SetEnableGPUBasedValidation( core::CVarRegistry::Get().GetValue<bool>( "renderer.gpu_validation", false ) );
        mDebugController->SetEnableSynchronizedCommandQueueValidation( true );
        mDebugController->SetForceLegacyBarrierValidation( true );

        ThrowIfFailed( DXGIGetDebugInterface1( 0, IID_PPV_ARGS( &mDxGIDebug ) ) );
        constexpr UINT dxgiFactoryFlags{ DXGI_CREATE_FACTORY_DEBUG };
#else
        constexpr UINT dxgiFactoryFlags{};
#endif
        ThrowIfFailed( CreateDXGIFactory2( dxgiFactoryFlags, IID_PPV_ARGS( &mDxgiFactory4 ) ) );

        if ( FAILED( mDxgiFactory4.As( &mDxgiFactory6 ) ) ) {
            MKT_CORE_LOGGER_WARN( "Could not acquire IDXGIFactory6 interface." );
        }

        const eastl::string deviceTypeString{ core::CVarRegistry::Get()->GetValue<eastl::string>( "device_type" ) };
        const renderer::rhi::GpuDeviceType deviceType{ rhi::InferGpuDeviceType( deviceTypeString ) };

        MKT_ASSERT( deviceType != renderer::rhi::GpuDeviceType::eInvalid, "Invalid device type." );

        // Init the device when the context is ready
        mDevice = renderer::rhi::IGpuDevice::Create( { .mApi = renderer::rhi::GraphicsAPI::eD3D12,
            .mDeviceType = deviceType,
            .mFeatureSupportFlags = GetRequestedDeviceFeatures() } );

        if ( !mDevice ) {
            MKT_THROW_RUNTIME_ERROR( "Could not initialize D3D12 GPU Device." );
        }

        mDevice->Initialize();
        mGraphicsQueue = mDevice->GetQueue( renderer::rhi::QueueType::eGraphics );

        if ( mWindow ) {
            // If no window is provided we use D3D12 headless
            InitializeSwapchain();
        }

        InitializeShaderCompiler();
        InitSwapchainRender();
        InitSynchronization();

        return true;
    }

    auto Context::Shutdown() -> void {
        MKT_BEGIN_PROFILER();
        // Do a wait idle
        mDevice->WaitIdle();

        mPipeline.Reset();
        mPipelineLayoutHandle.Reset();
        mBindingLayoutHandle.Reset();

        mBindlessLayout.Reset();
        mDescriptorTable.Reset();

        mVertexShader.Reset();
        mPixelShader.Reset();

        mSamplerState.Reset();

        mBindingSetHandle.Reset();

        mCommandList.Reset();

        mPresentTarget.Reset();
        mSwapChain.Reset();

        mFrameContexts.clear();

        mShaderCompiler->Shutdown();
        mShaderCompiler.reset();
    }

    auto Context::SubmitFrame() -> void {
        MKT_BEGIN_PROFILER();
        auto& frame{ mFrameContexts[mCurrentFrameIndex] };

        // Submit batched commands
        decltype( mBatchedSubmissions ) swapMap{};
        {
            std::lock_guard lock{ mBatchedSubmissionProcessMutex };
            swapMap = eastl::move( mBatchedSubmissions );
        }

        for ( const auto& [queue, submitInfo]: swapMap ) {
            queue->ExecuteCommandLists( submitInfo );
        }

        const bool recordPresent{ !mPresentTarget.IsEmpty() && !mSwapChain.IsEmpty() &&
                                  mWindow && mWindow->GetWidth() > 0 && mWindow->GetHeight() > 0 };

        if ( recordPresent ) {
#if false
            // Blit via full quad render
            renderer::rhi::TextureHandle colorImage{ mSwapChain->GetCurrentBackBufferImage() };
            mCommandList->Open( { .mScopeName = "Blit Swapchain" } );
            mCommandList->SetTransition( renderer::rhi::TransitionDescription{}.AddTexture( mPresentTarget.GetPtr(), renderer::rhi::ResourceStates::eShaderResource ) );

            if ( frame.mBlitTarget != mPresentTarget ) {
                // PrepareFrame waited for this frame's fence, so its old descriptor is no longer in use.
                if ( frame.mTextureDescriptorIndex != rhi::kInvalidBindingItemIndex ) {
                    mDevice->ReleaseDescriptorTableIndex( mDescriptorTable, 0, frame.mTextureDescriptorIndex );
                    frame.mTextureDescriptorIndex = rhi::kInvalidBindingItemIndex;
                    frame.mBlitTarget.Reset();
                }

                frame.mTextureDescriptorIndex = mDevice->WriteDescriptorTable( mDescriptorTable,
                    renderer::rhi::BindingTableItem::TextureSRV( 0, mPresentTarget.GetPtr() ) );
                frame.mBlitTarget = mPresentTarget;
            }

            auto graphicsState{ renderer::rhi::RenderPassDescription{}
                .SetRenderArea( renderer::rhi::Rect{ core::as<core::i32>(mSwapChain->GetWidth()), core::as<core::i32>(mSwapChain->GetHeight()) } )
                .AddRenderTarget( colorImage, renderer::rhi::Color{ .0f } ) };
            mCommandList->BeginRenderPass( graphicsState );

            struct DrawParams {
                core::u32 mSamplerIndex{};
                core::u32 mTextureIndex{};
            } params{
                .mSamplerIndex = core::checked_cast<BindingTable*>( mBindingSetHandle.GetPtr() )->GetSamplerRange().mBaseIndex,
                .mTextureIndex = core::as<core::u32>( frame.mTextureDescriptorIndex ) };
            mCommandList->SetPushConstants( mPipelineLayoutHandle.GetPtr(), &params, MKT_SIZEOF( params ), renderer::rhi::ShaderFlagsBits::All );

            mCommandList->BindPipeline( mPipeline.GetPtr() );

            auto bindingDescription{ renderer::rhi::BindResourcesDescription{}
                .SetBindPoint( renderer::rhi::PipelineType::eGraphics )
                .SetPipelineLayout( mPipelineLayoutHandle.GetPtr() )
                .AddResourceSet( 0, mBindingSetHandle.GetPtr() )
                .AddResourceSet( 1, mDescriptorTable.GetPtr() ) };
            mCommandList->BindPipelineResources( bindingDescription );

            mCommandList->SetViewportState( renderer::rhi::ViewportState{}
                .AddViewportAndScissorRect( Viewport( core::as<core::f32>( mSwapChain->GetWidth() ), core::as<core::f32>( mSwapChain->GetHeight() ) ) ) );

            constexpr auto drawArguments{ renderer::rhi::DrawArguments{}
                .SetVertexCount( 3 ) };
            mCommandList->Draw( drawArguments );

            mCommandList->EndRenderPass();

            mCommandList->SetTransition( renderer::rhi::TransitionDescription{}.AddTexture( colorImage.GetPtr(), renderer::rhi::ResourceStates::ePresent ) );

            mCommandList->Close();

#else
            // Blit via copy command
            mCommandList->Open( { .mScopeName = "Blit Swapchain" } );

            renderer::rhi::TextureHandle colorImage{ mSwapChain->GetCurrentBackBufferImage() };

            // The UI target can still have last frame's extent during a resize.
            // CopyTextureRegion never scales and must stay inside both images.
            const core::u32 copyWidth{ glm::min( mPresentTarget->GetWidth(), colorImage->GetWidth() ) };
            const core::u32 copyHeight{ glm::min( mPresentTarget->GetHeight(), colorImage->GetHeight() ) };

            if ( copyWidth != colorImage->GetWidth() || copyHeight != colorImage->GetHeight() ) {
                mCommandList->SetClearColor( colorImage.GetPtr(), renderer::rhi::kColorBlack );
            }

            const renderer::rhi::TextureSlice srcSlice{
                .mWidth = copyWidth,
                .mHeight = copyHeight
            };

            const renderer::rhi::TextureSlice dstSlice{
                .mWidth = copyWidth,
                .mHeight = copyHeight
            };

            mCommandList->Copy(
                mPresentTarget.GetPtr(), srcSlice,
                colorImage.GetPtr(), dstSlice );

            mCommandList->SetTransition( TransitionDescription{}
                .AddTexture( colorImage.GetPtr(), renderer::rhi::ResourceStates::ePresent ) );

            mCommandList->Close();
#endif
        }

        ++frame.mFenceValue;

        auto submitInfo{ renderer::rhi::SubmitInfo{}.AddSignal( frame.mFence, frame.mFenceValue ) };

        if ( recordPresent ) {
            submitInfo.AddCommandList( mCommandList );
        }

        mDevice->GetQueue( renderer::rhi::QueueType::eGraphics )->ExecuteCommandLists( submitInfo );
    }

    auto Context::PrepareFrame() -> void {
        MKT_BEGIN_PROFILER();
        // If it is the first frame we don't need to wait for anything
        // Otherwise mCurrentFrameIndex has advanced and we just wait for this frame to be done
        auto& frame{ mFrameContexts[mCurrentFrameIndex] };

        Fence* pFence{ core::checked_cast<Fence*>( frame.mFence.GetPtr() ) };

        if ( !pFence->Wait( frame.mFenceValue, eastl::numeric_limits<core::u64>::max() ) ) {
            MKT_THROW_RUNTIME_ERROR( "D3D12 frame fence wait failed." );
        }

        mDevice->RunGarbageCollection();

        if ( mWindow && mWindow->GetWidth() > 0 && mWindow->GetHeight() > 0 ) {
            if ( mSwapChain->GetWidth() != mWindow->GetWidth() || mSwapChain->GetHeight() != mWindow->GetHeight() ) {
                mSwapChain->OnResize( mWindow->GetWidth(), mWindow->GetHeight() );
            }
        }
    }

    auto Context::Update() -> void {
    }

    auto Context::Present() -> void {
        MKT_BEGIN_PROFILER();
        // Nothing to present if there is no window
        if ( !mWindow ) {
            return;
        }

        if ( mWindow->GetWidth() > 0 && mWindow->GetHeight() > 0 ) {
            mSwapChain->Present();
        }

        // Frame is advanced if we work with a swap chain
        mCurrentFrameIndex = ( mCurrentFrameIndex + 1 ) % GetBackBufferCount();
    }

    auto Context::SetPresentTarget( renderer::rhi::TextureHandle texture ) -> void {
        MKT_BEGIN_PROFILER();
        mPresentTarget = texture;
    }

    auto Context::SetRefreshRate( renderer::rhi::RefreshRate rate ) -> void {
        MKT_BEGIN_PROFILER();
        mRefreshRate = rate;
        mSwapChain->SetRefreshRate( mRefreshRate );
    }

    auto Context::BatchSubmission( rhi::SubmitInfo&& submitInfo, rhi::QueueType queue ) -> void {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mBatchedSubmissionProcessMutex };

        // Preserve waits/signals between submissions. Merging a later wait
        // ahead of an earlier signal on the same queue would deadlock it.
        auto* target{ core::checked_cast<Queue*>( mDevice->GetQueue( queue ) ) };
        mBatchedSubmissions.emplace_back( target, eastl::move( submitInfo ) );
    }

    auto Context::GetSwapChain() const -> D3D12SwapChainHandle {
        MKT_BEGIN_PROFILER();
        return mSwapChain;
    }

    auto Context::GetDxGIFactory4() const -> IDXGIFactory4* {
        MKT_BEGIN_PROFILER();
        return mDxgiFactory4.Get();
    }

    auto Context::GetDxGIFactory6() const -> IDXGIFactory6* {
        MKT_BEGIN_PROFILER();
        return mDxgiFactory6.Get();
    }

    auto Context::GetShaderCompiler() const -> ShaderCompiler* {
        MKT_BEGIN_PROFILER();
        return mShaderCompiler.get();
    }

    auto Context::GetBackBufferCount() const -> UINT {
        MKT_BEGIN_PROFILER();
        return kBackBufferCount;
    }

    auto Context::GetDxGIDebug() const -> IDXGIDebug1* {
        MKT_BEGIN_PROFILER();
#if !defined( NDEBUG )
        return mDxGIDebug.Get();
#else
        return nullptr;
#endif
    }

    auto Context::GetDebugController() const -> ID3D12Debug6* {
        MKT_BEGIN_PROFILER();
#if !defined( NDEBUG )
        return mDebugController.Get();
#else
        return nullptr;
#endif
    }

    auto Context::InitializeSwapchain() -> void {
        MKT_BEGIN_PROFILER();
        mSwapChain = core::checked_cast<Device*>( GetGpuDevice() )->CreateSwapChain( mWindow, mDxgiFactory4 );

        if ( !mSwapChain.IsEmpty() ) {
            mSwapChain->SetRefreshRate( mRefreshRate );
        }
    }

    auto Context::InitSwapchainRender() -> void {
        MKT_BEGIN_PROFILER();
        mCommandList = mDevice->CreateCommandList( renderer::rhi::QueueType::eGraphics );
        mCommandList->SetDebugName( "Context Swapchain CommandBuffer" );

        // Disabled for now, doing Image copy
        return;

        // Create shaders
        filesystem::FileHandle vsShader{ filesystem::FileService::Get()->LoadFile( "Resources/Shaders/slang/SwapChainBlit_Vert.slang" ) };
        auto vertexShaderDescription{ renderer::rhi::ShaderModuleCreateDescription{}
            .SetContents( vsShader )
            .SetModuleName( vsShader->GetName() )
            .SetModulePath( vsShader->GetPath() )
            .SetLanguage( renderer::rhi::ShaderLanguage::eSlang )
            .SetStage( renderer::rhi::ShaderType::eVertex ) };
        mVertexShader = mDevice->CreateShader( vertexShaderDescription );

        filesystem::FileHandle pxShader{ filesystem::FileService::Get()->LoadFile( "Resources/Shaders/slang/SwapChainBlit_Frag.slang" ) };
        auto fragmentShaderDescription{ renderer::rhi::ShaderModuleCreateDescription{}
            .SetContents( pxShader )
            .SetModuleName( pxShader->GetName() )
            .SetModulePath( pxShader->GetPath() )
            .SetLanguage( renderer::rhi::ShaderLanguage::eSlang )
            .SetStage( renderer::rhi::ShaderType::ePixel ) };
        mPixelShader = mDevice->CreateShader( fragmentShaderDescription );

        auto layoutDesc{ renderer::rhi::BindingLayoutDescription{}
            .SetRegisterSpace( 0 )
            .SetShaderVisibility( renderer::rhi::ShaderFlagsBits::All )
            .AddItem( renderer::rhi::BindingLayoutItem::Sampler( 0 ) ) };
        mBindingLayoutHandle = mDevice->CreateBindingLayout( layoutDesc );

        auto bindlessLayout{ renderer::rhi::DescriptorTableLayoutDescription{}
            .SetVisibility( renderer::rhi::ShaderFlagsBits::All )
            .SetRegisterSpace( 1 )
            .AddBindlessItem( renderer::rhi::BindlessLayoutItem::TextureSRV( 0, kBackBufferCount ) ) };
        mBindlessLayout = mDevice->CreateDescriptorTableLayout( bindlessLayout );

        mPipelineLayoutHandle = mDevice->CreatePipelineLayout( renderer::rhi::PipelineLayoutCreateDescription{}
            .AddBindingLayout( mBindingLayoutHandle )
            .AddBindingLayout( mBindlessLayout ) );

        auto graphicsPipelineDescription{ renderer::rhi::GraphicsPipelineDescription{}
            .AddShader( mPixelShader )
            .AddShader( mVertexShader )

            .AddColorFormat( renderer::rhi::Format::eBGRA8_UNORM )
            .SetDepthTest( false )
            .SetDepthWrite( false )

            .SetPolygonMode( renderer::rhi::PolygonMode::eFill )
            .SetCullMode( renderer::rhi::CullMode::eCullBack )
            .SetWindingOrder( renderer::rhi::WindingOrder::eCounterClockwise )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .SetPipelineLayout( mPipelineLayoutHandle ) };

        mPipeline = mDevice->CreatePipeline( graphicsPipelineDescription );

        // Sampler
        auto samplerDes{ renderer::rhi::SamplerCreateDescription{}
            .SetFilter( rhi::SamplerFilter::eLinear )
            .SetWrap( renderer::rhi::SamplerWrapMode::eClampToBorder )
            .SetBorderColor( renderer::rhi::kColorWhite ) };
        mSamplerState = mDevice->CreateSampler( samplerDes );

        // Bindless set
        mDescriptorTable = mDevice->CreateDescriptorTable( mBindlessLayout );

        // Non-bindless set
        auto bindingSetDesc{ renderer::rhi::BindingTableDescription{}
            .AddItem( renderer::rhi::BindingTableItem::Sampler( 0, mSamplerState.GetPtr() ) ) };
        mBindingSetHandle = mDevice->CreateBindingTable( bindingSetDesc, mBindingLayoutHandle );
    }

    auto Context::InitSynchronization() -> void {
        MKT_BEGIN_PROFILER();
        mFrameContexts.resize( GetBackBufferCount() );

        for ( auto& context: mFrameContexts ) {
            context.mFence = mDevice->CreateFence( context.mFenceValue );
        }
    }

    auto Context::InitializeShaderCompiler() -> void {
        MKT_BEGIN_PROFILER();
        mShaderCompiler = eastl::make_unique<ShaderCompiler>();
        mShaderCompiler->Initialize();
    }
}// namespace Mikoto

#endif
