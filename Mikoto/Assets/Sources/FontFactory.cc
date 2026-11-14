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

#include <vector>
#include <algorithm>

#include <EASTL/sort.h>
#include <EASTL/algorithm.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Logging/Logger.hh>

#include <Assets/FontFactory.hh>

#include <Text/Utility.hh>
#include <Text/FontAtlas.hh>

namespace mikoto::asset {

    using namespace mikoto::core;
    using namespace mikoto::filesystem;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;

    auto FontFactory::UploadAtlas( const FontAtlasData& data ) -> renderer::rhi::TextureHandle {
        MKT_BEGIN_PROFILER();

        if ( data.pixels.empty() ) {
            return renderer::rhi::TextureHandle::CreateEmpty();
        }

        auto description{ renderer::rhi::TextureCreateDescription{}
            .SetWidth( data.width )
            .SetHeight( data.height )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetMultisampling( renderer::rhi::Multisampling::eMsaaX1 )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::CopyDest | renderer::rhi::TextureUsageFlagsBits::ShaderResource )
            .SetBufferData( memory::BufferSpanHandle::New( data.pixels.data(), data.pixels.size() ) )
            .SetFormat( renderer::rhi::Format::eRGBA8_UNORM ) };

        return mDevice->CreateTexture( description );
    }

    auto FontFactory::UploadSlugAtlas( const SlugHornAtlasData& data ) -> SlugTexturePage {
        MKT_BEGIN_PROFILER();

        if ( data.mCurves.mPixels.empty() || data.mBands.mPixels.empty() ) {
            return {};
        }

        const auto upload = [&]( const SlugHornTextureData& texture, rhi::Format format ) -> rhi::TextureHandle {
            if ( texture.mPixels.empty() ) {
                return rhi::TextureHandle::CreateEmpty();
            }
            // These are numeric tables, not color images: one mip, no sRGB,
            // and exact integer Load() reads in the SlugHorn shaders.
            return mDevice->CreateTexture( renderer::rhi::TextureCreateDescription{}
                .SetWidth( texture.mWidth )
                .SetHeight( texture.mHeight )
                .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
                .SetMultisampling( renderer::rhi::Multisampling::eMsaaX1 )
                .SetFormat( format )
                .SetUsage( renderer::rhi::TextureUsageFlagsBits::CopyDest |
                    renderer::rhi::TextureUsageFlagsBits::ShaderResource )
                .SetBufferData( memory::BufferSpanHandle::New( texture.mPixels.data(), texture.mPixels.size() ) ) );
        };
        return { upload( data.mCurves, rhi::Format::eRGBA32_FLOAT ),
            upload( data.mBands, rhi::Format::eRG16_UINT ),
            upload( data.mStrokeCurves, rhi::Format::eRGBA32_FLOAT ) };
    }

    FontFactory::FontFactory( const FontFactoryCreateInfo& options )
        : mDevice{ options.mDevice } {
    }

    auto FontFactory::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        mIsInitialized = true;
    }

    auto FontFactory::Shutdown() -> void {
        MKT_BEGIN_PROFILER();
        mIsInitialized = false;
    }

    auto FontFactory::LoadFont( const FontLoadDescription& description ) -> renderer::FontHandle {
        MKT_BEGIN_PROFILER();

        if ( description.mFile.IsEmpty() ) {
            return FontHandle::CreateEmpty();
        }

        eastl::vector<core::u32> base;

        for ( core::u32 cp = 32; cp < 127; ++cp ) {
            base.push_back( cp );
        }

        base.push_back( 0xFFFD );

        try {
            const auto path{ description.mFile->GetPath() };
            const bool colorFont{ text::HasColorGlyphs( path.GetC_Str() ) };
            auto data = text::GenerateFontAtlas( path.GetC_Str(), description.mSize, { base.data(), base.size() } );

            if ( data.pixels.empty() && colorFont ) {
                // Bitmap-only fonts have no outline atlas; their glyph pages are
                // requested lazily just like the fallback of an ordinary font.
                data.width = data.height = 1;
                data.pixels.resize( 4, 0 );
            }

            TextureHandle atlas{ UploadAtlas( data ) };

            if ( atlas.IsEmpty() ) {
                MKT_CORE_LOGGER_ERROR( "Could not generate font atlas: {}", description.mFile->GetPath().GetC_Str() );
                return FontHandle::CreateEmpty();
            }

            FontHandle font{ FontHandle::New( atlas, description.mSize, description.mFile->GetPath() ) };
            font->mColorFontPath = colorFont ? path : description.mColorFont.IsEmpty() ? filesystem::Path{ "Resources/Fonts/Emoji/NotoColorEmoji.ttf" }
                                                                                       : description.mColorFont;

            font->mSlugPages.push_back( UploadSlugAtlas( data.slug ) );
            font->mGlyphs = data.glyphs;
            font->mKerning = data.kerning;
            font->mRequestedGlyphs.insert( base.begin(), base.end() );
            font->mLineHeight = data.lineHeight;
            font->mAscender = data.ascender;
            font->mDistanceRange = data.distanceRange;
            font->mMaxHeight = data.lineHeight * description.mSize;

            for ( const auto& [cp, glyph]: data.glyphs ) {
                font->mMaxWidth = eastl::max( font->mMaxWidth, glyph.mWidth );
            }

            return font;
        } catch ( const std::exception& error ) {
            MKT_CORE_LOGGER_ERROR( "Font load failed for {}: {}", description.mFile->GetPath().GetC_Str(), error.what() );
            return FontHandle::CreateEmpty();
        }
    }

    auto FontFactory::EnsureColorGlyphs( renderer::Font& font, eastl::span<const core::u32> codepoints ) -> void {
        eastl::vector<text::ColorGlyphRequest> missing{};
        ankerl::unordered_dense::set<eastl::string> pending{};
        const auto enqueue{ [&]( eastl::span<const core::u32> sequence ) {
            const eastl::string key{ reinterpret_cast<const char*>( sequence.data() ), sequence.size_bytes() };

            if ( font.mColorGlyphs.contains( key ) || !pending.emplace( key ).second ) {
                return;
            }

            text::ColorGlyphRequest request{ .mGlyphKey = font.mNextColorGlyph++ };
            request.mSequence.assign( sequence.data(), sequence.data() + sequence.size() );
            missing.push_back( eastl::move( request ) );
        } };

        for ( core::usize i{}; i < codepoints.size(); ++i ) {
            const auto length{ text::EmojiSequenceLength( { codepoints.data() + i, codepoints.size() - i } ) };

            if ( !length ) {
                continue;
            }

            const auto sequence{ codepoints.subspan( i, length ) };
            enqueue( sequence );

            if ( length > 1 && sequence[1] != 0xFE0E ) {
                // Unsupported joined sequences can still display their base
                // emoji instead of a replacement box from the outline font.
                enqueue( sequence.first( 1 ) );
            }
        }

        for ( core::usize start{}; start < missing.size(); start += 64 ) {
            const eastl::span<const text::ColorGlyphRequest> batch{ missing.data() + start, eastl::min( core::usize{ 64 }, missing.size() - start ) };
            const auto data{ text::GenerateColorGlyphAtlas( font.mColorFontPath.GetC_Str(), core::as<core::f32>( font.mPixelSize ), { batch.data(), batch.size() } ) };
            const auto atlas{ UploadAtlas( data ) };

            if ( !data.glyphs.empty() && atlas.IsEmpty() ) {
                MKT_THROW_RUNTIME_ERROR( "Colour glyph atlas upload failed" );
            }

            const auto page{ core::as<core::u32>( font.mAtlasPages.size() ) };

            if ( !atlas.IsEmpty() ) {
                font.mAtlasPages.push_back( atlas );
                // Keep page indices shared between backends. Colour pages never
                // allocate curve/band textures and always take the RGBA path.
                font.mSlugPages.push_back( {} );

                for ( const auto& [key, source]: data.glyphs ) {
                    auto glyph{ source };
                    glyph.mAtlasPage = page;
                    font.mGlyphs.emplace( key, glyph );
                }
            }

            for ( const auto& request: batch ) {
                const eastl::string key{ reinterpret_cast<const char*>( request.mSequence.data() ), request.mSequence.size() * sizeof( core::u32 ) };
                // Remember unsupported sequences too: stationary text must not
                // reopen the font or invoke the shaper on every frame.
                font.mColorGlyphs.emplace( key, data.glyphs.contains( request.mGlyphKey ) ? request.mGlyphKey : 0 );
            }
        }
    }

    auto FontFactory::EnsureGlyphs( renderer::Font& font, eastl::span<const core::u32> codepoints ) -> void {
        MKT_BEGIN_PROFILER();

        std::lock_guard lock{ mGlyphMutex };

        EnsureColorGlyphs( font, codepoints );

        eastl::vector<core::u32> missing{};

        for ( auto cp: codepoints ) {
            if ( cp >= 32 && cp <= 0x10FFFF && !( cp >= 0xD800 && cp <= 0xDFFF ) &&
                 cp != 0x85 && cp != 0x2028 && cp != 0x2029 && !font.mRequestedGlyphs.contains( cp ) ) {
                missing.push_back( cp );
            }
        }

        eastl::sort( missing.begin(), missing.end() );
        missing.erase( eastl::unique( missing.begin(), missing.end() ), missing.end() );
        // Small immutable pages avoid repacking or replacing glyphs already in flight.
        for ( usize start{}; start < missing.size(); start += 256 ) {
            const auto batch{ eastl::span<const core::u32>{ missing.data(), missing.size() }
                .subspan( start, eastl::min( size_t{ 256 }, missing.size() - start ) ) };
            eastl::vector<core::u32> previous{};

            for ( const auto& [cp, glyph]: font.mGlyphs ) {
                if ( glyph.mImageType == GlyphImageType::eOutline ) {
                    previous.push_back( cp );
                }
            }

            try {
                const auto data = text::GenerateFontAtlas( font.mPath.GetC_Str(),
                    as<float>( font.mPixelSize ), { batch.data(), batch.size() }, { previous.data(), previous.size() } );
                auto atlas = UploadAtlas( data );

                if ( !data.glyphs.empty() && atlas.IsEmpty() ) {
                    MKT_THROW_RUNTIME_ERROR( "Font atlas upload failed" );
                }

                if ( !atlas.IsEmpty() ) {
                    const auto page = static_cast<core::u32>( font.mAtlasPages.size() );
                    const auto slug = UploadSlugAtlas( data.slug );

                    font.mAtlasPages.push_back( atlas );
                    font.mSlugPages.push_back( slug );

                    for ( const auto& [cp, source]: data.glyphs ) {
                        auto glyph = source;
                        glyph.mAtlasPage = page;
                        font.mGlyphs.emplace( cp, glyph );
                    }

                    font.mKerning.insert( data.kerning.begin(), data.kerning.end() );
                }

                // Missing glyphs in the font are remembered and rendered with a replacement.
                font.mRequestedGlyphs.insert( batch.begin(), batch.end() );
            } catch ( const std::exception& error ) {
                MKT_CORE_LOGGER_ERROR( "Font glyph generation failed for {}: {}", font.mPath.GetC_Str(), error.what() );
                return;
            }
        }
    }
}// namespace mikoto::asset
