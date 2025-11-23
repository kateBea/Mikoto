//
// Created by zanet on 3/15/2025.
//

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

    auto FontFactory::GenerateAtlas( const CStr fontFilename ) const -> MsdfData {
        using namespace msdf_atlas;

        MsdfData result{};

        // Initialize instance of FreeType library
        if ( m_FreeTypeHandle != nullptr ) {
            msdfgen::FontHandle *font{ msdfgen::loadFont( m_FreeTypeHandle, fontFilename ) };

            if ( font != nullptr ) {
                // Storage for glyph geometry and their coordinates in the atlas
                MsdfGlyphGeometryList glyphs{};

                // FontGeometry is a helper class that loads a set of glyphs from a single font.
                // It can also be used to get additional font metrics, kerning information, etc.
                FontGeometry fontGeometry( std::addressof( glyphs ) );

                // Specify a set of character glyphs
                std::array charsetRanges{
                    CharsetRange{ 0x0020, 0x007F },// Basic Latin
                    CharsetRange{ 0x3040, 0x309F },// Hiragana
                    CharsetRange{ 0x30A0, 0x30FF } // Katakana
                    //CharsetRange{ 0x00A0, 0x00FF },// Latin-1 Supplement
                    //CharsetRange{ 0x0100, 0x017F },// Latin Extended-A
                    //CharsetRange{ 0x0180, 0x024F },// Latin Extended-B
                    //CharsetRange{ 0x0370, 0x03FF },// Greek & Coptic
                    //CharsetRange{ 0x0400, 0x04FF } // Cyrillic
                };

                msdf_atlas::Charset charset{};
                for ( auto [Start, End]: charsetRanges ) {
                    for ( UInt32 c = Start; c <= End; c++ ) {
                        charset.add( c );
                    }
                }

                // The second argument can be ignored unless you mix different font sizes in one atlas.
                // In the last argument, you can specify a charset other than ASCII.
                // To load specific glyph indices, use loadGlyphs instead.
                fontGeometry.loadCharset( font, 5.0, charset );

                // Apply MSDF edge coloring. See edge-coloring.h for other coloring strategies.
                constexpr double maxCornerAngle{ 3.0 };

                for ( GlyphGeometry &glyph: glyphs ) {
                    glyph.edgeColoring( &msdfgen::edgeColoringInkTrap, maxCornerAngle, 0 );
                }

                // TightAtlasPacker class computes the layout of the atlas.
                TightAtlasPacker packer{};

                // Set atlas parameters:
                // setDimensions or setDimensionsConstraint to find the best value
                packer.setDimensionsConstraint( DimensionsConstraint::SQUARE );

                // setScale for a fixed size or setMinimumScale to use the largest that fits
                packer.setMinimumScale( 24.0 );

                // setPixelRange or setUnitRange
                packer.setPixelRange( 2.0 );
                packer.setMiterLimit( 1.0 );

                // Compute atlas layout - pack glyphs
                packer.pack( glyphs.data(), static_cast<int>( glyphs.size() ) );

                // Get final atlas dimensions
                Int32 width{};
                Int32 height{};
                packer.getDimensions( width, height );

                result.AtlasWidth = width;
                result.AtlasHeight = height;

                // GeneratorAttributes can be modified to change the generator's default settings.
                GeneratorAttributes attributes{
                    .config{ msdfgen::MSDFGeneratorConfig{ true } },
                    .scanlinePass{ true }
                };

                // The ImmediateAtlasGenerator class facilitates the generation of the atlas bitmap.
                MsdfAtlasGen generator{ width, height };

                generator.setAttributes( attributes );
                generator.setThreadCount( 8 );

                // Generate atlas bitmap
                generator.generate( glyphs.data(), glyphs.size() );

                // Copy the atlas pixel data
                constexpr Int32 channelCount{ 4 };
                std::vector<Byte> bytes{};
                const msdfgen::BitmapConstRef<msdfgen::byte, 4> &bitmapRef{ generator.atlasStorage() };
                for ( Int64 index{}; index < height * width * channelCount; ++index ) {
                    bytes.emplace_back( static_cast<Byte>( bitmapRef.pixels[index] ) );
                }

                result.Bytes = std::move( bytes );
                result.GlyphData = std::move( glyphs );

                // Cleanup
                msdfgen::destroyFont( font );
            }
        }

        return result;
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
        auto result{ GenerateAtlas( path.c_str() ) };

        // Match the channel count for bitmap atlas storage
        const Int32 width{ result.AtlasWidth };
        const Int32 height{ result.AtlasHeight };
        constexpr auto channelCount{ 4 };

        // Create texture from the raw atlas data
        TextureDescription textureDesc{};
        textureDesc.WithWidth( width )
                .WithHeight( height )
                .WithChannelCount( channelCount )

                .WithData( result.Bytes.data() )

                .WithType( TextureType::TEXTURE_2D )
                .WithFormat( TextureFormat::TEXTURE_FORMAT_SRGB8_ALPHA8 )

                .WithResourceType( ResourceUsageType::RESOURCE_USAGE_STATIC );

        TextureHandle texture{ m_GpuDevice->CreateTexture( textureDesc ) };
        texture->SetDebugName( fmt::format("FontAtlas: {}", description.FontFile->GetPath() ) );

        // Construct font
        Font *newFont{ new Font( texture, description.PixelSize ) };

        newFont->SetPath( description.FontFile->GetPath() );
        newFont->SetName( description.FontFile->GetName() );

        // Fill glyph data
        for ( auto &g: result.GlyphData ) {
            msdfgen::unicode_t codepoint{ g.getCodepoint() };
            if ( codepoint == 0 ) {
                continue;
            }

            double advance{ g.getAdvance() };

            double pl{}, pb{}, pr{}, pt{};
            g.getQuadPlaneBounds( pl, pb, pr, pt );

            double texL{}, texB{}, texR{}, texT{};
            g.getQuadAtlasBounds( texL, texB, texR, texT );

            FontGlyph glyph{};
            glyph.m_Codepoint = codepoint;

            // Pixel space size (use atlas bounds)
            glyph.m_Size = glm::ivec2(
                    static_cast<UInt32>( texR - texL ),
                    static_cast<UInt32>( texT - texB ) );

            // Plane bounds (used by shader to scale quad)
            glyph.m_PlaneBoundsMin = glm::vec2( static_cast<float>( pl ), static_cast<float>( pb ) );
            glyph.m_PlaneBoundsMax = glm::vec2( static_cast<float>( pr ), static_cast<float>( pt ) );

            // UV coords (we must normalize by atlas resolution)
            glyph.m_UVMin = glm::vec2(
                    static_cast<float>( texL ) / static_cast<float>( result.AtlasWidth ),
                    static_cast<float>( texB ) / static_cast<float>( result.AtlasHeight ) );
            glyph.m_UVMax = glm::vec2(
                    static_cast<float>( texR ) / static_cast<float>( result.AtlasWidth ),
                    static_cast<float>( texT ) / static_cast<float>( result.AtlasHeight ) );

            // --- IMPORTANT: FreeType bearing and advance are baked into the plane! --- //
            glyph.m_Advance = static_cast<UInt32>( advance );

            // Bearing should be derived from plane bounds (only x matters)
            glyph.m_Bearing = glm::ivec2(
                    static_cast<UInt32>( pl ),// Left offset
                    static_cast<UInt32>( pt ) // Top offset
            );

            newFont->RegisterGlyph( codepoint, glyph );
        }

        return FontHandle::Create( newFont );
    }
}// namespace Mikoto