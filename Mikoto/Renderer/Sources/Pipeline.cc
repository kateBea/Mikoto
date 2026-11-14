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

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Logging/Assert.hh>

#include <Renderer/Rhi/Pipeline.hh>

namespace mikoto::renderer::rhi {

    using namespace mikoto::core;
    using namespace mikoto::memory;

    auto ComputePipelineDescription::SetUseReflection( bool value ) -> ComputePipelineDescription& {
        MKT_BEGIN_PROFILER();
        mUseReflection = value;
        return *this;
    }

    auto ComputePipelineDescription::SetComputeStage( ShaderModuleHandle handle ) -> ComputePipelineDescription& {
        MKT_BEGIN_PROFILER();
        mStage = eastl::move( handle );
        return *this;
    }

    auto ComputePipelineDescription::SetPipelineLayout( PipelineLayoutHandle handle ) -> ComputePipelineDescription& {
        MKT_BEGIN_PROFILER();
        mPipelineLayout = eastl::move( handle );
        return *this;
    }

    auto BlendState::SetEnabled( bool value ) -> BlendState& {
        MKT_BEGIN_PROFILER();
        mEnabled = value;
        return *this;
    }

    auto BlendState::SetSrcColorFactor( BlendFactor value ) -> BlendState& {
        MKT_BEGIN_PROFILER();
        mSrcColorFactor = value;
        return *this;
    }

    auto BlendState::SetDstColorFactor( BlendFactor value ) -> BlendState& {
        MKT_BEGIN_PROFILER();
        mDstColorFactor = value;
        return *this;
    }

    auto BlendState::SetColorOperation( BlendOperation value ) -> BlendState& {
        MKT_BEGIN_PROFILER();
        mColorOperation = value;
        return *this;
    }

    auto BlendState::SetSrcAlphaFactor( BlendFactor value ) -> BlendState& {
        MKT_BEGIN_PROFILER();
        mSrcAlphaFactor = value;
        return *this;
    }

    auto BlendState::SetDstAlphaFactor( BlendFactor value ) -> BlendState& {
        MKT_BEGIN_PROFILER();
        mDstAlphaFactor = value;
        return *this;
    }

    auto BlendState::SetAlphaOperation( BlendOperation value ) -> BlendState& {
        MKT_BEGIN_PROFILER();
        mAlphaOperation = value;
        return *this;
    }

    auto BlendState::SetWriteMask( ColorWriteMask value ) -> BlendState& {
        MKT_BEGIN_PROFILER();
        mWriteMask = value;
        return *this;
    }

    auto GraphicsPipelineDescription::SetPipelineLayout( PipelineLayoutHandle handle ) -> GraphicsPipelineDescription& {
        MKT_BEGIN_PROFILER();
        mPipelineLayout = eastl::move( handle );
        return *this;
    }

    auto GraphicsPipelineDescription::SetInputLayout( InputLayoutHandle handle ) -> GraphicsPipelineDescription& {
        MKT_BEGIN_PROFILER();
        mInputLayout = eastl::move( handle );
        return *this;
    }

    auto GraphicsPipelineDescription::SetPolygonMode( PolygonMode mode ) -> GraphicsPipelineDescription& {
        MKT_BEGIN_PROFILER();
        mPolygonMode = mode;
        return *this;
    }

    auto GraphicsPipelineDescription::SetTopology( PrimitiveTopology topology ) -> GraphicsPipelineDescription& {
        MKT_BEGIN_PROFILER();
        mPrimitiveTopology = topology;
        return *this;
    }

    auto GraphicsPipelineDescription::SetMultisampling( Multisampling msaa ) -> GraphicsPipelineDescription& {
        MKT_BEGIN_PROFILER();
        mMultisampling = msaa;
        return *this;
    }


    auto GraphicsPipelineDescription::SetUseReflection( bool value ) -> GraphicsPipelineDescription& {
        MKT_BEGIN_PROFILER();
        mUseReflection = value;
        return *this;
    }

    auto GraphicsPipelineDescription::AddShader( ShaderModuleHandle handle ) -> GraphicsPipelineDescription& {
        MKT_BEGIN_PROFILER();
        mShaders[handle->GetType()] = handle;
        return *this;
    }

    auto GraphicsPipelineDescription::SetCullMode( CullMode mode ) -> GraphicsPipelineDescription& {
        MKT_BEGIN_PROFILER();
        mCullMode = mode;
        return *this;
    }

    auto GraphicsPipelineDescription::SetAlphaBlendState( const BlendState& state ) -> GraphicsPipelineDescription& {
        MKT_BEGIN_PROFILER();
        mAlphaBlendState = state;
        return *this;
    }

    auto GraphicsPipelineDescription::SetBlendEnable( bool value ) -> GraphicsPipelineDescription& {
        MKT_BEGIN_PROFILER();
        mAlphaBlendState.SetEnabled( value );
        return *this;
    }

    auto GraphicsPipelineDescription::SetDepthTest( bool value ) -> GraphicsPipelineDescription& {
        MKT_BEGIN_PROFILER();
        mEnableDepthTest = value;
        return *this;
    }

    auto GraphicsPipelineDescription::SetDepthWrite( bool value ) -> GraphicsPipelineDescription& {
        MKT_BEGIN_PROFILER();
        mEnableDepthWrite = value;
        return *this;
    }

    auto GraphicsPipelineDescription::SetDepthFormat( Format format ) -> GraphicsPipelineDescription& {
        MKT_BEGIN_PROFILER();
        mDepthFormat = format;
        return *this;
    }

    auto GraphicsPipelineDescription::AddColorFormat( Format format ) -> GraphicsPipelineDescription& {
        MKT_BEGIN_PROFILER();
        mColorFormats.push_back( format );
        return *this;
    }

    auto GraphicsPipelineDescription::SetWindingOrder( WindingOrder order ) -> GraphicsPipelineDescription& {
        MKT_BEGIN_PROFILER();
        mWindingOrder = order;
        return *this;
    }

    GraphicsPipeline::GraphicsPipeline( const GraphicsPipelineDescription& desc )
        : mDesc{ desc } {
    }

    auto GraphicsPipeline::GetDescription() const noexcept -> const GraphicsPipelineDescription& {
        MKT_BEGIN_PROFILER();
        return mDesc;
    }

    auto GraphicsPipeline::GetPipelineLayout() const -> PipelineLayoutHandle {
        MKT_BEGIN_PROFILER();
        return mDesc.mPipelineLayout;
    }

    ComputePipeline::ComputePipeline( const ComputePipelineDescription& desc )
        : mDesc{ desc } {
    }

    auto ComputePipeline::GetDescription() const noexcept -> const ComputePipelineDescription& {
        MKT_BEGIN_PROFILER();
        return mDesc;
    }

    auto ComputePipeline::GetPipelineLayout() const -> PipelineLayoutHandle {
        MKT_BEGIN_PROFILER();
        return mDesc.mPipelineLayout;
    }

    auto PipelineLayoutCreateDescription::SetPushConstantSize( core::usize size ) -> PipelineLayoutCreateDescription& {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( size != 0, "Push constants size cannot be zero" );

        mPushConstantsSize = size;
        return *this;
    }

    auto PipelineLayoutCreateDescription::SetPushConstantsVisibility( ShaderFlags stage ) -> PipelineLayoutCreateDescription& {
        MKT_BEGIN_PROFILER();
        mPushConstantsVisibility = stage;
        return *this;
    }

    auto PipelineLayoutCreateDescription::AddBindingLayout( BindingLayoutHandle layout ) -> PipelineLayoutCreateDescription& {
        MKT_BEGIN_PROFILER();
        mBindingLayouts.emplace_back( layout );
        return *this;
    }


    auto InputLayoutCreateDescription::SetShader( ShaderModuleHandle shader ) -> InputLayoutCreateDescription& {
        MKT_BEGIN_PROFILER();
        mShaderModule = shader;
        return *this;
    }

    auto InputLayoutCreateDescription::SetBindings( eastl::span<const VertexBindingDescription> items ) -> InputLayoutCreateDescription& {
        MKT_BEGIN_PROFILER();
        mVertexBindingDescriptions.insert( mVertexBindingDescriptions.end(), items.begin(), items.end() );
        return *this;
    }

    auto InputLayoutCreateDescription::SetAttributes( eastl::span<const VertexAttributeDescription> items ) -> InputLayoutCreateDescription& {
        MKT_BEGIN_PROFILER();
        mVertexAttributeDescriptions.insert( mVertexAttributeDescriptions.end(), items.begin(), items.end() );
        return *this;
    }

    auto InputLayoutCreateDescription::PushBinding( const VertexBindingDescription& desc ) -> InputLayoutCreateDescription& {
        MKT_BEGIN_PROFILER();
        mVertexBindingDescriptions.emplace_back( desc );
        return *this;
    }

    auto InputLayoutCreateDescription::PushAttribute( const VertexAttributeDescription& desc ) -> InputLayoutCreateDescription& {
        MKT_BEGIN_PROFILER();
        mVertexAttributeDescriptions.emplace_back( desc );
        return *this;
    }

    auto GraphicsPipeline::GetPipelineType() const -> PipelineType {
        return PipelineType::eGraphics;
    }


    auto ComputePipeline::GetPipelineType() const -> PipelineType {
        return PipelineType::eCompute;
    }


    RayTracingPipeline::RayTracingPipeline( const RayTracingPipelineDescription& description )
        : mDescription{ description } {
    }

    auto RayTracingPipeline::GetDescription() const -> const RayTracingPipelineDescription& {
        return mDescription;
    }

    auto RayTracingPipeline::GetPipelineType() const -> PipelineType {
        return PipelineType::eRayTracing;
    }

    auto RayTracingPipeline::GetPipelineLayout() const -> PipelineLayoutHandle {
        return mDescription.mPipelineLayout;
    }

}// namespace mikoto::renderer::rhi
