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

#include <Renderer/Core/FrameGraphTypes.hh>

namespace mikoto::renderer {

    auto FGBarrierHistory::IsCurrent( renderer::rhi::ResourceStates state ) const -> bool {
        MKT_BEGIN_PROFILER();
        return mIsValid && mState == state;
    }

    auto FGBarrierHistory::Next(
        renderer::rhi::ResourceStates stateBefore,
        renderer::rhi::ResourceStates stateAfter,
        renderer::rhi::PipelineStageFlags stages,
        renderer::rhi::BarrierAccessFlags accesses ) const -> FGBarrierHistory {
        MKT_BEGIN_PROFILER();
        FGBarrierHistory next{
            .mStages = stages,
            .mAccess = accesses,
            .mState = stateAfter,
            .mIsValid = true
        };

        if ( IsCurrent( stateBefore ) && stateBefore == stateAfter &&
             !( mAccess & renderer::rhi::BarrierAccessFlagsBits::Writes ) && !( accesses & renderer::rhi::BarrierAccessFlagsBits::Writes ) ) {
            next.mStages |= mStages;
            next.mAccess |= mAccess;
        }

        return next;
    }

    auto FGSamplerDescription::SetMaxAnisotropy( float anisotropy ) -> FGSamplerDescription & {
        mMaxAnisotropy = glm::clamp( anisotropy, 1.0f, 16.0f );
        return *this;
    }

    auto FGSamplerDescription::SetMipLevels( float mipLevels ) -> FGSamplerDescription & {
        MKT_BEGIN_PROFILER();
        mMipLevels = mipLevels;
        return *this;
    }

    auto FGSamplerDescription::SetBorderColor( const renderer::rhi::Color &color ) -> FGSamplerDescription & {
        MKT_BEGIN_PROFILER();
        mBorderColor = color;
        return *this;
    }

    auto FGSamplerDescription::SetName( eastl::string_view name ) -> FGSamplerDescription & {
        MKT_BEGIN_PROFILER();
        mName = name;
        return *this;
    }

    auto FGSamplerDescription::SetFilter( renderer::rhi::SamplerFilter filter ) -> FGSamplerDescription & {
        MKT_BEGIN_PROFILER();
        mMinFilter = filter;
        mMagFilter = filter;
        return *this;
    }

    auto FGSamplerDescription::SetMinFilter( renderer::rhi::SamplerFilter filter ) -> FGSamplerDescription & {
        MKT_BEGIN_PROFILER();
        mMinFilter = filter;
        return *this;
    }

    auto FGSamplerDescription::SetMagFilter( renderer::rhi::SamplerFilter filter ) -> FGSamplerDescription & {
        MKT_BEGIN_PROFILER();
        mMagFilter = filter;
        return *this;
    }

    auto FGSamplerDescription::SetWrap( renderer::rhi::SamplerWrapMode wrap ) -> FGSamplerDescription & {
        MKT_BEGIN_PROFILER();
        mWrapU = wrap;
        mWrapV = wrap;
        mWrapW = wrap;
        return *this;
    }

    auto FGSamplerDescription::SetWrapU( renderer::rhi::SamplerWrapMode wrap ) -> FGSamplerDescription & {
        MKT_BEGIN_PROFILER();
        mWrapU = wrap;
        return *this;
    }

    auto FGSamplerDescription::SetWrapV( renderer::rhi::SamplerWrapMode wrap ) -> FGSamplerDescription & {
        MKT_BEGIN_PROFILER();
        mWrapV = wrap;
        return *this;
    }

    auto FGSamplerDescription::SetWrapW( renderer::rhi::SamplerWrapMode wrap ) -> FGSamplerDescription & {
        MKT_BEGIN_PROFILER();
        mWrapW = wrap;
        return *this;
    }

    auto FGPipelineDescription::SetCullMode( renderer::rhi::CullMode mode ) -> FGPipelineDescription & {
        MKT_BEGIN_PROFILER();
        mCullMode = mode;
        return *this;
    }

    auto FGPipelineDescription::SetPolygonMode( renderer::rhi::PolygonMode mode ) -> FGPipelineDescription & {
        MKT_BEGIN_PROFILER();
        mPolygonMode = mode;
        return *this;
    }

    auto FGPipelineDescription::SetMultisampling( renderer::rhi::Multisampling msaa ) -> FGPipelineDescription & {
        MKT_BEGIN_PROFILER();
        mMultisampling = msaa;
        return *this;
    }

    auto FGPipelineDescription::SetDepthTest( bool value ) -> FGPipelineDescription & {
        MKT_BEGIN_PROFILER();
        mEnableDepthTest = value;
        return *this;
    }

    auto FGPipelineDescription::SetDepthWrite( bool value ) -> FGPipelineDescription & {
        MKT_BEGIN_PROFILER();
        mEnableDepthWrite = value;
        return *this;
    }

    auto FGPipelineDescription::SetDepthFormat( renderer::rhi::Format format ) -> FGPipelineDescription & {
        MKT_BEGIN_PROFILER();
        mDepthFormat = format;
        return *this;
    }

    auto FGPipelineDescription::SetAlphaBlendState( const rhi::BlendState &state ) -> FGPipelineDescription & {
        MKT_BEGIN_PROFILER();
        mAlphaBlendState = state;
        return *this;
    }

    auto FGPipelineDescription::SetBlendEnable( bool value ) -> FGPipelineDescription & {
        MKT_BEGIN_PROFILER();
        mAlphaBlendState.SetEnabled( value );
        return *this;
    }

    auto FGPipelineDescription::AddColorFormat( renderer::rhi::Format format ) -> FGPipelineDescription & {
        MKT_BEGIN_PROFILER();
        mColorFormats.push_back( format );
        return *this;
    }

    auto FGPipelineDescription::SetName( eastl::string_view name ) -> FGPipelineDescription & {
        MKT_BEGIN_PROFILER();
        mName = name;
        return *this;
    }

    auto FGPipelineDescription::SetPipelineType( renderer::rhi::PipelineType type ) -> FGPipelineDescription & {
        MKT_BEGIN_PROFILER();
        mPipelineType = type;
        return *this;
    }

    auto FGPipelineDescription::PushShader( const filesystem::Path &path, FGStageType stage, eastl::string_view entryPoint ) -> FGPipelineDescription & {
        MKT_BEGIN_PROFILER();
        mShaders[stage] = path;
        mEntryPoints[stage] = entryPoint;
        return *this;
    }

    auto FGPipelineDescription::SetTopology( renderer::rhi::PrimitiveTopology topology ) -> FGPipelineDescription & {
        MKT_BEGIN_PROFILER();
        mTopology = topology;
        return *this;
    }

    auto FGBufferDescription::SetName( eastl::string_view name ) -> FGBufferDescription & {
        MKT_BEGIN_PROFILER();
        mName = name;
        return *this;
    }

    auto FGBufferDescription::SetInitialData( memory::BufferSpanHandle data ) -> FGBufferDescription & {
        MKT_BEGIN_PROFILER();
        mInitialContents = data;
        mElementSizeBytes = data->GetSize();

        // If you specify initial data it means you to copy it to this
        // resource which means it must support being a copy Dest resource
        SetUsage( renderer::rhi::BufferUsageFlagsBits::CopyDest );

        return *this;
    }

    auto FGBufferDescription::SetUsage( renderer::rhi::BufferUsageFlags flags ) -> FGBufferDescription & {
        MKT_BEGIN_PROFILER();
        mBufferUsageFlags = flags;
        return *this;
    }

    auto FGBufferDescription::SetSizeBytes( core::usize byteSize ) -> FGBufferDescription & {
        MKT_BEGIN_PROFILER();
        mElementSizeBytes = byteSize;
        return *this;
    }

    auto FGBufferDescription::SetElementsSize( core::u32 elementCount, core::usize elementSizeBytes ) -> FGBufferDescription & {
        MKT_BEGIN_PROFILER();
        mElementCount = elementCount;
        mElementSizeBytes = elementSizeBytes;
        return *this;
    }

    auto FGBufferDescription::SetHeapType( renderer::rhi::HeapType heap ) -> FGBufferDescription & {
        MKT_BEGIN_PROFILER();
        mHeapType = heap;
        return *this;
    }

    auto FGTextureDescription::SetName( eastl::string_view name ) -> FGTextureDescription & {
        MKT_BEGIN_PROFILER();
        mName = name;
        return *this;
    }

    auto FGTextureDescription::SetWidth( core::u32 width ) -> FGTextureDescription & {
        MKT_BEGIN_PROFILER();
        mWidth = width;
        return *this;
    }

    auto FGTextureDescription::SetHeight( core::u32 height ) -> FGTextureDescription & {
        MKT_BEGIN_PROFILER();
        mHeight = height;
        return *this;
    }

    auto FGTextureDescription::SetDepth( core::u32 depth ) -> FGTextureDescription & {
        mDepth = depth;
        return *this;
    }

    auto FGTextureDescription::SetMipCount( core::u32 count ) -> FGTextureDescription & {
        MKT_BEGIN_PROFILER();
        mMipCount = count;
        return *this;
    }

    auto FGTextureDescription::SetHeapType( renderer::rhi::HeapType heapType ) -> FGTextureDescription & {
        MKT_BEGIN_PROFILER();
        mHeapType = heapType;
        return *this;
    }

    auto FGTextureDescription::SetMultisampling( renderer::rhi::Multisampling sampleCount ) -> FGTextureDescription & {
        MKT_BEGIN_PROFILER();
        mMultisampling = sampleCount;
        return *this;
    }

    auto FGTextureDescription::SetDimensions( renderer::rhi::TextureDimension dimensions ) -> FGTextureDescription & {
        MKT_BEGIN_PROFILER();
        mDimension = dimensions;
        return *this;
    }

    auto FGTextureDescription::SetFormat( renderer::rhi::Format format ) -> FGTextureDescription & {
        MKT_BEGIN_PROFILER();
        mFormat = format;
        return *this;
    }

    auto FGTextureDescription::SetUsage( renderer::rhi::TextureUsageFlags usage ) -> FGTextureDescription & {
        MKT_BEGIN_PROFILER();
        mUsage = usage;
        return *this;
    }

    auto ResourceVersion::HasWriter() const -> bool {
        MKT_BEGIN_PROFILER();
        return !mWriterPass.empty();
    }
}// namespace mikoto::renderer
