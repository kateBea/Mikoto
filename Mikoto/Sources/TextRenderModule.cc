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
#include <Core/Blackboard.hh>

#include <Scene/Scene.hh>
#include <Scene/Camera.hh>
#include <Scene/Component.hh>

#include <Text/Unicode.hh>

#include <Renderer/Text/Font.hh>

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/CommandContext.hh>

#include <Renderer/Passes/PrepassModule.hh>
#include <Renderer/Passes/TextRenderModule.hh>
#include <Renderer/Passes/GeometryShadingModule.hh>

namespace mikoto::renderer {

    using namespace mikoto::scene;

    TextRenderModule::TextRenderModule( RenderResolution resolution )
        : mResolution{ resolution }
    {}

    auto TextRenderModule::SetScene( const Scene *scene ) -> void {
        mScene = scene;
    }

    auto TextRenderModule::SetCamera( const Camera *camera ) -> void {
        mCamera = camera;
    }

    auto TextRenderModule::RegisterPasses( FrameGraph &graph) -> void {
        RegisterSlugPass( graph );
        RegisterTextRender( graph );
    }

    auto TextRenderModule::RegisterSlugPass( FrameGraph &graph) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
            "SlugTextRendering",
            FGPassType::eGraphics,
            []( FGNodeBuilder&, Blackboard& ) -> void {
            },
            []( CommandContext&, Blackboard& ) -> void {
            } );
    }

    auto TextRenderModule::RegisterTextRender( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        TextRenderingPassParameters& info{ graph.GetOrCreate<TextRenderingPassParameters>() };

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "MSDFText_Pipeline" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .SetDepthFormat( Format::eD32 )
            .AddColorFormat( Format::eRGBA8_UNORM )
            .PushShader( "MSDFText_Vert.slang", FGStageType::eVertex )
            .PushShader( "MSDFText_Frag.slang", FGStageType::ePixel ) };

        info.mMsdfPipeline = graph.Create( pipelineBuilder );

        auto textDataBufferDesc{ FGBufferDescription{}
            .SetName( "MSDFText_RenderData" )
            .SetUsage( BufferUsageFlagsBits::kStorage | BufferUsageFlagsBits::kCopyDst )
            .SetSizeBytes( kMaxGlyphs * MKT_SIZEOF( TextDrawParameters ) )
            .SetHeapType( HeapType::eDeviceLocal )};

        info.mMsdfTextRenderData = graph.Create( textDataBufferDesc );

        graph.RegisterPass<TextRenderingPassParameters>(
            "MSDFText_Upload",
            FGPassType::eTransfer,
            []( FGNodeBuilder& b, TextRenderingPassParameters& info ) -> void {
                b.Write( info.mMsdfTextRenderData, FGPipelineStage::eCopy );
            },
            [this]( CommandContext& ctx, Blackboard& b ) -> void {
                SetupTextRenderData( ctx, b );

                if (mGlyphCount != 0) {
                    TextRenderingPassParameters& textInfo{ b.Get<TextRenderingPassParameters>() };
                    ctx.CopyBuffer( textInfo.mMsdfTextRenderData,0,  mTextInfo.data(), sizeof( TextDrawParameters ) * mGlyphCount );
                }
            } );

        graph.RegisterPass(
            "MSDFTextPass_Render",
            FGPassType::eGraphics,
            []( FGNodeBuilder& builder, Blackboard& blackboard ) {
                PrepassModuleInfo& prepass{ blackboard.Get<PrepassModuleInfo>() };
                GeomShadingModuleInfo& finalImageInfo{ blackboard.Get<GeomShadingModuleInfo>() };
                TextRenderingPassParameters& textInfo{ blackboard.Get<TextRenderingPassParameters>() };

                builder.Read( textInfo.mMsdfTextRenderData, FGPipelineStage::ePixelShader );

                builder.Write( finalImageInfo.mColorImage, FGPipelineStage::eRenderTarget );
                builder.Write( prepass.mPrepassDepthTarget, FGPipelineStage::eDepthTarget );
            },
        [this]( CommandContext& ctx, Blackboard& blackboard ) -> void {
            if (mGlyphCount == 0) {
                return;
            }

            PrepassModuleInfo& prepass{ blackboard.Get<PrepassModuleInfo>() };
            GeomShadingModuleInfo& finalImageInfo{ blackboard.Get<GeomShadingModuleInfo>() };
            TextRenderingPassParameters& textInfo{ blackboard.Get<TextRenderingPassParameters>() };

            struct DrawParams {
                u32 mTextSamplerID{};
                u32 mTextParametersID{};
            } params{
                .mTextSamplerID = ctx.PushSampler( finalImageInfo.mDefaultSampler ),
                .mTextParametersID = ctx.PushBuffer_SRV( textInfo.mMsdfTextRenderData ),
            };

            ctx.PushConstants( params );

            const auto dimensions{ InferDimensions( mResolution ) };

            const auto graphicsState{ ContextRenderState{}
                .SetRenderArea( Rect{ as<i32>( dimensions.first ), as<i32>( dimensions.second ) } )
                .AddDepthTarget( prepass.mPrepassDepthTarget, LoadOp::eLoad )
                .AddRenderTarget( finalImageInfo.mColorImage, kColorBlack, LoadOp::eLoad ) };
            ctx.BeginRender( graphicsState );

            ctx.SetViewportState( ViewportState{}
                .AddViewportAndScissorRect( Viewport( as<u32>( dimensions.first ), as<u32>( dimensions.second ) ) ) );

            ctx.BindPipeline( textInfo.mMsdfPipeline );

            ctx.Draw( 6 );

            ctx.EndRender();
        } );
    }

    auto TextRenderModule::SetupTextRenderData( CommandContext& ctx, Blackboard& ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto& registry{ mScene->GetRegistry() };
        auto renderables{ registry.view<TransformComponent, TextComponent>() };

        mGlyphCount = 0;

        for ( auto& entity: renderables ) {
            auto& textComponent{ registry.get<TextComponent>( entity ) };
            if ( !textComponent.HasFont() ) {
                continue;
            }

            auto& textComp{ registry.get<TextComponent>( entity ) };
            auto& transformComp{ registry.get<TransformComponent>( entity ) };

            FontHandle font{ textComp.GetFontHandle() };

            const Camera* camera{ textComponent.GetCamera() };

            const f32 fontSize{ textComponent.GetSize() };
            const float4& color{ textComponent.GetColor() };
            const float4& position{ transformComp.GetTranslation(), 1.0f };

            f64 xPos{ position.x };
            f64 yPos{ position.y };
            f64 scale{ fontSize / font->GetSize() };

            double lineHeight{ font->GetMaxHeight() * scale };

            auto decoded{ text::GetUnicodeFromUtf8( textComponent.GetContents() ) };

            for ( const auto& character: decoded ) {
                const bool isLineFeed{ character == as<u32>( U'\n' ) ||
                    string::IsLineFeed( character ) };

                if ( isLineFeed ) {
                    xPos = position.x;
                    yPos += lineHeight;
                    continue;
                }

                f64 advance{ 0 };

                if ( font->HasGlyph( character ) ) {
                    const FontGlyph& glyph{ font->GetGlyph( as<u32>( character ) ) };

                    if ( !string::IsSpace( character ) ) {
                        // Quad Coordinates
                        f64 x0{ xPos + glyph.mPlaneBounds.x * fontSize };
                        f64 y0{ yPos - glyph.mPlaneBounds.y * fontSize };

                        // UV Coordinates
                        TextureHandle atlas{ font->GetAtlas() };
                        f64 s0{ glyph.mAtlasBounds.x / as<f32>( atlas->GetWidth() ) };
                        f64 t0{ glyph.mAtlasBounds.w / as<f32>( atlas->GetHeight() ) };
                        f64 s1{ glyph.mAtlasBounds.z / as<f32>( atlas->GetWidth() ) };
                        f64 t1{ glyph.mAtlasBounds.y / as<f32>( atlas->GetHeight() ) };

                        // Imported textures need to be synchronized externally
                        // Before the frame graph runs the client needs to make sure the resource
                        // is in the specific state the resource will be used in.
                        FGTextureHandle importedAtlas{ ctx.ImportTexture( atlas ) };

                        TextDrawParameters fontParams{
                            .mModel{ textComponent.IsWorldText() ? transformComp.GetTransform() : float4x4{ 1.0f } },
                            .mPosition{ x0, y0 + std::round( ( font->GetMaxHeight() * scale ) ) - ( glyph.mHeight * scale ), position.z, position.w },
                            .mSize{ glyph.mWidth * scale, glyph.mHeight * scale, 0.0f, 0.0f },
                            .mColor{ color },
                            .mTexCoords{ { s0, t0 }, { s1, t0 }, { s1, t1 }, { s0, t1 } },
                            .mTextureAtlasID = as<u32>( ctx.PushTexture_SRV( importedAtlas ) ),
                        };

                        float4x4 view{};
                        float4x4 projection{};

                        if ( !textComponent.IsWorldText() ) {
                            if ( camera ) {
                                view = camera->GetProjection();
                                projection = camera->GetViewMatrix();
                            } else {
                                const auto dimension{ InferDimensions( mResolution ) };
                                projection = glm::ortho( 0.0f, dimension.first, dimension.second, 0.0f, -1.0f, 1.0f );
                                view = glm::mat4{ 1.0f };
                            }
                        } else {
                            view = mCamera->GetProjection();
                            projection = mCamera->GetViewMatrix();
                        }

                        fontParams.mProjection = view;
                        fontParams.mView = projection;

                        MKT_ASSERT( mGlyphCount <= kMaxGlyphs, "Exceeded max glyphs" );

                        if (mGlyphCount > mTextInfo.size()) {
                            mTextInfo.emplace_back( fontParams );
                        } else {
                            mTextInfo[mGlyphCount++] = fontParams;
                        }
                    }

                    advance = glyph.mAdvanceX * fontSize;
                } else {
                    // If the character does not exist just insert space
                    // equal to Space character.
                    const FontGlyph& glyph{ font->GetGlyph( as<u32>( ' ' ) ) };
                    advance = glyph.mAdvanceX * fontSize;
                }

                xPos += advance;
            }
        }
    }
}// namespace Mikoto