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

#include <EASTL/hash_set.h>

#include <Core/Profiler.hh>

#include <Renderer/Rhi/GpuDevice.hh>

#include <Renderer/CompatibilityRenderer/Passes/CompatibilityGeometryModule.hh>
#include <Renderer/CompatibilityRenderer/Passes/CompatibilitySelectionModule.hh>

namespace mikoto::renderer {
    struct alignas( 16 ) CompatibilityOutlineParameters {
        core::float4 mColor{};
        core::i32 mRadius{};
        core::u32 mOutputForPreview{};
        core::uint2 mPadding{};
    };

    static auto MapSelectionPixel( const ReadPixelViewportInfo& viewport, rhi::Extent2D size ) -> eastl::optional<core::uint2> {
        if ( viewport.mViewportWidth <= 0 || viewport.mViewportHeight <= 0 ) {
            return {};
        }

        const core::float2 uv{
            ( viewport.mX - viewport.mViewportX ) / viewport.mViewportWidth,
            ( viewport.mY - viewport.mViewportY ) / viewport.mViewportHeight
        };
        if ( !size.mWidth || !size.mHeight || !( uv.x >= 0.0f && uv.x < 1.0f && uv.y >= 0.0f && uv.y < 1.0f ) ) {
            return {};
        }

        return glm::min( core::uint2{ uv * core::float2{ size.mWidth, size.mHeight } },
            core::uint2{ size.mWidth - 1, size.mHeight - 1 } );
    }

    auto CompatibilitySelectionModule::Initialize( rhi::IGpuDevice* device ) -> void {
        mDevice = device;
        mOutline.Initialize( device, "../compatibility/slang/CompatibilityOutline_Frag.slang", sizeof( CompatibilityOutlineParameters ), true );
    }

    auto CompatibilitySelectionModule::Cancel() -> void {
        mPending.reset();
        mRecorded = false;
    }

    auto CompatibilitySelectionModule::Request( const ReadPixelViewportInfo& viewport, rhi::Extent2D size ) -> bool {
        if ( !mDevice->IsFeatureSupported( rhi::GpuFeatureSupportFlagsBits::TextureRegionReadback ) ||
             !MapSelectionPixel( viewport, size ) ) {
            return false;
        }

        mPending = viewport;
        mRecorded = false;
        return true;
    }

    auto CompatibilitySelectionModule::GetPickResult() -> eastl::optional<core::u32> {
        if ( !mPending || !mRecorded || !mImage ) {
            return {};
        }

        const rhi::Extent2D size{ .mWidth = mImage->GetWidth(), .mHeight = mImage->GetHeight() };
        const auto location{ MapSelectionPixel( *mPending, size ) };
        mPending.reset();
        core::uint2 pixel{};
        if ( !location || !ReadRegion( *location, { .mWidth = 1, .mHeight = 1 }, { &pixel, 1 } ) ) {
            return core::u32{};
        }

        return pixel.x;
    }

    auto CompatibilitySelectionModule::ReadRectangle( const ReadPixelViewportInfo& first, const ReadPixelViewportInfo& last ) const -> eastl::vector<core::u32> {
        if ( !mRecorded || !mImage ) {
            return {};
        }

        const rhi::Extent2D extent{ .mWidth = mImage->GetWidth(), .mHeight = mImage->GetHeight() };
        const auto a{ MapSelectionPixel( first, extent ) };
        const auto b{ MapSelectionPixel( last, extent ) };
        if ( !a || !b ) {
            return {};
        }

        const auto origin{ glm::min( *a, *b ) };
        const auto end{ glm::max( *a, *b ) };
        const rhi::Extent2D size{ .mWidth = end.x - origin.x + 1, .mHeight = end.y - origin.y + 1 };
        eastl::vector<core::uint2> pixels( core::usize{ size.mWidth } * size.mHeight );
        if ( !ReadRegion( origin, size, pixels ) ) {
            return {};
        }

        eastl::hash_set<core::u32> seen{};
        eastl::vector<core::u32> ids{};
        for ( const auto& pixel: pixels ) {
            if ( pixel.x && seen.insert( pixel.x ).second ) {
                ids.push_back( pixel.x );
            }
        }
        return ids;
    }

    auto CompatibilitySelectionModule::ReadRegion( core::uint2 offset, rhi::Extent2D size, eastl::span<core::uint2> pixels ) const -> bool {
        MKT_BEGIN_PROFILER();

        // Picking runs after the viewport submits its selection pass.
        const rhi::Rect region{
            core::as<core::i32>( offset.x ), core::as<core::i32>( offset.x + size.mWidth ),
            core::as<core::i32>( offset.y ), core::as<core::i32>( offset.y + size.mHeight )
        };
        return mDevice->ReadTextureRegion( mImage.GetPtr(), region,
            { reinterpret_cast<core::u8*>( pixels.data() ), pixels.size() * sizeof( core::uint2 ) } );
    }

    auto CompatibilitySelectionModule::Record( rhi::ICommandList* commands, rhi::Extent2D size, CompatibilityGeometryModule& geometry,
        const CompatibilityEnvironment& environment, rhi::IBuffer& frameUniform ) -> void {
        ResizeCompatibilityTarget( mDevice, mImage, size, rhi::Format::eRG32_UINT,
            rhi::TextureUsageFlagsBits::RenderTarget | rhi::TextureUsageFlagsBits::ShaderResource | rhi::TextureUsageFlagsBits::CopySource,
            "Compatibility selection IDs" );
        ResizeCompatibilityTarget( mDevice, mDepth, size, kCompatibilityDepthFormat, rhi::TextureUsageFlagsBits::DepthTarget,
            "Compatibility selection depth" );

        auto pass{ rhi::RenderPassDescription{}
            .SetScopeName( "Compatibility Selection" )
            .SetRenderArea( rhi::Rect{ core::as<core::i32>( size.mWidth ), core::as<core::i32>( size.mHeight ) } )
            .AddDepthTarget( mDepth )
            .AddRenderTarget( mImage, {} ) };
        commands->SetTransition( rhi::TransitionDescription{}
            .AddTexture( mImage.GetPtr(), rhi::ResourceStates::eRenderTarget )
            .AddTexture( mDepth.GetPtr(), rhi::ResourceStates::eDepthWrite ) );
        commands->BeginRenderPass( pass );
        SetCompatibilityViewport( commands, size );
        geometry.RecordSelection( commands, environment, frameUniform );
        commands->EndRenderPass();
        commands->SetTransition( rhi::TransitionDescription{}.AddTexture( mImage.GetPtr(), rhi::ResourceStates::eShaderResource ) );
        mRecorded = true;
    }

    auto CompatibilitySelectionModule::RecordOutline( rhi::ICommandList* commands, rhi::TextureHandle sceneImage, const core::float4& color,
        core::f32 width, bool outputForPreview ) -> rhi::TextureHandle {
        const rhi::Extent2D size{ .mWidth = sceneImage->GetWidth(), .mHeight = sceneImage->GetHeight() };
        ResizeCompatibilityTarget( mDevice, mOutlineImage, size, kCompatibilityOutputFormat,
            rhi::TextureUsageFlagsBits::RenderTarget | rhi::TextureUsageFlagsBits::ShaderResource | rhi::TextureUsageFlagsBits::CopySource,
            "Compatibility outline color" );

        const CompatibilityOutlineParameters parameters{
            .mColor = color,
            .mRadius = core::as<core::i32>( glm::round( width ) ),
            .mOutputForPreview = outputForPreview ? 1u : 0u
        };
        mOutline.Record( commands, "Compatibility Outline", sceneImage, mOutlineImage, &parameters, sizeof( parameters ), mImage );
        return mOutlineImage;
    }
}
