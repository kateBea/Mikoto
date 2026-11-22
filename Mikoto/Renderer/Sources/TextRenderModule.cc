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


#include <span>
#include <ranges>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Logging/Logger.hh>

#include <Assets/FontFactory.hh>

#include <Text/Utility.hh>
#include <Text/FontAtlas.hh>
#include <Text/TextLayout.hh>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/ForwardRenderer/Passes/PrepassModule.hh>
#include <Renderer/ForwardRenderer/Passes/TextRenderModule.hh>
#include <Renderer/ForwardRenderer/Passes/PresentationModule.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryShadingModule.hh>

#include <Scene/Scene.hh>


namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::scene;
    using namespace mikoto::renderer::rhi;

    auto TextRenderModule::CreatePipeline( const char* name, bool slug, bool world ) -> FGPipelineDescription {
        MKT_BEGIN_PROFILER();
        auto result{ FGPipelineDescription{}
            .SetName( name )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .SetCullMode( renderer::rhi::CullMode::eNone )
            .SetBlendEnable( true )
            .SetDepthTest( world )
            .SetDepthWrite( false )
            .AddColorFormat( world ? renderer::rhi::Format::eRGBA16_FLOAT : renderer::rhi::Format::eBGRA8_UNORM )
            .PushShader( slug ? "SlugHornText_Vert.slang" : "MSDFText_Vert.slang", FGStageType::eVertex )
            .PushShader( slug ? "SlugHornText_Frag.slang" : "MSDFText_Frag.slang", FGStageType::ePixel ) };

        if ( world ) {
            result.SetDepthFormat( renderer::rhi::Format::eD32 );
        }

        return result;
    }

    TextRenderModule::TextRenderModule( renderer::rhi::RenderResolution resolution )
        : mResolution{ resolution } {
    }

    auto TextRenderModule::SetScene( const scene::Scene* scene ) -> void {
        MKT_BEGIN_PROFILER();
        mScene = scene;
    }

    auto TextRenderModule::SetCamera( const scene::Camera* camera ) -> void {
        MKT_BEGIN_PROFILER();
        mCamera = camera;
    }

    auto TextRenderModule::RegisterTextRender( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER();
        auto& info = graph.GetOrCreate<TextRenderingPassParameters>();
        info.mMsdfPipeline = graph.Create( CreatePipeline( "MSDFText_World", false, true ) );
        info.mSlugWorldPipeline = graph.Create( CreatePipeline( "SlugText_World", true, true ) );

        info.mMsdfTextRenderData = graph.Create( FGBufferDescription{}
            .SetName( "MSDFText_RenderData" )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured | renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetSizeBytes( kMaxGlyphs * sizeof( TextDrawParameters ) )
            .SetHeapType( renderer::rhi::HeapType::eDeviceLocal ) );

        graph.RegisterPass<TextRenderingPassParameters>(
            "MSDFText_Upload",
            FGPassType::eTransfer,
            []( FGNodeBuilder& b, TextRenderingPassParameters& p ) {
                b.UseResource( p.mMsdfTextRenderData, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& ctx, core::Blackboard& b ) {
                SetupTextRenderData( ctx, b );

                if ( mGlyphCount ) {
                    ctx.CopyBuffer( b.Get<TextRenderingPassParameters>().mMsdfTextRenderData, 0,
                        mTextInfo.data(), sizeof( TextDrawParameters ) * mGlyphCount );
                }
            } );

        graph.RegisterPass(
            "MSDFTextPass_Render",
            FGPassType::eGraphics,
            []( FGNodeBuilder& builder, core::Blackboard& b ) {
                builder.UseResource( b.Get<TextRenderingPassParameters>().mMsdfTextRenderData, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<GeomShadingModuleInfo>().mColorImage, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                builder.UseResource( b.Get<PrepassModuleInfo>().mPrepassDepthTarget, FGPipelineStage::eDepthTarget, FGResourceAccess::eRead );
            },
            [this]( CommandContext& ctx, core::Blackboard& b ) {
                if ( !mWorldGlyphCount ) {
                    return;
                }

                const auto& p = b.Get<TextRenderingPassParameters>();
                const auto& color = b.Get<GeomShadingModuleInfo>();
                struct DrawParams {
                    core::u32 sampler{};
                    core::u32 buffer{};
                    core::u32 glyphOffset{};
                } params{
                    ctx.PushSampler( color.mDefaultSampler ),
                    ctx.PushBuffer_SRV( p.mMsdfTextRenderData ),
                    0
                };

                const auto dimensions = InferDimensions( mResolution );

                ctx.BeginRender( RenderBeginDescription{}
                    .SetRenderArea( renderer::rhi::Rect{ core::as<core::i32>( dimensions.first ), core::as<core::i32>( dimensions.second ) } )
                    .AddDepthTarget( b.Get<PrepassModuleInfo>().mPrepassDepthTarget, renderer::rhi::LoadOp::eLoad )
                    .AddRenderTarget( color.mColorImage, renderer::rhi::kColorBlack, renderer::rhi::LoadOp::eLoad ) );

                ctx.SetViewportState( renderer::rhi::ViewportState{}
                    .AddViewportAndScissorRect( renderer::rhi::Viewport( core::as<core::u32>( dimensions.first ), core::as<core::u32>( dimensions.second ) ) ) );

                for ( const auto& batch: mBatches ) {
                    if ( !batch.mIsWorld ) {
                        continue;
                    }

                    ctx.BindPipeline( batch.mIsSlug ? p.mSlugWorldPipeline : p.mMsdfPipeline );

                    params.glyphOffset = batch.mOffset;

                    ctx.PushConstants( params );
                    ctx.Draw( 6, batch.mCount );
                }

                ctx.EndRender();
            } );
    }

    auto TextRenderModule::RegisterScreenPass( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER();

        auto& info{ graph.GetOrCreate<TextRenderingPassParameters>() };
        info.mScreenPipeline = graph.Create( CreatePipeline( "MSDFText_Screen", false, false ) );
        info.mSlugScreenPipeline = graph.Create( CreatePipeline( "SlugText_Screen", true, false ) );

        graph.RegisterPass(
            "MSDFText_ScreenOverlay",
            FGPassType::eGraphics,
            []( FGNodeBuilder& builder, core::Blackboard& b ) {
                builder.UseResource( b.Get<TextRenderingPassParameters>().mMsdfTextRenderData, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<PresentationPassData>().mColorImage, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& ctx, core::Blackboard& b ) {
                if ( mGlyphCount <= mWorldGlyphCount ) {
                    return;
                }

                const auto& p = b.Get<TextRenderingPassParameters>();
                const auto [width, height] = InferDimensions( mResolution );

                ctx.BeginRender( RenderBeginDescription{}.SetRenderArea( renderer::rhi::Rect{ core::as<core::i32>( width ), core::as<core::i32>( height ) } ).AddRenderTarget( b.Get<PresentationPassData>().mColorImage, renderer::rhi::kColorBlack, renderer::rhi::LoadOp::eLoad ) );
                ctx
                    .SetViewportState( renderer::rhi::ViewportState{}
                        .AddViewportAndScissorRect( renderer::rhi::Viewport( core::as<core::u32>( width ), core::as<core::u32>( height ) ) ) );

                struct DrawParams {
                    core::u32 mSampler{};
                    core::u32 mBuffer{};
                    core::u32 mGlyphOffset{};
                } params{
                    ctx.PushSampler( b.Get<GeomShadingModuleInfo>().mDefaultSampler ),
                    ctx.PushBuffer_SRV( p.mMsdfTextRenderData ), core::as<core::u32>( mWorldGlyphCount )
                };

                for ( const auto& batch: mBatches ) {
                    if ( batch.mIsWorld ) {
                        continue;
                    }

                    ctx.BindPipeline( batch.mIsSlug ? p.mSlugScreenPipeline : p.mScreenPipeline );

                    params.mGlyphOffset = batch.mOffset;
                    ctx.PushConstants( params );
                    ctx.Draw( 6, batch.mCount );
                }

                ctx.EndRender();
            } );
    }

    auto TextRenderModule::SetupTextRenderData( CommandContext& ctx, core::Blackboard& ) -> void {
        MKT_BEGIN_PROFILER_NAMED();
        mGlyphCount = mWorldGlyphCount = 0;

        mTextInfo.clear();
        mBatches.clear();

        // Keep scratch capacity across frames. Text components decode UTF-8
        // when edited; gathering here reads those existing code points.
        mItems.clear();
        mAtlasBindings.Clear();

        for ( auto& request: mGlyphRequests | std::views::values ) {
            request.mCharacters.clear();
            request.mUsed = false;
        }

        if ( !mScene ) {
            return;
        }

        const auto& registry = mScene->GetRegistry();
        const auto renderables{ registry.view<scene::TransformComponent, scene::TextComponent, scene::TagComponent>() };

        for ( const auto entity: renderables ) {
            const auto& text = renderables.get<scene::TextComponent>( entity );

            if ( !renderables.get<scene::TagComponent>( entity ).IsActive() || !text.HasFont() || text.GetRenderColor().a <= 0.0f ) {
                continue;
            }

            mItems.push_back( { &renderables.get<scene::TransformComponent>( entity ), &text } );
            auto& request{ mGlyphRequests[text.GetFontHandle().GetPtr()] };
            request.mUsed = true;

            for ( const auto point: text.GetUnicodeContents() ) {
                request.mCharacters.push_back( point.GetValue() );
            }

            // Do not let emoji shaping join the end of one entity to the next.
            request.mCharacters.push_back( '\n' );
        }

        // Remove stale font identities before dereferencing keys. Live fonts
        // retain character-buffer capacity between frames.
        std::erase_if( mGlyphRequests, []( const auto& entry ) {
            return !entry.second.mUsed;
        } );

        for ( auto& [font, request]: mGlyphRequests ) {
            asset::FontFactory::Get()->EnsureGlyphs( *font, { request.mCharacters.data(), request.mCharacters.size() } );
        }

        const auto dimensions = InferDimensions( mResolution );
        const auto screenProjection = text::ScreenTextProjection( dimensions.first, dimensions.second );

        const auto textureID = [&]( const renderer::rhi::TextureHandle& texture ) {
            return mAtlasBindings.Resolve( texture, [&]( const renderer::rhi::TextureHandle& page ) {
                return ctx.PushTexture_SRV( ctx.ImportTexture( page ) );
            } );
        };

        bool truncated{};
        // Screen text is composited after world text and ignores scene depth.
        for ( const bool world: { true, false } ) {
            for ( const auto& item: mItems ) {
                const auto& text{ *item.mText };

                if ( text.IsWorldText() != world ) {
                    continue;
                }

                const auto* camera{ text.GetCamera() ? text.GetCamera() : mCamera };

                if ( world && !camera ) {
                    continue;
                }

                const auto& font = *text.GetFont();
                const auto codepoints{ text.GetUnicodeContents().GetCodePoints() };
                text::LayoutTextInto( font, std::span<const text::CodePoint>{ codepoints.data(), codepoints.size() },
                    { text.GetSize(), text.GetSpacing(), text.GetLineSpacing(),
                        static_cast<text::Alignment>( text.GetAlignment() ), !world },
                    mLayout, kMaxGlyphs - mTextInfo.size() );

                const auto& layout{ mLayout };
                const auto model{ world ? item.mTransform->GetWorldTransform() : text::ScreenTextModel( text.GetScreenPlacement() ) };
                const auto projection{ world ? camera->GetProjection() : screenProjection };
                const auto view{ world ? camera->GetViewMatrix() : core::float4x4{ 1.0f } };

                truncated |= layout.mTruncated;

                for ( const auto& quad: layout.mGlyphs ) {
                    const auto& glyph = font.GetGlyph( quad.mCodepoint );
                    const auto atlas = font.GetAtlas( glyph.mAtlasPage );

                    if ( atlas.IsEmpty() ) {
                        continue;
                    }

                    const auto slugPage = font.GetSlugPage( glyph.mAtlasPage );
                    const bool slug = glyph.mImageType == GlyphImageType::eOutline && text.GetBackend() == scene::TextBackend::eSlug &&
                                      glyph.mSlugStrokeCount && !slugPage.curves.IsEmpty() && !slugPage.bands.IsEmpty() && !slugPage.strokeCurves.IsEmpty();
                    const auto& uv = glyph.mAtlasBounds;
                    const float left = uv.x / atlas->GetWidth();
                    const float right = uv.z / atlas->GetWidth();
                    const float bottom = uv.y / atlas->GetHeight();
                    const float top = uv.w / atlas->GetHeight();
                    TextDrawParameters p{
                        .mProjection = projection,
                        .mView = view,
                        .mModel = model,
                        .mPosition = { quad.mPosition, 0.0f, 1.0f },
                        .mSize = { quad.mSize, world ? 1.0f : -1.0f, text.GetSize() },
                        .mColor = text.GetRenderColor(),
                        .mTexCoords = { { left, world ? bottom : top },
                            { right, world ? bottom : top },
                            { right, world ? top : bottom },
                            { left, world ? top : bottom } },
                        .mTextureAtlasID = slug ? 0 : textureID( atlas ),
                        .mDistanceRange = font.GetDistanceRange(),
                        .mImageType = glyph.mImageType,
                        // Slug consumes an em-space radius; MSDF consumes normalized distance.
                        .mOutlineWidth = glyph.mImageType == GlyphImageType::eOutline ? ( slug ? text.GetOutlineWidth() : text.GetOutlineWidth() * core::as<core::f32>( font.GetSize() ) / font.GetDistanceRange() ) : 0.0f,
                        .mGlyphPlaneBounds = glyph.mPlaneBounds,
                        .mBandTransform = glyph.mSlugBandTransform,
                        .mGlyphBands = { glyph.mSlugGlyph[0], glyph.mSlugGlyph[1], glyph.mSlugGlyph[2], glyph.mSlugGlyph[3] },
                        .mViewport = { dimensions.first, dimensions.second },
                        .mCurveTextureID = slug ? textureID( slugPage.curves ) : 0,
                        .mBandTextureID = slug ? textureID( slugPage.bands ) : 0,
                        .mOutlineColor = { core::float3{ text.GetOutlineColor() }, text.GetOutlineColor().a * text.GetRenderColor().a },
                        .mSlugStroke = { slug && text.GetOutlineWidth() > 0 ? textureID( slugPage.strokeCurves ) : 0,
                            glyph.mSlugStrokeStart, glyph.mSlugStrokeCount, 0 }
                    };

                    if ( mBatches.empty() || mBatches.back().mIsWorld != world || mBatches.back().mIsSlug != slug ) {
                        mBatches.push_back( { core::as<core::u32>( mTextInfo.size() ), 0, world, slug } );
                    }

                    ++mBatches.back().mCount;
                    mTextInfo.push_back( p );
                }
            }

            if ( world ) {
                mWorldGlyphCount = mTextInfo.size();
            }
        }

        mGlyphCount = mTextInfo.size();

        if ( truncated && !mReportedCapacity ) {
            MKT_CORE_LOGGER_WARN( "Text exceeds the {} glyph render limit; excess glyphs were skipped.", kMaxGlyphs );
        }

        mReportedCapacity = truncated;
    }
}// namespace mikoto::renderer
