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

#include <cstring>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Pipeline.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )

// D3D12 extension library.
#include <Platform/Windows/Windows.hh>

#include <directx/d3d12.h>
#include <directx/d3dx12.h>
#include <wrl.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>

#include <Renderer/Rhi/Utility.hh>
#include <Renderer/Rhi/D3D12/D3D12Device.hh>
#include <Renderer/Rhi/D3D12/D3D12Shader.hh>
#include <Renderer/Rhi/D3D12/D3D12Pipeline.hh>
#include <Renderer/Rhi/D3D12/Direct3D12Helpers.hh>

namespace mikoto::renderer::d3d12 {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    using Microsoft::WRL::ComPtr;

    static auto CheckRT( HRESULT result ) -> void {
        if ( FAILED( result ) ) {
            MKT_THROW_RUNTIME_ERROR( "D3D12 ray-tracing operation failed." );
        }
    }

    static auto RayDevice( renderer::rhi::IGpuDevice* device ) -> ComPtr<ID3D12Device5> {
        ComPtr<ID3D12Device5> result{};
        CheckRT( static_cast<Device*>( device )->GetDevice()->QueryInterface( IID_PPV_ARGS( &result ) ) );
        return result;
    }

    GraphicsPipeline::GraphicsPipeline( const rhi::GraphicsPipelineDescription& info )
        : rhi::GraphicsPipeline{ info } {
        MKT_BEGIN_PROFILER();
        // Resources
        PipelineLayout* pipelineLayout{ core::checked_cast<PipelineLayout*>( mDesc.mPipelineLayout.GetPtr() ) };

        if ( pipelineLayout ) {
            ID3D12RootSignature* rootSignature{ *pipelineLayout };
            mD3D12PipelineDesc.pRootSignature = rootSignature;
        }

        // Input layout
        if ( !mDesc.mInputLayout.IsEmpty() ) {
            InputLayout* inputLayout{ core::checked_cast<InputLayout*>( mDesc.mInputLayout.GetPtr() ) };

            MKT_ASSERT( inputLayout->GetInputElementsCount() != 0, "Cannot pass an empty input layout." );

            mD3D12PipelineDesc.InputLayout.NumElements = inputLayout->GetInputElementsCount();
            mD3D12PipelineDesc.InputLayout.pInputElementDescs = inputLayout->GetInputElements();
        }

        // Vertex Shader
        MKT_ASSERT( mDesc.mShaders.contains( renderer::rhi::ShaderType::eVertex ),
            "Vertex shader stage is required to construct the graphics pipeline." );

        Shader* vertexShader{ core::checked_cast<Shader*>( mDesc.mShaders.at( renderer::rhi::ShaderType::eVertex ).GetPtr() ) };
        D3D12_SHADER_BYTECODE vsBytecode{};
        vsBytecode.pShaderBytecode = vertexShader->GetContents();
        vsBytecode.BytecodeLength = vertexShader->GetContentsByteSize();
        mD3D12PipelineDesc.VS = vsBytecode;// NOTE: this is safe because the struct gets copied

        // Pixel Shader
        if ( mDesc.mShaders.contains( renderer::rhi::ShaderType::ePixel ) ) {
            Shader* shader{ core::checked_cast<Shader*>( mDesc.mShaders.at( renderer::rhi::ShaderType::ePixel ).GetPtr() ) };
            D3D12_SHADER_BYTECODE psBytecode{};
            psBytecode.pShaderBytecode = shader->GetContents();
            psBytecode.BytecodeLength = shader->GetContentsByteSize();
            mD3D12PipelineDesc.PS = psBytecode;
        }

        // Domain shader
        if ( mDesc.mShaders.contains( renderer::rhi::ShaderType::eDomain ) ) {
            Shader* shader{ core::checked_cast<Shader*>( mDesc.mShaders.at( renderer::rhi::ShaderType::eDomain ).GetPtr() ) };

            D3D12_SHADER_BYTECODE dsBytecode{};
            dsBytecode.pShaderBytecode = shader->GetContents();
            dsBytecode.BytecodeLength = shader->GetContentsByteSize();
            mD3D12PipelineDesc.DS = dsBytecode;
        }

        // Hull shader
        if ( mDesc.mShaders.contains( renderer::rhi::ShaderType::eHull ) ) {
            Shader* shader{ core::checked_cast<Shader*>( mDesc.mShaders.at( renderer::rhi::ShaderType::eHull ).GetPtr() ) };

            D3D12_SHADER_BYTECODE hsBytecode{};
            hsBytecode.pShaderBytecode = shader->GetContents();
            hsBytecode.BytecodeLength = shader->GetContentsByteSize();
            mD3D12PipelineDesc.HS = hsBytecode;
        }

        // Geometry shader
        if ( mDesc.mShaders.contains( renderer::rhi::ShaderType::eGeometry ) ) {
            Shader* shader{ core::checked_cast<Shader*>( mDesc.mShaders.at( renderer::rhi::ShaderType::eGeometry ).GetPtr() ) };

            D3D12_SHADER_BYTECODE gsBytecode{};
            gsBytecode.pShaderBytecode = shader->GetContents();
            gsBytecode.BytecodeLength = shader->GetContentsByteSize();
            mD3D12PipelineDesc.GS = gsBytecode;
        }

        // Translate primitive coverage and face orientation into the native rasterizer state.
        D3D12_RASTERIZER_DESC rasterDesc{};
        rasterDesc.FillMode = d3d12::GetFillMode( mDesc.mPolygonMode );
        rasterDesc.CullMode = d3d12::GetCullMode( mDesc.mCullMode );
        rasterDesc.FrontCounterClockwise = mDesc.mWindingOrder == renderer::rhi::WindingOrder::eCounterClockwise ? TRUE : FALSE;
        rasterDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        rasterDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        rasterDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        rasterDesc.DepthClipEnable = TRUE;
        rasterDesc.MultisampleEnable = mDesc.mMultisampling != renderer::rhi::Multisampling::eMsaaX1;
        rasterDesc.AntialiasedLineEnable = FALSE;
        rasterDesc.ForcedSampleCount = 0;
        rasterDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        mD3D12PipelineDesc.RasterizerState = rasterDesc;
        mD3D12PipelineDesc.PrimitiveTopologyType = d3d12::GetTopologyType( mDesc.mPrimitiveTopology );

        // Apply the shared blend contract to each enabled render target.
        D3D12_BLEND_DESC blendDesc{};
        blendDesc.AlphaToCoverageEnable = FALSE;
        blendDesc.IndependentBlendEnable = FALSE;

        const auto& blend{ mDesc.mAlphaBlendState };
        D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc{};
        defaultRenderTargetBlendDesc.BlendEnable = blend.mEnabled ? TRUE : FALSE;
        defaultRenderTargetBlendDesc.LogicOpEnable = FALSE;
        defaultRenderTargetBlendDesc.SrcBlend = d3d12::GetBlendFactor( blend.mSrcColorFactor );
        defaultRenderTargetBlendDesc.DestBlend = d3d12::GetBlendFactor( blend.mDstColorFactor );
        defaultRenderTargetBlendDesc.BlendOp = d3d12::GetBlendOp( blend.mColorOperation );
        defaultRenderTargetBlendDesc.SrcBlendAlpha = d3d12::GetBlendFactor( blend.mSrcAlphaFactor );
        defaultRenderTargetBlendDesc.DestBlendAlpha = d3d12::GetBlendFactor( blend.mDstAlphaFactor );
        defaultRenderTargetBlendDesc.BlendOpAlpha = d3d12::GetBlendOp( blend.mAlphaOperation );
        defaultRenderTargetBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
        defaultRenderTargetBlendDesc.RenderTargetWriteMask = d3d12::GetColorWriteMask( blend.mWriteMask );

        MKT_ASSERT( mDesc.mColorFormats.size() <= D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT,
            "Number of color formats exceeds maximum concurrent RTV supported" );

        for ( UINT i{ 0 }; i < mDesc.mColorFormats.size(); ++i ) {
            blendDesc.RenderTarget[i] = defaultRenderTargetBlendDesc;
        }

        mD3D12PipelineDesc.BlendState = blendDesc;

        // Avoid enabling depth or stencil operations without a matching attachment.
        const bool hasDepth{ mDesc.mDepthFormat != renderer::rhi::Format::eUnknown };
        mD3D12PipelineDesc.DepthStencilState.DepthFunc = d3d12::GetCompareOp( mDesc.mDepthCompareOp );
        mD3D12PipelineDesc.DepthStencilState.DepthEnable = hasDepth && mDesc.mEnableDepthTest ? TRUE : FALSE;
        mD3D12PipelineDesc.DepthStencilState.DepthWriteMask =
            hasDepth && mDesc.mEnableDepthTest && mDesc.mEnableDepthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
        mD3D12PipelineDesc.DepthStencilState.StencilEnable = hasDepth && mDesc.mEnableStencilTest ? TRUE : FALSE;
        mD3D12PipelineDesc.SampleMask = UINT_MAX;

        // Output
        mD3D12PipelineDesc.NumRenderTargets = mDesc.mColorFormats.size();

        for ( UINT i{ 0 }; i < mDesc.mColorFormats.size(); ++i ) {
            mD3D12PipelineDesc.RTVFormats[i] = d3d12::GetFormat( mDesc.mColorFormats[i] );
        }

        mD3D12PipelineDesc.DSVFormat = mDesc.mEnableDepthTest || mDesc.mEnableStencilTest ? d3d12::GetFormat( mDesc.mDepthFormat ) : DXGI_FORMAT_UNKNOWN;

        // MSAA
        mD3D12PipelineDesc.SampleDesc.Count = d3d12::GetSampleCount( mDesc.mMultisampling );
        mD3D12PipelineDesc.SampleDesc.Quality = 0;

        mRequiresEmptySignature = mDesc.mPipelineLayout.IsEmpty();
    }

    auto GraphicsPipeline::GetNativeHandle( rhi::ObjectType type ) -> rhi::Object {
        MKT_BEGIN_PROFILER();
        return renderer::rhi::Object{ nullptr };
    }

    auto GraphicsPipeline::GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object {
        MKT_BEGIN_PROFILER();
        return renderer::rhi::Object{ nullptr };
    }

    auto GraphicsPipeline::SetDebugName( eastl::string_view name ) -> void {
        MKT_BEGIN_PROFILER();

        if ( name.empty() ) {
            return;
        }

        mDebugName = name;
        mPipelineState->SetName( string::ToWide( mDebugName ).c_str() );
    }

    auto GraphicsPipeline::HasEmptyRootSignature() const -> bool {
        MKT_BEGIN_PROFILER();
        return mRequiresEmptySignature;
    }

    GraphicsPipeline::~GraphicsPipeline() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }

    GraphicsPipeline::operator ID3D12PipelineState*() const {
        MKT_BEGIN_PROFILER();
        return mPipelineState.Get();
    }

    auto GraphicsPipeline::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        Device* device{ core::checked_cast<Device*>( mDevice ) };
        ID3D12Device2* d3d12Device{ device->GetDevice() };

        if ( HasEmptyRootSignature() ) {
            mD3D12PipelineDesc.pRootSignature = device->GetEmptyRootSignature();
        }

        const auto result{ d3d12Device->CreateGraphicsPipelineState(
            &mD3D12PipelineDesc,
            IID_PPV_ARGS( &mPipelineState ) ) };

        if ( FAILED( result ) ) {
            for ( const auto& [stage, handle]: mDesc.mShaders ) {
                const auto* shader{ core::checked_cast<const Shader*>( handle.GetPtr() ) };
                MKT_CORE_LOGGER_ERROR( "D3D12 rejected pipeline shader: {}", shader->GetSourceName() );
            }

            ThrowIfFailed( result );
        }

        mIsAllocated = true;
    }

    auto GraphicsPipeline::Destroy() -> void {
        MKT_BEGIN_PROFILER();
        mIsAllocated = false;
    }

    ComputePipeline::ComputePipeline( const rhi::ComputePipelineDescription& info )
        : rhi::ComputePipeline{ info } {
        MKT_BEGIN_PROFILER();
        // Resources
        PipelineLayout* pipelineLayout{ core::checked_cast<PipelineLayout*>( mDesc.mPipelineLayout.GetPtr() ) };

        if ( pipelineLayout ) {
            ID3D12RootSignature* rootSignature{ *pipelineLayout };
            mD3D12PipelineDesc.pRootSignature = rootSignature;
        }

        // Compute shader
        Shader* shader{ core::checked_cast<Shader*>( mDesc.mStage.GetPtr() ) };

        D3D12_SHADER_BYTECODE csBytecode{};
        csBytecode.pShaderBytecode = shader->GetContents();
        csBytecode.BytecodeLength = shader->GetContentsByteSize();
        mD3D12PipelineDesc.CS = csBytecode;

        mD3D12PipelineDesc.NodeMask = 0;
        mD3D12PipelineDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

        mRequiresEmptySignature = mDesc.mPipelineLayout.IsEmpty();
    }

    auto ComputePipeline::GetNativeHandle( rhi::ObjectType type ) -> rhi::Object {
        MKT_BEGIN_PROFILER();
        return renderer::rhi::Object{ nullptr };
    }

    auto ComputePipeline::GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object {
        MKT_BEGIN_PROFILER();
        return renderer::rhi::Object{ nullptr };
    }

    auto ComputePipeline::SetDebugName( eastl::string_view name ) -> void {
        MKT_BEGIN_PROFILER();

        if ( name.empty() ) {
            return;
        }

        mDebugName = name;
        mPipelineState->SetName( string::ToWide( mDebugName ).c_str() );
    }

    auto ComputePipeline::HasEmptyRootSignature() const -> bool {
        MKT_BEGIN_PROFILER();
        return mRequiresEmptySignature;
    }

    ComputePipeline::operator ID3D12PipelineState*() const {
        MKT_BEGIN_PROFILER();
        return mPipelineState.Get();
    }

    ComputePipeline::~ComputePipeline() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto ComputePipeline::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        Device* device{ core::checked_cast<Device*>( mDevice ) };
        ID3D12Device2* d3d12Device{ device->GetDevice() };

        if ( HasEmptyRootSignature() ) {
            mD3D12PipelineDesc.pRootSignature = device->GetEmptyRootSignature();
        }

        ThrowIfFailed( d3d12Device->CreateComputePipelineState(
            &mD3D12PipelineDesc,
            IID_PPV_ARGS( &mPipelineState ) ) );

        mIsAllocated = true;
    }

    auto ComputePipeline::Destroy() -> void {
        MKT_BEGIN_PROFILER();
        mIsAllocated = false;
    }


    RayTracingPipeline::RayTracingPipeline( const renderer::rhi::RayTracingPipelineDescription& description )
        : rhi::RayTracingPipeline{ description } {
    }

    RayTracingPipeline::~RayTracingPipeline() {
        Destroy();
    }

    auto RayTracingPipeline::Initialize() -> void {
        auto device{ RayDevice( mDevice ) };

        // Separate libraries may all use main. Give each stage a unique state-object export.
        eastl::wstring names[]{ string::ToWide( mDescription.mRayGenerationEntry ), string::ToWide( mDescription.mMissEntry ),
            string::ToWide( mDescription.mClosestHitEntry ) };
        const auto hitGroupName{ string::ToWide( mDescription.mHitGroupName ) };
        mShaderExports = { hitGroupName + L"_RayGeneration", hitGroupName + L"_Miss", hitGroupName + L"_ClosestHit" };

        renderer::rhi::ShaderModuleHandle shaders[]{ mDescription.mRayGeneration, mDescription.mMiss, mDescription.mClosestHit };
        D3D12_EXPORT_DESC exports[3]{};
        D3D12_DXIL_LIBRARY_DESC libraries[3]{};
        D3D12_STATE_SUBOBJECT objects[7]{};

        for ( UINT i{}; i < 3; ++i ) {
            exports[i].Name = mShaderExports[i].c_str();
            exports[i].ExportToRename = names[i].c_str();
            libraries[i].DXILLibrary = { shaders[i]->GetContents(), shaders[i]->GetContentsByteSize() };
            libraries[i].NumExports = 1;
            libraries[i].pExports = &exports[i];
            objects[i] = { D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &libraries[i] };
        }

        // Associate the triangle hit group with the caller's shared RHI layout.
        D3D12_HIT_GROUP_DESC hit{};
        hit.HitGroupExport = hitGroupName.c_str();
        hit.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
        hit.ClosestHitShaderImport = mShaderExports[2].c_str();
        objects[3] = { D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &hit };
        D3D12_RAYTRACING_SHADER_CONFIG shaderConfig{ mDescription.mPayloadBytes, 8 };
        objects[4] = { D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, &shaderConfig };
        ID3D12RootSignature* rootPointer{ *core::checked_cast<PipelineLayout*>( mDescription.mPipelineLayout.GetPtr() ) };
        objects[5] = { D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, &rootPointer };
        D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig{ mDescription.mRecursionDepth };
        objects[6] = { D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, &pipelineConfig };
        D3D12_STATE_OBJECT_DESC pipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE, 7, objects };
        CheckRT( device->CreateStateObject( &pipeline, IID_PPV_ARGS( &mPipeline ) ) );

        mIsAllocated = true;
    }

    auto RayTracingPipeline::Destroy() -> void {
        mPipeline.Reset();
        mIsAllocated = false;
    }

}// namespace mikoto::renderer::d3d12

#endif
