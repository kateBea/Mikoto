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

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Exception.hh>
#include <Core/Platform.hh>

#include <Filesystem/File.hh>
#include <Filesystem/Path.hh>
#include <Filesystem/FileService.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

#include <Renderer/Rhi/D3D12/D3D12Device.hh>
#include <Renderer/Rhi/D3D12/D3D12Context.hh>
#include <Renderer/Rhi/D3D12/Direct3D12Helpers.hh>

namespace mikoto::renderer::d3d12 {

    using namespace mikoto::core;
    using namespace mikoto::filesystem;
    using namespace mikoto::renderer::rhi;

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

        constexpr UINT dxgiFactoryFlags{ DXGI_CREATE_FACTORY_DEBUG };
        ThrowIfFailed( CreateDXGIFactory2( dxgiFactoryFlags, IID_PPV_ARGS( &mDxgiFactory ) ) );

        // Init the device when the context is ready
        mDevice = IGpuDevice::Create({
            .mApi = GraphicsAPI::eD3D12,
            .mFeaturesSupport{
                // If  the context was created with a window
                // we request for a device with support for presentation
                .mEnablePresentation = mWindow != nullptr,
                .mDeviceType = GpuDeviceType::eDiscrete,
            },
        });
        if ( !mDevice ) {
            MKT_THROW_RUNTIME_ERROR( "Could not initialize D3D12 GPU Device." );
        }
        mDevice->Init();
        if (mWindow) {
            // If no window is provided we use D3D12 headless
            InitializeSwapchain();
        }

        InitializeShaderCompiler();
        InitSwapchainRender();

        return true;
    }

    auto Context::Shutdown() -> void {

        mFence.Reset();

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

        mShaderCompiler->Shutdown();
        mShaderCompiler.reset();
    }

    auto Context::SubmitFrame() -> void {
        Device* device{ checked_cast<Device*>( mDevice.get() ) };
        Queue* queue{ checked_cast<Queue*>( device->GetQueue( QueueType::eGraphics ) ) };

        if (!mPresentTarget.IsEmpty() && !mSwapChain.IsEmpty()) {
#if true
            // Blit via full quad render
            TextureHandle colorImage{ mSwapChain->GetCurrentBackBufferImage() };
            mCommandList->Begin( { .mScopeName = "Blit Swapchain" } );
            mCommandList->SetTransition( mPresentTarget.GetRaw(), ResourceStates::eShaderResource );

            if (mTableUpdateRequired) {
                (void)mDevice->WriteDescriptorTable( mDescriptorTable, BindingSetItem::Texture_SRV( 0, mPresentTarget.GetRaw() ) );
                mTableUpdateRequired = false;
            }

            auto graphicsState{ GraphicsState{}
                .SetRenderArea( Rect{ as<i32>(mSwapChain->GetWidth()), as<i32>(mSwapChain->GetHeight()) } )
                .AddRenderTarget( colorImage, Color{ .0f } ) };
            mCommandList->BeginRendering( graphicsState );

            mCommandList->BindPipeline( mPipeline.GetRaw() );

            auto bindingDescription{ BindResourcesDescription{}
                .SetBindPoint( PipelineType::eGraphics )
                .SetPipelineLayout( mPipelineLayoutHandle.GetRaw() )
                .AddResourceSet( 0, mBindingSetHandle.GetRaw() )
                .AddResourceSet( 1, mDescriptorTable.GetRaw() ) };
            mCommandList->BindPipelineResources( bindingDescription );

            mCommandList->SetViewportState( ViewportState{}
                .AddViewportAndScissorRect( Viewport( as<f32>( mSwapChain->GetWidth() ), as<f32>( mSwapChain->GetHeight() ) ) ) );

            constexpr auto drawArguments{ DrawArguments{}
                .SetVertexCount( 3 ) };
            mCommandList->Draw( drawArguments );

            mCommandList->EndRendering();

            mCommandList->SetTransition( colorImage.GetRaw(), ResourceStates::ePresent );

            mCommandList->End();

            auto submitInfo{ SubmitInfo{}
                .AddCommandList( mCommandList ) };
            queue->ExecuteCommandLists( submitInfo );

#else
            // Blit via copy command
            TextureHandle colorImage{ mSwapChain->GetCurrentBackBufferImage() };
            mCommandList->Begin( { .mScopeName = "Blit Swapchain" } );

            const TextureSlice srcSlice{
                .mWidth = (u32)mPresentTarget->GetWidth(),
                .mHeight = (u32)mPresentTarget->GetHeight() };

            const TextureSlice dstSlice{
                .mWidth = (u32)colorImage->GetWidth(),
                .mHeight = (u32)colorImage->GetHeight() };

            mCommandList->Copy(
                mPresentTarget.GetRaw(), srcSlice,
                colorImage.GetRaw(), dstSlice );

            mCommandList->SetTransition( colorImage.GetRaw(), ResourceStates::ePresent );

            mCommandList->End();

            auto submitInfo{ SubmitInfo{}
                .AddCommandList( mCommandList ) };
            mDevice->GetQueue(QueueType::eGraphics)->ExecuteCommandLists( submitInfo );
#endif
        }


        // Wait for previous frame to finish.
        // This is not the best, doing it for now for simplicity and testing purposes.
        Fence* pFence{ checked_cast<Fence*>( mFence.GetRaw() ) };
        ( void )pFence->Signal( mFenceValue );
        ++mFenceValue;

        (void)pFence->Wait( mFenceValue, INFINITE );
    }

    auto Context::PrepareFrame() -> void {
        mDevice->RunGarbageCollection();
    }

    auto Context::Update() -> void {

    }

    auto Context::Present() -> void {
        mSwapChain->Present();
    }

    auto Context::SetPresentTarget( TextureHandle texture ) -> void {
        if (texture.GetRaw() != mPresentTarget.GetRaw()) {
            mPresentTarget = texture;
            mTableUpdateRequired = true;
        }
    }

    auto Context::SetRefreshRate( RefreshRate rate ) -> void {
        mRefreshRate = rate;
        mSwapChain->SetRefreshRate( mRefreshRate );
    }

    auto Context::BatchSubmission( rhi::SubmitInfo&&submitInfo, rhi::QueueType queue ) -> void {

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
#else
        return nullptr;
#endif
    }

    auto Context::GetDebugController() const -> ID3D12Debug6* {
#if !defined(NDEBUG)
        return mDebugController.Get();
#else
        return nullptr;
#endif
    }

    auto Context::InitializeSwapchain() -> void {
        mSwapChain = checked_cast<Device*>( GetGpuDevice() )->CreateSwapChain( mWindow, mDxgiFactory );
        if (!mSwapChain.IsEmpty()) {
            mSwapChain->SetRefreshRate( mRefreshRate );
        }
    }

    auto Context::InitSwapchainRender() -> void {
        mFence = mDevice->CreateFence( mFenceValue );

        // Create shaders
        FileHandle vsShader{ FileService::Get()->LoadFile( "Resources/Shaders/slang/SwapChainBlit_Vert.slang" ) };
        auto vertexShaderDescription{ ShaderModuleCreateDescription{}
            .SetContents( vsShader )
            .SetModuleName( vsShader->GetName() )
            .SetModulePath( vsShader->GetPath() )
            .SetLanguage( ShaderLanguage::eSlang )
            .SetStage( ShaderType::eVertex ) };
        mVertexShader = mDevice->CreateShader( vertexShaderDescription );

        FileHandle pxShader{ FileService::Get()->LoadFile( "Resources/Shaders/slang/SwapChainBlit_Frag.slang" ) };
        auto fragmentShaderDescription{ ShaderModuleCreateDescription{}
            .SetContents( pxShader )
            .SetModuleName( pxShader->GetName() )
            .SetModulePath( pxShader->GetPath() )
            .SetLanguage( ShaderLanguage::eSlang )
            .SetStage( ShaderType::ePixel ) };
        mPixelShader = mDevice->CreateShader( fragmentShaderDescription );

        auto layoutDesc{ BindingLayoutDescription{}
            .SetRegisterSpace( 0 )
            .SetShaderVisibility(ShaderFlagsBits::kAll)
            .AddItem(BindingLayoutItem::Sampler(0))};
        mBindingLayoutHandle = mDevice->CreateBindingLayout(layoutDesc);

        auto bindlessLayout{ BindlessLayoutDescription{}
            .SetVisibility(ShaderFlagsBits::kAll)
            .SetRegisterSpace( 1 )
            .AddBindlessItem(BindlessLayoutItem::Texture_SRV(0, 1)) }; // I just need one image slot I can update
        mBindlessLayout = mDevice->CreateBindlessLayout( bindlessLayout );

        mPipelineLayoutHandle = mDevice->CreatePipelineLayout( PipelineLayoutCreateDescription{}
            .AddBindingLayout( mBindingLayoutHandle )
            .AddBindingLayout( mBindlessLayout ));

        auto graphicsPipelineDescription{ GraphicsPipelineDescription{}
            .AddShader( mPixelShader )
            .AddShader( mVertexShader )

            .AddColorFormat( Format::eBGRA8_UNORM )

            .SetPolygonMode( PolygonMode::eFill )
            .SetCullMode( CullMode::eCullBack )
            .SetWindingOrder( WindingOrder::eCounterClockwise )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .SetPipelineLayout( mPipelineLayoutHandle ) };

        mPipeline = mDevice->CreatePipeline( graphicsPipelineDescription );

        // Sampler
        auto samplerDes{ SamplerCreateDescription{}
            .SetFilter( rhi::SamplerFilter::eLinear )
            .SetWrap( SamplerWrapMode::eClampToBorder )
            .SetBorderColor( kColorWhite ) };
        mSamplerState = mDevice->CreateSampler( samplerDes );

        // Bindless set
        mDescriptorTable = mDevice->CreateDescriptorTable( mBindlessLayout );

        // Non-bindless set
        auto bindingSetDesc{ BindingSetDescription{}
            .AddItem( BindingSetItem::Sampler( 0, mSamplerState.GetRaw() ) ) };
        mBindingSetHandle = mDevice->CreateBindingSet( bindingSetDesc, mBindingLayoutHandle );

        mCommandList = mDevice->CreateCommandList( QueueType::eGraphics );
        mCommandList->SetDebugName( "Context Swapchain CommandBuffer" );
    }

    auto Context::InitializeShaderCompiler() -> void {
        mShaderCompiler = eastl::make_unique<ShaderCompiler>();
        mShaderCompiler->Initialize();
    }
}// namespace Mikoto

#endif
