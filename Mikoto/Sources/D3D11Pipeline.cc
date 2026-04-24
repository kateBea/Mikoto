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

#if defined( MIKOTO_PLATFORM_WINDOWS )

#include <Renderer/D3D11/D3D11Device.hh>
#include <Renderer/D3D11/D3D11Pipeline.hh>
#include <Renderer/D3D11/Direct3D11Helpers.hh>

namespace mikoto::renderer::d3d11 {

    GraphicsPipeline::GraphicsPipeline( const rhi::GraphicsPipelineDescription &desc )
        : IGraphicsPipeline{ desc }
    {}

    auto GraphicsPipeline::GetDescription() const noexcept -> const rhi::GraphicsPipelineDescription& {
        return mDesc;
    }

    auto GraphicsPipeline::GetNativeHandle( rhi::ObjectType type ) -> rhi::Object {
        switch (type) {
            case rhi::ObjectType::D3D11_BlendState:
                return rhi::Object(mBlendState.Get());
            case rhi::ObjectType::D3D11_RasterizerState:
                return rhi::Object(mRasterizerState.Get());
            case rhi::ObjectType::D3D11_DepthStencilState:
                return rhi::Object(mDepthStencilStace.Get());

            default:
                return rhi::Object(nullptr);
        }

        return rhi::Object(nullptr);
    }

    auto GraphicsPipeline::GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object {
        switch (type) {
            case rhi::ObjectType::D3D11_BlendState:
                return rhi::Object(mBlendState.Get());
            case rhi::ObjectType::D3D11_RasterizerState:
                return rhi::Object(mRasterizerState.Get());
            case rhi::ObjectType::D3D11_DepthStencilState:
                return rhi::Object(mDepthStencilStace.Get());

            default:
                return rhi::Object(nullptr);
        }

        return rhi::Object(nullptr);
    }

    GraphicsPipeline::~GraphicsPipeline() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto GraphicsPipeline::Initialize() -> void {
        // Rasterization
        auto desc{ D3D11_RASTERIZER_DESC{
            .FillMode = (mDesc.mPolygonMode == PolygonMode::eLines)
                ? D3D11_FILL_WIREFRAME
                : D3D11_FILL_SOLID,

            .CullMode = GetCullMode(mDesc.mCullMode),

            .FrontCounterClockwise = (mDesc.mWindingOrder == WindingOrder::eCounterClockwise),

            .DepthBias             = 0,
            .DepthBiasClamp        = 0.0f,
            .SlopeScaledDepthBias  = 0.0f,

            .DepthClipEnable       = TRUE,

            .ScissorEnable         = FALSE, // can expose later

            .MultisampleEnable     = (mDesc.mMultisampling != Multisampling::eMsaaX1),

            .AntialiasedLineEnable = FALSE
        }};

        if (FAILED(checked_cast<Device*>(mDevice)->GetDevice()->CreateRasterizerState(&desc, &mRasterizerState))) {
            MKT_ASSERT( false, "Failed to create raster state");
        }

        // Blend state
        // One for each color attachments this pipeline will support
        D3D11_BLEND_DESC blendDesc{};
        blendDesc.AlphaToCoverageEnable = FALSE;
        blendDesc.IndependentBlendEnable = TRUE;

        for (u32 i{}; i < mDesc.mColorFormats.size(); ++i) {
            auto& rt { blendDesc.RenderTarget[i] };

            if (mDesc.mEnableAlphaBlending) {
                rt.BlendEnable = TRUE;

                rt.SrcBlend       = D3D11_BLEND_SRC_ALPHA;
                rt.DestBlend      = D3D11_BLEND_INV_SRC_ALPHA;
                rt.BlendOp        = D3D11_BLEND_OP_ADD;

                rt.SrcBlendAlpha  = D3D11_BLEND_ONE;
                rt.DestBlendAlpha = D3D11_BLEND_ZERO;
                rt.BlendOpAlpha   = D3D11_BLEND_OP_ADD;
            } else {
                rt.BlendEnable = FALSE;
            }

            rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        }

        if (FAILED(checked_cast<Device*>(mDevice)->GetDevice()->CreateBlendState(&blendDesc, &mBlendState))) {
            MKT_ASSERT( false, "Failed to create blend state" );
        }

        // Depth stencil state
        D3D11_DEPTH_STENCIL_DESC depthDesc{};
        depthDesc.DepthEnable = mDesc.mEnableDepthTest;
        depthDesc.DepthWriteMask = mDesc.mEnableDepthWrite
            ? D3D11_DEPTH_WRITE_MASK_ALL
            : D3D11_DEPTH_WRITE_MASK_ZERO;

        depthDesc.DepthFunc = GetComparisonFunc(mDesc.mDepthCompareOp);

        depthDesc.StencilEnable = mDesc.mEnableStencilTest;

        depthDesc.StencilReadMask  = D3D11_DEFAULT_STENCIL_READ_MASK;
        depthDesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;

        // Default stencil ops
        depthDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
        depthDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        depthDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
        depthDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;

        depthDesc.BackFace = depthDesc.FrontFace;

        if (FAILED(checked_cast<Device*>(mDevice)->GetDevice()->CreateDepthStencilState(&depthDesc, &mDepthStencilStace))) {
            MKT_ASSERT( false, "Failed to create depth state" );
        }

        mIsAllocated = true;
    }

    auto GraphicsPipeline::Release() -> void {
        mIsAllocated = false;
    }

    ComputePipeline::ComputePipeline( const rhi::ComputePipelineDescription &desc )
        : IComputePipeline{ desc }
    {}

    ComputePipeline::~ComputePipeline() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto ComputePipeline::Initialize() -> void {
        mIsAllocated = true;
    }

    auto ComputePipeline::Release() -> void {
        mIsAllocated = false;
    }
}// namespace mikoto::renderer::d3d11

#endif