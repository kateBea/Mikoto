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
#include <Core/String.hh>
#include <Core/Types.hh>

#include <Renderer/Rhi/Pipeline.hh>

namespace mikoto::renderer::rhi {

    using namespace mikoto::core;
    using namespace mikoto::memory;

    auto ComputePipelineDescription::SetUseReflection( bool value ) -> ComputePipelineDescription & {
        mUseReflection = value;
        return *this;
    }

    auto ComputePipelineDescription::SetComputeStage( ShaderModuleHandle handle ) -> ComputePipelineDescription & {
        mStage = eastl::move( handle );
        return *this;
    }

    auto ComputePipelineDescription::SetPipelineLayout( PipelineLayoutHandle handle ) -> ComputePipelineDescription & {
        mPipelineLayout = eastl::move( handle );
        return *this;
    }

    auto GraphicsPipelineDescription::SetPipelineLayout( PipelineLayoutHandle handle ) -> GraphicsPipelineDescription & {
        mPipelineLayout = eastl::move( handle );
        return *this;
    }

    auto GraphicsPipelineDescription::SetInputLayout( InputLayoutHandle handle ) -> GraphicsPipelineDescription & {
        mInputLayout = eastl::move( handle );
        return *this;
    }

    auto GraphicsPipelineDescription::SetPolygonMode( PolygonMode mode ) -> GraphicsPipelineDescription & {
        mPolygonMode = mode;
        return *this;
    }

    auto GraphicsPipelineDescription::SetTopology( PrimitiveTopology topology ) -> GraphicsPipelineDescription & {
        mPrimitiveTopology = topology;
        return *this;
    }

    auto GraphicsPipelineDescription::SetMultisampling( Multisampling msaa ) -> GraphicsPipelineDescription & {
        mMultisampling = msaa;
        return *this;
    }


    auto GraphicsPipelineDescription::SetUseReflection( bool value ) -> GraphicsPipelineDescription & {
        mUseReflection = value;
        return *this;
    }

    auto GraphicsPipelineDescription::AddShader( ShaderModuleHandle handle ) -> GraphicsPipelineDescription & {
        mShaders[handle->GetType()] = handle;
        return *this;
    }

    auto GraphicsPipelineDescription::SetCullMode( CullMode mode ) -> GraphicsPipelineDescription & {
        mCullMode = mode;
        return *this;
    }

    auto GraphicsPipelineDescription::SetBlendEnable( bool value ) -> GraphicsPipelineDescription & {
        mEnableAlphaBlending = value;
        return *this;
    }

    auto GraphicsPipelineDescription::SetDepthTest( bool value ) -> GraphicsPipelineDescription & {
        mEnableDepthTest = value;
        return *this;
    }

    auto GraphicsPipelineDescription::SetDepthWrite( bool value ) -> GraphicsPipelineDescription & {
        mEnableDepthWrite = value;
        return *this;
    }

    auto GraphicsPipelineDescription::SetDepthFormat( Format format ) -> GraphicsPipelineDescription & {
        mDepthFormat = format;
        return *this;
    }

    auto GraphicsPipelineDescription::AddColorFormat( Format format ) -> GraphicsPipelineDescription & {
        mColorFormats.push_back( format );
        return *this;
    }

    auto GraphicsPipelineDescription::SetWindingOrder( WindingOrder order ) -> GraphicsPipelineDescription & {
        mWindingOrder = order;
        return *this;
    }

    IGraphicsPipeline::IGraphicsPipeline( const GraphicsPipelineDescription &desc )
        : IPipeline{ PipelineType::eGraphics }, mDesc{ desc } {}

    auto IGraphicsPipeline::GetDescription() const noexcept -> const GraphicsPipelineDescription & {
        return mDesc;
    }

    auto IGraphicsPipeline::GetPipelineLayout() const -> PipelineLayoutHandle {
        return mDesc.mPipelineLayout;
    }

    IComputePipeline::IComputePipeline( const ComputePipelineDescription &desc )
        : IPipeline{ PipelineType::eCompute }, mDesc{ desc } {
    }

    auto IComputePipeline::GetDescription() const noexcept -> const ComputePipelineDescription & {
        return mDesc;
    }

    auto IComputePipeline::GetPipelineLayout() const -> PipelineLayoutHandle {
        return mDesc.mPipelineLayout;
    }

    auto PipelineLayoutCreateDescription::SetPushConstantSize( core::usize size ) -> PipelineLayoutCreateDescription & {
        MKT_ASSERT( size != 0, "Push constants size cannot be zero" );

        mPushConstantsSize = size;
        return *this;
    }

    auto PipelineLayoutCreateDescription::SetPushConstantsVisibility( ShaderFlags stage ) -> PipelineLayoutCreateDescription & {
        mPushConstantsVisibility = stage;
        return *this;
    }

    auto PipelineLayoutCreateDescription::AddBindingLayout( BindingLayoutHandle layout ) -> PipelineLayoutCreateDescription & {
        mBindingLayouts.emplace_back( layout );
        return *this;
    }

    auto IPipeline::GetPipelineType() const -> PipelineType {
        return mPipelineType;
    }

    IPipeline::IPipeline( const PipelineType pipelineType )
        : mPipelineType{ pipelineType } {}

    auto InputLayoutCreateDescription::SetShader( ShaderModuleHandle shader ) -> InputLayoutCreateDescription & {
        mShaderModule = shader;
        return *this;
    }

    auto InputLayoutCreateDescription::SetBindings( eastl::span<const VertexBindingDescription> items ) -> InputLayoutCreateDescription & {
        mVertexBindingDescriptions.insert( mVertexBindingDescriptions.end(), items.begin(), items.end() );
        return *this;
    }

    auto InputLayoutCreateDescription::SetAttributes( eastl::span<const VertexAttributeDescription> items ) -> InputLayoutCreateDescription & {
        mVertexAttributeDescriptions.insert( mVertexAttributeDescriptions.end(), items.begin(), items.end() );
        return *this;
    }

    auto InputLayoutCreateDescription::PushBinding( const VertexBindingDescription &desc ) -> InputLayoutCreateDescription & {
        mVertexBindingDescriptions.emplace_back( desc );
        return *this;
    }

    auto InputLayoutCreateDescription::PushAttribute( const VertexAttributeDescription &desc ) -> InputLayoutCreateDescription & {
        mVertexAttributeDescriptions.emplace_back( desc );
        return *this;
    }
}// namespace mikoto::renderer::rhi