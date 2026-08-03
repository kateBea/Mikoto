//    Copyright 2025 ケイト
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
#include <Core/Types.hh>
#include <Core/Platform.hh>

#include <Renderer/Core/Rhi.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

// D3D12 extension library.
#include <directx/d3d12.h>
#include <directx/d3dx12.h>

#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <wrl.h>

#include <Renderer/D3D12/D3D12Device.hh>
#include <Renderer/D3D12/D3D12Shader.hh>
#include <Renderer/D3D12/D3D12Pipeline.hh>
#include <Renderer/D3D12/Direct3D12Helpers.hh>

namespace mikoto::renderer::d3d12 {

    using namespace mikoto::renderer::rhi;

    GraphicsPipeline::GraphicsPipeline( const rhi::GraphicsPipelineDescription &info )
        : IGraphicsPipeline{ info }
    {
        // Resources
        PipelineLayout* pipelineLayout{ checked_cast<PipelineLayout*>( mDesc.mPipelineLayout.GetRaw() ) };
        ID3D12RootSignature* rootSignature{ *pipelineLayout };
        mD3D12PipelineDesc.pRootSignature = rootSignature;

        // Input layout
        if (!mDesc.mInputLayout.IsEmpty()) {
            InputLayout* inputLayout{ checked_cast<InputLayout*>( mDesc.mInputLayout.GetRaw() ) };

            MKT_ASSERT( inputLayout->GetInputElementsCount() != 0, "Cannot pass an empty input layout.");

            mD3D12PipelineDesc.InputLayout.NumElements = inputLayout->GetInputElementsCount();
            mD3D12PipelineDesc.InputLayout.pInputElementDescs = inputLayout->GetInputElements();
        }

        // Vertex Shader
        MKT_ASSERT( mDesc.mShaders.contains( ShaderType::eVertex ),
            "Vertex shader stage is required to construct the graphics pipeline.");

        Shader* vertexShader{ checked_cast<Shader*>( mDesc.mShaders.at( ShaderType::eVertex ).GetRaw() ) };
        D3D12_SHADER_BYTECODE vsBytecode{};
        vsBytecode.pShaderBytecode = vertexShader->GetContents();
        vsBytecode.BytecodeLength = vertexShader->GetContentsByteSize();
        mD3D12PipelineDesc.VS = vsBytecode; // NOTE: this is safe because the struct gets copied

        // Pixel Shader
        if (mDesc.mShaders.contains( ShaderType::ePixel )) {
            Shader* shader{ checked_cast<Shader*>( mDesc.mShaders.at( ShaderType::ePixel ).GetRaw() ) };
            D3D12_SHADER_BYTECODE psBytecode{};
            psBytecode.pShaderBytecode = shader->GetContents();
            psBytecode.BytecodeLength = shader->GetContentsByteSize();
            mD3D12PipelineDesc.PS = psBytecode;
        }

        // Domain shader
        if (mDesc.mShaders.contains( ShaderType::eDomain )) {
            Shader* shader{ checked_cast<Shader*>( mDesc.mShaders.at( ShaderType::eDomain ).GetRaw() ) };

            D3D12_SHADER_BYTECODE dsBytecode{};
            dsBytecode.pShaderBytecode = shader->GetContents();
            dsBytecode.BytecodeLength = shader->GetContentsByteSize();
            mD3D12PipelineDesc.PS = dsBytecode;
        }

        // Hull shader
        if (mDesc.mShaders.contains( ShaderType::eHull )) {
            Shader* shader{ checked_cast<Shader*>( mDesc.mShaders.at( ShaderType::eHull ).GetRaw() ) };

            D3D12_SHADER_BYTECODE hsBytecode{};
            hsBytecode.pShaderBytecode = shader->GetContents();
            hsBytecode.BytecodeLength = shader->GetContentsByteSize();
            mD3D12PipelineDesc.PS = hsBytecode;
        }

        // Geometry shader
        if (mDesc.mShaders.contains( ShaderType::eGeometry )) {
            Shader* shader{ checked_cast<Shader*>( mDesc.mShaders.at( ShaderType::eHull ).GetRaw() ) };

            D3D12_SHADER_BYTECODE gsBytecode{};
            gsBytecode.pShaderBytecode = shader->GetContents();
            gsBytecode.BytecodeLength = shader->GetContentsByteSize();
            mD3D12PipelineDesc.PS = gsBytecode;
        }

        // Rasterization
        D3D12_RASTERIZER_DESC rasterDesc;
        rasterDesc.FillMode = d3d12::GetFillMode(mDesc.mPolygonMode);
        rasterDesc.CullMode = d3d12::GetCullMode(mDesc.mCullMode);
        rasterDesc.FrontCounterClockwise = mDesc.mWindingOrder == WindingOrder::eCounterClockwise ? TRUE : FALSE;
        rasterDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        rasterDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        rasterDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        rasterDesc.DepthClipEnable = TRUE;
        rasterDesc.MultisampleEnable = mDesc.mMultisampling != Multisampling::eMsaaX1;
        rasterDesc.AntialiasedLineEnable = FALSE;
        rasterDesc.ForcedSampleCount = 0;
        rasterDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
        mD3D12PipelineDesc.RasterizerState = rasterDesc;
        mD3D12PipelineDesc.PrimitiveTopologyType = d3d12::GetTopologyType(mDesc.mPrimitiveTopology);

        // Color/Blend
        D3D12_BLEND_DESC blendDesc{};
        blendDesc.AlphaToCoverageEnable = FALSE;
        blendDesc.IndependentBlendEnable = FALSE;
        const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc{
            mDesc.mEnableAlphaBlending ? TRUE : FALSE,
            FALSE,
            D3D12_BLEND_ONE,
            D3D12_BLEND_ZERO,
            D3D12_BLEND_OP_ADD,
            D3D12_BLEND_ONE,
            D3D12_BLEND_ZERO,
            D3D12_BLEND_OP_ADD,
            D3D12_LOGIC_OP_NOOP,
            D3D12_COLOR_WRITE_ENABLE_ALL };

        MKT_ASSERT( mDesc.mColorFormats.size() <= D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT,
            "Number of color formats exceeds maximum concurrent RTV supported");

        for ( UINT i{ 0 }; i < mDesc.mColorFormats.size(); ++i ) {
            blendDesc.RenderTarget[i] = defaultRenderTargetBlendDesc;
        }
        mD3D12PipelineDesc.BlendState = blendDesc;

        // Depth/Stencil State
        mD3D12PipelineDesc.DepthStencilState.DepthFunc = d3d12::GetDepthCompareOp(mDesc.mDepthCompareOp);
        mD3D12PipelineDesc.DepthStencilState.DepthEnable = mDesc.mEnableDepthTest ? TRUE : FALSE;
        mD3D12PipelineDesc.DepthStencilState.StencilEnable = FALSE;
        mD3D12PipelineDesc.SampleMask = UINT_MAX;

        // Output
        mD3D12PipelineDesc.NumRenderTargets = mDesc.mColorFormats.size();
        for ( UINT i{ 0 }; i < mDesc.mColorFormats.size(); ++i ) {
            mD3D12PipelineDesc.RTVFormats[i] = d3d12::GetFormat( mDesc.mColorFormats[i] );
        }
        mD3D12PipelineDesc.DSVFormat = d3d12::GetFormat( mDesc.mDepthFormat );;
        mD3D12PipelineDesc.SampleDesc.Count = 1;
    }

    auto GraphicsPipeline::GetNativeHandle( rhi::ObjectType type ) -> rhi::Object {
        return IGraphicsPipeline::GetNativeHandle( type );
    }

    auto GraphicsPipeline::GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object {
        return IGraphicsPipeline::GetNativeHandle( type );
    }

    auto GraphicsPipeline::SetDebugName( eastl::string_view name ) -> void {
        IGraphicsPipeline::SetDebugName( name );
    }

    GraphicsPipeline::~GraphicsPipeline() {
        if (mIsAllocated) {
            Release();
        }
    }

    GraphicsPipeline::operator ID3D12PipelineState*() const {
        return mPipelineState.Get();
    }

    auto GraphicsPipeline::Initialize() -> void {
        Device* device{ checked_cast<Device*>( mDevice ) };
        ID3D12Device2* d3d12Device{ device->GetDevice() };

        ThrowIfFailed(d3d12Device->CreateGraphicsPipelineState(
            &mD3D12PipelineDesc,
            IID_PPV_ARGS(&mPipelineState)));

        mIsAllocated = true;
    }

    auto GraphicsPipeline::Release() -> void {
        mIsAllocated = false;
    }

    ComputePipeline::ComputePipeline( const rhi::ComputePipelineDescription &info )
        : IComputePipeline{ info }
    {
        // Resources
        PipelineLayout* pipelineLayout{ checked_cast<PipelineLayout*>( mDesc.mPipelineLayout.GetRaw() ) };
        ID3D12RootSignature* rootSignature{ *pipelineLayout };
        mD3D12PipelineDesc.pRootSignature = rootSignature;

        // Compute shader
        Shader* shader{ checked_cast<Shader*>( mDesc.mStage.GetRaw() ) };

        D3D12_SHADER_BYTECODE csBytecode{};
        csBytecode.pShaderBytecode = shader->GetContents();
        csBytecode.BytecodeLength = shader->GetContentsByteSize();
        mD3D12PipelineDesc.CS = csBytecode;

        mD3D12PipelineDesc.NodeMask = 0;
        mD3D12PipelineDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    }

    auto ComputePipeline::GetNativeHandle( rhi::ObjectType type ) -> rhi::Object {
        return IComputePipeline::GetNativeHandle( type );
    }

    auto ComputePipeline::GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object {
        return IComputePipeline::GetNativeHandle( type );
    }

    auto ComputePipeline::SetDebugName( eastl::string_view name ) -> void {
        if (name.empty()) {
            return;
        }

        mDebugName = name;
        mPipelineState->SetName( string::ToWide( mDebugName ).c_str() );
    }

    ComputePipeline::operator ID3D12PipelineState*() const {
        return mPipelineState.Get();
    }

    ComputePipeline::~ComputePipeline() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto ComputePipeline::Initialize() -> void {
        Device* device{ checked_cast<Device*>( mDevice ) };
        ID3D12Device2* d3d12Device{ device->GetDevice() };

        ThrowIfFailed(d3d12Device->CreateComputePipelineState(
            &mD3D12PipelineDesc,
            IID_PPV_ARGS(&mPipelineState)));

        mIsAllocated = true;
    }

    auto ComputePipeline::Release() -> void {
        mIsAllocated = false;
    }
}// namespace mikoto::renderer::d3d12

#endif