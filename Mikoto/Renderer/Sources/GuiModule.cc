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

#include <EASTL/sort.h>
#include <EASTL/algorithm.h>

#include <ankerl/unordered_dense.h>

#include <Logging/Logger.hh>

#include <Assets/FontFactory.hh>
#include <Assets/AssetRegistryService.hh>

#include <Text/Utility.hh>
#include <Text/TextLayout.hh>

#include <Renderer/ForwardRenderer/Passes/GuiModule.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/ForwardRenderer/Passes/PrepassModule.hh>
#include <Renderer/ForwardRenderer/Passes/PresentationModule.hh>

#include <Scene/Scene.hh>
namespace mikoto::renderer {

    using namespace core;
    using namespace rhi;
    using namespace scene;

    GuiModule::GuiModule( renderer::rhi::RenderResolution resolution )
        : mResolution{ resolution } {
    }

    auto GuiModule::SetScene( const scene::Scene* scene ) -> void {
        mScene = scene;
    }

    auto GuiModule::SetCamera( const scene::Camera* camera ) -> void {
        mCamera = camera;
    }

    auto GuiModule::SetGeometryManager( GeometryCullModule& geom ) -> void {
        mGeometryCullModule = &geom;
    }

    auto GuiModule::RegisterUploadPass( FrameGraph& graph ) -> void {
        mBuffer = graph.Create( FGBufferDescription{}
            .SetName( "Canvas_Quads" )
            .SetSizeBytes( kMaxQuads * sizeof( Quad ) )
            .SetHeapType( renderer::rhi::HeapType::eDeviceLocal )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured | renderer::rhi::BufferUsageFlagsBits::CopyDest ) );

        // Build one ordered stream before drawing; the frame graph orders its
        // transfer write before either canvas pipeline reads the buffer.
        graph.RegisterPass(
            "Canvas_Upload",
            FGPassType::eTransfer,
            [this]( FGNodeBuilder& builder, core::Blackboard& ) {
                builder.UseResource( mBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& ctx, core::Blackboard& ) {
                Upload( ctx );
            } );
    }

    auto GuiModule::RegisterRenderPass( FrameGraph& graph ) -> void {
        mSampler = graph.Create( FGSamplerDescription{}
            .SetName( "Canvas_Sampler" )
            .SetFilter( renderer::rhi::SamplerFilter::eLinear )
            .SetWrap( renderer::rhi::SamplerWrapMode::eClampToEdge ) );

        auto pipeline = FGPipelineDescription{}
            .SetName( "Canvas_World" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetCullMode( renderer::rhi::CullMode::eNone )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .SetBlendEnable( true )
            .SetDepthTest( true )
            .SetDepthWrite( false )
            .SetDepthFormat( renderer::rhi::Format::eD32 )
            .AddColorFormat( renderer::rhi::Format::eBGRA8_UNORM )
            .PushShader( "Canvas_Vert.slang", FGStageType::eVertex )
            .PushShader( "Canvas_Frag.slang", FGStageType::ePixel );

        const auto worldPipeline = graph.Create( pipeline );
        const auto overlayPipeline = graph.Create( pipeline
            .SetName( "Canvas_Overlay" )
            .SetDepthTest( false ) );

        graph.RegisterPass(
            "Canvas_Render",
            FGPassType::eGraphics,
            [this]( FGNodeBuilder& builder, core::Blackboard& b ) {
                builder.UseResource( mBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( mBuffer, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );

                builder.UseResource( b.Get<PresentationPassData>().mColorImage, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                builder.UseResource( b.Get<PrepassModuleInfo>().mPrepassDepthTarget, FGPipelineStage::eDepthTarget, FGResourceAccess::eRead );
            },
            [this, worldPipeline, overlayPipeline]( CommandContext& ctx, core::Blackboard& b ) {
                if ( mQuads.empty() ) {
                    return;
                }

                const auto [width, height]{ InferDimensions( mResolution ) };

                ctx.BeginRender( RenderBeginDescription{}
                    .SetRenderArea( renderer::rhi::Rect{ core::as<core::i32>( width ), core::as<core::i32>( height ) } )
                    .AddRenderTarget( b.Get<PresentationPassData>().mColorImage, renderer::rhi::Color{}, renderer::rhi::LoadOp::eLoad )
                    .AddDepthTarget( b.Get<PrepassModuleInfo>().mPrepassDepthTarget, renderer::rhi::LoadOp::eLoad ) );

                ctx.SetViewportState( renderer::rhi::ViewportState{}
                    .AddViewportAndScissorRect( renderer::rhi::Viewport( core::as<float>( width ), core::as<float>( height ) ) ) );

                struct Parameters {
                    SPointer mQuads;
                    core::u32 mSampler;
                    core::u32 mOffset;
                } params{
                    ctx.GetDeviceBufferAddress( mBuffer ),
                    ctx.PushSampler( mSampler ),
                    0
                };

                if ( mWorldQuads ) {
                    ctx.BindPipeline( worldPipeline );
                    ctx.PushConstants( params );
                    ctx.Draw( 6, mWorldQuads );
                }

                if ( mQuads.size() > mWorldQuads ) {
                    params.mOffset = mWorldQuads;

                    ctx.BindPipeline( overlayPipeline );
                    ctx.PushConstants( params );
                    ctx.Draw( 6, core::as<core::u32>( mQuads.size() ) - mWorldQuads );
                }

                ctx.EndRender();
            } );
    }

    auto GuiModule::Upload( CommandContext& ctx ) -> void {
        mQuads.clear();
        mWorldQuads = 0;

        if ( !mScene ) {
            return;
        }

        struct Item {
            const Canvas* mCanvas{};
            const scene::TransformComponent* mTransform{};
            core::u64 mId{};
        };

        eastl::vector<Item> items{};

        for ( const auto [entity, component, transform, tag]:
            mScene->GetRegistry().view<scene::CanvasComponent, scene::TransformComponent, scene::TagComponent>().each() ) {
            if ( tag.IsActive() ) {
                items.push_back( { &component.GetCanvas(), &transform, tag.GetGuid() } );
            }
        }

        eastl::sort( items.begin(), items.end(),
            []( const Item& a, const Item& b ) {
            if ( a.mCanvas->IsOverlay() != b.mCanvas->IsOverlay() ) {
                return !a.mCanvas->IsOverlay();
            }

            if ( a.mCanvas->GetSortOrder() != b.mCanvas->GetSortOrder() ) {
                return a.mCanvas->GetSortOrder() < b.mCanvas->GetSortOrder();
            }

            return a.mId < b.mId;
        } );

        bool truncated{};
        ankerl::unordered_dense::map<const renderer::rhi::ITexture*, core::u32> textures{};

        const auto textureID = [&]( renderer::rhi::TextureHandle texture ) {
            const auto [entry, inserted] = textures.try_emplace( texture.GetPtr(), 0 );

            if ( inserted ) {
                entry->second = ctx.PushTexture_SRV( ctx.ImportTexture( texture ) );
            }

            return entry->second;
        };

        // Render targets can have a fixed resolution and be displayed in a
        // differently shaped viewport. Use the displayed camera dimensions
        // so the final image keeps the same UI scale on both axes.
        const auto [targetWidth, targetHeight]{ InferDimensions( mResolution ) };
        core::float2 viewportSize{ targetWidth, targetHeight };

        if ( mCamera ) {
            const auto [width, height]{ mCamera->GetViewPort() };
            viewportSize = { width, height };
        }

        for ( const auto& item: items ) {
            const auto& canvas = *item.mCanvas;
            const auto rootSize{ canvas.GetLayoutSize( viewportSize ) };
            const auto* camera = canvas.GetCamera() ? canvas.GetCamera() : mCamera;

            if ( !canvas.IsOverlay() && !camera ) {
                continue;
            }

            const auto projection = canvas.IsOverlay()
                                        ? text::ScreenTextProjection( rootSize.x, rootSize.y )
                                        : camera->GetViewProjection() * item.mTransform->GetWorldTransform() *
                                              glm::scale( core::float4x4{ 1 }, core::float3{ canvas.GetWorldScale(), -canvas.GetWorldScale(), 1 } );

            const auto commands{ canvas.GetCommands() };
            // Drawing and input share the same hierarchy and transforms. Child
            // anchors are relative to their parent, not the full viewport.
            for ( const auto& element: canvas.GetResolvedElements( rootSize ) ) {
                const auto& command{ commands[element.mCommandIndex] };

                if ( !element.mVisible ) {
                    continue;
                }

                const auto rect{ element.mRect };
                const auto transform{ projection * element.mLocalToCanvas };

                const auto emit = [&]( CanvasRect r, core::float4 color, core::u32 kind,
                                      renderer::rhi::TextureHandle texture = {}, core::float4 uv = { 0, 0, 1, 1 },
                                      float range = 0, float rounding = 0, float stroke = 0 ) {
                    if ( mQuads.size() >= kMaxQuads ) {
                        truncated = true;
                        return;
                    }

                    if ( r.mSize.x <= 0 || r.mSize.y <= 0 ) {
                        return;
                    }

                    Quad q{};

                    const core::float2 points[4]{
                        r.mPosition,
                        r.mPosition + core::float2{ r.mSize.x, 0 },
                        r.mPosition + r.mSize,
                        r.mPosition + core::float2{ 0, r.mSize.y }
                    };

                    for ( i32 i{}; i < 4; ++i ) {
                        q.mPositions[i] = transform * core::float4{ points[i], 0, 1 };
                    }

                    q.mColor = color;

                    if ( !element.mEnabled ) {
                        q.mColor.a *= 0.45f;
                    }

                    q.mUV = uv;
                    q.mSize = r.mSize;
                    q.mKind = kind;
                    q.mTexture = texture.IsEmpty() ? 0 : textureID( texture );
                    q.mDistanceRange = range;
                    q.mRounding = rounding;
                    q.mStroke = stroke;

                    mQuads.push_back( q );
                };

                core::float4 color{ command.mColor.mR, command.mColor.mG, command.mColor.mB, command.mColor.mA };
                // Feedback changes RGB only: a translucent widget keeps its
                // authored coverage while responding to pointer interaction.
                if ( element.mEnabled && command.mInteractable ) {
                    if ( command.mPressed ) {
                        color = core::float4{ core::float3{ color } * 0.75f, color.a };
                    }
                    else if ( command.mHovered ) {
                        color = core::float4{ glm::mix( core::float3{ color }, core::float3{ 1 }, 0.18f ), color.a };
                    }
                }

                switch ( command.mType ) {
                    case CanvasCommandType::eQuad:
                        emit( rect, color, 0, {}, { 0, 0, 1, 1 }, 0, command.mRounding );
                        break;

                    case CanvasCommandType::eRect:
                        emit( rect, color, 0, {}, { 0, 0, 1, 1 }, 0, command.mRounding, command.mThickness );
                        break;

                    case CanvasCommandType::eSprite:
                        if ( const auto texture{ command.GetTexture() } ) {
                            emit( rect, color, 1, texture, { 0, 0, 1, 1 }, 0, command.mRounding );
                        }

                        break;

                    case CanvasCommandType::eLine: {
                        const auto delta = command.mEnd - command.mStart;
                        const float length = glm::length( delta );

                        if ( mQuads.size() >= kMaxQuads ) {
                            truncated = true;
                            break;
                        }

                        if ( length <= 0 || command.mThickness <= 0 ) {
                            break;
                        }

                        const auto normal = core::float2{ -delta.y, delta.x } / length * command.mThickness * 0.5f;
                        Quad q{};

                        const core::float2 points[]{
                            command.mStart - normal,
                            command.mEnd - normal,
                            command.mEnd + normal,
                            command.mStart + normal
                        };

                        for ( int i = 0; i < 4; ++i ) {
                            q.mPositions[i] = transform * core::float4{ points[i], 0, 1 };
                        }

                        q.mColor = color;

                        if ( !element.mEnabled ) {
                            q.mColor.a *= 0.45f;
                        }

                        q.mSize = { length, command.mThickness };

                        mQuads.push_back( q );
                        break;
                    }

                    case CanvasCommandType::eButton:
                        emit( rect, color, 0, {}, { 0, 0, 1, 1 }, 0, command.mRounding );
                        break;

                    case CanvasCommandType::eToggle:
                        emit( rect,
                            command.mValue > 0.5f ? color : core::float4{ 0.18f, 0.18f, 0.18f, color.a },
                            0, {}, { 0, 0, 1, 1 }, 0, command.mRounding );
                        break;

                    case CanvasCommandType::eSlider: {
                        emit( rect, core::float4{ 0.18f, 0.18f, 0.18f, color.a }, 0 );
                        emit( { rect.mPosition, { rect.mSize.x * glm::clamp( command.mValue, 0.0f, 1.0f ), rect.mSize.y } }, color, 0 );
                        const core::f32 knobWidth{ glm::min( 8.0f, rect.mSize.x ) };
                        const core::f32 knobPosition{ glm::clamp( command.mValue, 0.0f, 1.0f ) * glm::max( 0.0f, rect.mSize.x - knobWidth ) };

                        // Soften the handle corners while keeping the existing drag area and travel.
                        const core::f32 knobRounding{ glm::min( 3.0f, glm::min( knobWidth, rect.mSize.y ) * 0.5f ) };
                        emit( { { knobPosition, 0 }, { knobWidth, rect.mSize.y } },
                            core::float4{ 1, 1, 1, color.a }, 0, {}, { 0, 0, 1, 1 }, 0, knobRounding );
                        break;
                    }

                    default:
                        break;
                }

                // Mouse press/hover feedback does not need a persistent focus ring.
                if ( canvas.HasNavigationFocus() && command.mFocused && element.mEnabled && command.mInteractable ) {
                    emit( rect, core::float4{ 1, 0.8f, 0.25f, 1 }, 0, {}, { 0, 0, 1, 1 }, 0, command.mRounding, 2 );
                }

                if ( command.mText.empty() || ( command.mType != CanvasCommandType::eText && command.mType < CanvasCommandType::eButton ) ) {
                    continue;
                }

                auto font = command.mFont;

                if ( font.IsEmpty() ) {
                    if ( mDefaultFont.IsEmpty() ) {
                        mDefaultFont = asset::AssetRegistryService::Get()->LoadAsset<Font>( filesystem::Path{ "Resources/Fonts/Open_Sans/static/OpenSans-Regular.ttf" } );
                    }

                    font = mDefaultFont;
                }

                if ( font.IsEmpty() ) {
                    continue;
                }

                eastl::vector<core::u32> points;
                text::ForEachUtf8CodePoint( command.mText, [&]( core::u32 point ) {
                    points.push_back( point );
                } );

                const std::span<const core::u32> codepoints{ points.data(), points.size() };
                asset::FontFactory::Get()->EnsureGlyphs( *font, { codepoints.data(), codepoints.size() } );

                const auto layout = text::LayoutText(
                    *font, codepoints,
                    { command.mFontSize, 0, 1, text::Alignment::eLeft, true },
                    kMaxQuads - mQuads.size() );
                truncated |= layout.mTruncated;

                const auto origin = command.mType == CanvasCommandType::eText
                                        ? rect.mPosition
                                        : rect.mPosition + ( rect.mSize - ( layout.mMaximum - layout.mMinimum ) ) * 0.5f - layout.mMinimum;

                for ( const auto& glyphQuad: layout.mGlyphs ) {
                    const auto& glyph = font->GetGlyph( glyphQuad.mCodepoint );
                    const auto atlas = font->GetAtlas( glyph.mAtlasPage );

                    if ( atlas.IsEmpty() ) {
                        continue;
                    }

                    const auto bounds = glyph.mAtlasBounds;
                    const core::float4 uv{
                        bounds.x / atlas->GetWidth(), bounds.w / atlas->GetHeight(),
                        bounds.z / atlas->GetWidth(), bounds.y / atlas->GetHeight()
                    };

                    emit( { origin + glyphQuad.mPosition, glyphQuad.mSize },
                        command.mType == CanvasCommandType::eText ? color : core::float4{ 1, 1, 1, color.a },
                        glyph.mImageType == GlyphImageType::eColor ? 3 : 2, atlas, uv, font->GetDistanceRange() );
                }
            }

            if ( !canvas.IsOverlay() ) {
                mWorldQuads = core::as<core::u32>( mQuads.size() );
            }
        }

        if ( truncated && !mReportedCapacity ) {
            MKT_CORE_LOGGER_WARN( "Canvas exceeded {} quads; excess elements were skipped.", kMaxQuads );
        }

        mReportedCapacity = truncated;

        if ( !mQuads.empty() ) {
            ctx.CopyBuffer( mBuffer, 0, mQuads.data(), mQuads.size() * sizeof( Quad ) );
        }
    }
}// namespace mikoto::renderer
