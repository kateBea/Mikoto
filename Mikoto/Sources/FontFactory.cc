//
// Created by zanet on 3/15/2025.
//

#include <vector>

#include <fmt/format.h>

// I love Windows.h defining min and max macros that break everything

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#include <msdf-atlas-gen/glyph-generators.h>
#include <msdf-atlas-gen/msdf-atlas-gen.h>

#include <Renderer/Core/FontFactory.hh>

namespace Mikoto {
    auto FontFactory::GenerateAtlas( const CStr fontFilename, Int32 fontSize, bool expensiveColoring ) -> MsdfData {
        MsdfData result{};

        Int32 fontWeight{};
        Int32 isVariableWeight{};

        // Initialize instance of FreeType library
        if ( m_FreeTypeHandle != nullptr ) {
            msdfgen::FontHandle *font{ msdfgen::loadFont( m_FreeTypeHandle, fontFilename ) };

            if ( font != nullptr ) {
                // Storage for glyph geometry and their coordinates in the atlas
                MsdfGlyphGeometryList glyphs{};

                std::vector<msdfgen::FontVariationAxis> axes;
                msdfgen::listFontVariationAxes(axes, m_FreeTypeHandle, font);

                if (!axes.empty()) {
                    isVariableWeight = msdfgen::setFontVariationAxis(m_FreeTypeHandle, font, "Weight", fontWeight);
                }
                // else {
                //     fontWeight = getWeightFromString(m_MetaData.subFamily);
                // }

                // FontGeometry is a helper class that loads a set of glyphs from a single font.
                // It can also be used to get additional font metrics, kerning information, etc.
                msdf_atlas::FontGeometry fontGeometry( std::addressof( glyphs ) );

                // Specify a set of character glyphs
                std::array charsetRanges{
                    CharsetRange{ 0x0020, 0x007F },// Basic Latin
                    CharsetRange{ 0x3040, 0x309F },// Hiragana
                    CharsetRange{ 0x30A0, 0x30FF } // Katakana
                    //CharsetRange{ 0x00A0, 0x00FF },// Latin-1 Supplementº
                    //CharsetRange{ 0x0100, 0x017F },// Latin Extended-A
                    //CharsetRange{ 0x0180, 0x024F },// Latin Extended-B
                    //CharsetRange{ 0x0370, 0x03FF },// Greek & Coptic
                    //CharsetRange{ 0x0400, 0x04FF } // Cyrillic
                };

                msdf_atlas::Charset charset{};
                for ( auto [Start, End]: charsetRanges ) {
                    for ( Int32 c{ Start }; c <= End; c++ ) {
                        charset.add( c );
                    }
                }

                // The second argument can be ignored unless you mix different font sizes in one atlas.
                // In the last argument, you can specify a charset other than ASCII.
                // To load specific glyph indices, use loadGlyphs instead.
                fontGeometry.loadCharset( font, 1.0, charset );

                // Apply MSDF edge coloring. See edge-coloring.h for other coloring strategies.
                unsigned long long glyphSeed{ 0 };
                constexpr unsigned long long LCG_MULTIPLIER{ 6364136223846793005ull };

                if (expensiveColoring ) {
                    constexpr UInt64 coloringSeed{ 0 };
                    constexpr unsigned long long LCG_INCREMENT{ 1442695040888963407ull };

                    msdf_atlas::Workload([&glyphs = glyphs, &coloringSeed, &LCG_MULTIPLIER, &LCG_INCREMENT, &glyphSeed](int i, int threadNo) -> bool {
                        glyphSeed = (LCG_MULTIPLIER * (coloringSeed ^ i) + LCG_INCREMENT) * !!coloringSeed;
                        glyphs[i].edgeColoring(msdfgen::edgeColoringInkTrap, 3.0, glyphSeed);
                        return true;
                    }, glyphs.size()).finish(8);
                }
                else {
                    constexpr double maxCornerAngle{ 3.0 };

                    for (msdf_atlas::GlyphGeometry& glyph : glyphs) {
                        glyphSeed *= LCG_MULTIPLIER;
                        glyph.edgeColoring(msdfgen::edgeColoringInkTrap, maxCornerAngle, glyphSeed);
                    }
                }

                // TightAtlasPacker class computes the layout of the atlas.
                msdf_atlas::TightAtlasPacker packer{};

                // Set atlas parameters:
                // setDimensions or setDimensionsConstraint to find the best value
                packer.setDimensionsConstraint( msdf_atlas::DimensionsConstraint::SQUARE );

                // setScale for a fixed size or setMinimumScale to use the largest that fits
                packer.setScale( fontSize );

                // setPixelRange or setUnitRange
                packer.setPixelRange( 2.0 );
                packer.setMiterLimit( 1.0 );

                // Compute atlas layout - pack glyphs
                packer.pack( glyphs.data(), static_cast<Int32>( glyphs.size() ) );

                // Get final atlas dimensions
                Int32 width{};
                Int32 height{};
                packer.getDimensions( width, height );

                // The ImmediateAtlasGenerator class facilitates the generation of the atlas bitmap.
                MTSDFGen generator{ width, height };

                // GeneratorAttributes can be modified to change the generator's default settings.
                msdf_atlas::GeneratorAttributes attributes{
                    .config{ msdfgen::MSDFGeneratorConfig{ true } },
                    .scanlinePass{ true }
                };
                generator.setAttributes( attributes );
                generator.setThreadCount( 8 );

                // Generate atlas bitmap
                generator.generate( glyphs.data(), static_cast<Int32>( glyphs.size() ) );

                SubmitAtlasBitmapAndLayout(generator.atlasStorage(), glyphs, result, fontSize);

                // Cleanup
                msdfgen::destroyFont( font );
            }
        }

        return result;
    }

    auto FontFactory::SubmitAtlasBitmapAndLayout( const msdf_atlas::BitmapAtlasStorage<msdf_atlas::byte, 4> &atlas, std::vector<msdf_atlas::GlyphGeometry> glyphs,  MsdfData& data, Int32 fontSize ) const -> void {
        msdfgen::BitmapConstRef bitmap{ (msdfgen::BitmapConstRef<unsigned char, 4>)atlas };

        Int32 atlasWidth{ bitmap.width };
        Int32 atlasHeight{ bitmap.height };

        uint32_t code = 0;

        for (const auto& g : glyphs) {
            FontGlyph glyphData{};

            double pLeft, pBottom, pRight, pTop;
            double aLeft, aBottom, aRight, aTop;
            g.getQuadPlaneBounds(pLeft, pBottom, pRight, pTop);
            g.getQuadAtlasBounds(aLeft, aBottom, aRight, aTop);

            glyphData.m_AtlasBounds = { aLeft, aBottom, aRight, aTop };
            glyphData.m_PlaneBounds = { pLeft, pBottom, pRight, pTop };
            glyphData.m_AdvanceX = static_cast<float>(g.getAdvance());
            glyphData.m_Width = aRight - aLeft;
            glyphData.m_Height = aTop - aBottom;
            glyphData.m_BearingUnderline = pBottom * fontSize;

            data.GlyphInfo.insert({ g.getCodepoint(), glyphData });

            if (glyphData.m_Height > data.MaxHeight) {
                code = g.getCodepoint();
            }

            data.MaxHeight = std::max(data.MaxHeight, (int)((float)glyphData.m_Height + glyphData.m_BearingUnderline));
        }

        // Create texture from the raw atlas data
        TextureDescription textureDesc{};
        textureDesc.WithWidth( atlasWidth )
                .WithHeight( atlasHeight )
                .WithChannelCount( 4 )

                .WithData( (unsigned char*)bitmap.pixels )

                .WithType( TextureType::TEXTURE_2D )
                .WithFormat( TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM )

                .WithResourceType( ResourceUsageType::RESOURCE_USAGE_STATIC );

        TextureHandle texture{ m_GpuDevice->CreateTexture( textureDesc ) };

        data.TextureAtlas = texture;
    }

    FontFactory::FontFactory( const FontFactoryCreateInfo &options )
        : m_GpuDevice( options.Device ) {}

    auto FontFactory::Init() -> void {
        m_FreeTypeHandle = msdfgen::initializeFreetype();

        m_IsInitialized = true;
    }

    auto FontFactory::Shutdown() -> void {
        if ( !m_IsInitialized ) {
            return;
        }

        msdfgen::deinitializeFreetype( m_FreeTypeHandle );

        m_IsInitialized = false;
    }

    auto FontFactory::LoadFont( const FontLoadDescription &description ) -> FontHandle {
        // Create Atlas texture
        const std::string &path{ description.FontFile->GetPath() };
        auto result{ GenerateAtlas( path.c_str(), description.FontSize ) };

        result.TextureAtlas->SetDebugName( fmt::format( "FontAtlas {}", description.FontFile->GetPath() ) );


#if false
        msdfgen::FontHandle *font = msdfgen::loadFont( m_FreeTypeHandle, description.FontFile->GetPath().c_str() );

        msdfgen::Shape shape;
        if (msdfgen::loadGlyph(shape, font, 'C'))
        {
            shape.normalize();
            //                      max. angle
            msdfgen::edgeColoringSimple(shape, 3.0);
            //           image width, height
            msdfgen::Bitmap<float, 4> msdf(32, 32);
            //                     range, scale, translation
            msdfgen::generateMTSDF(msdf, shape, 4.0, 1.0, msdfgen::Vector2(4.0, 4.0));
            msdfgen::savePng(msdf, "output.png");
        }
#endif

        // Construct font
        Font *newFont{ new Font( result.TextureAtlas, description.FontSize ) };

        newFont->SetPath( description.FontFile->GetPath() );
        newFont->SetName( description.FontFile->GetName() );

        // Fill glyph data
        for ( auto &[codepoint, glyphData]: result.GlyphInfo ) {
            if ( codepoint == 0 ) {
                continue;
            }

            newFont->RegisterGlyph( codepoint, glyphData );
        }

        newFont->m_MaxHeight = result.MaxHeight;

        return FontHandle::Create( newFont );
    }
}// namespace Mikoto