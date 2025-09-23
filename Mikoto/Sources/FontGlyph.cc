// //
// // Created by zanet on 3/15/2025.
// //
//
// #include <Common/Common.hh>
//
// #include <Core/Logging/Logger.hh>
//
// #include "../Renderer/Text/FontGlyph.hh"
// #include "../Renderer/Text/FontService.hh"
//
// namespace Mikoto {
//
//     FontGlyph::FontGlyph( FT_Face face, FT_UInt characterIndex )
//         : m_Face{ face }, m_GlyphIndex{ characterIndex } {
//         auto error{ FT_Load_Glyph( m_Face, m_GlyphIndex, FT_LOAD_DEFAULT ) };
//         if ( error != 0 ) {
//             MKT_THROW_RUNTIME_ERROR( "failed to load glyph" );
//         }
//
//         FT_GlyphSlot glyphSlot{ m_Face->glyph };
//         error = FT_Render_Glyph( glyphSlot, FT_RENDER_MODE_NORMAL );
//         if ( error ) {
//             MKT_THROW_RUNTIME_ERROR( "failed to render glyph" );
//         }
//
//         m_Bearing.x = glyphSlot->bitmap_left;
//         m_Bearing.y = glyphSlot->bitmap_top;
//
//         m_Size.x = static_cast<int>( glyphSlot->metrics.width / 64 );
//         m_Size.y = static_cast<int>( glyphSlot->metrics.height / 64 );
//
//         m_Advance = static_cast<int>( glyphSlot->advance.x / 64 );
//
//         CreateBuffers();
//
//         CreateTextureFromBitmap();
//     }
//
//     auto FontGlyph::CreateBuffers() -> void {
//         float x0 = static_cast<float>(m_Bearing.x);
//         float y0 = static_cast<float>(m_Bearing.y - m_Size.y);
//         float x1 = x0 + static_cast<float>(m_Size.x);
//         float y1 = y0 + static_cast<float>(m_Size.y);
//
//         // Default neutral color
//         float r{ 0.0f };
//         float g{ 0.0f };
//         float b{ 0.0f };
//
//         std::vector<float> vertices{
//             // Position (x, y, z)  | Color (r, g, b) | UV (u, v)
//             x0, y0, 0.0f, r, g, b, 0.0f, 1.0f,// Bottom-left
//             x1, y0, 0.0f, r, g, b, 1.0f, 1.0f,// Bottom-right
//             x1, y1, 0.0f, r, g, b, 1.0f, 0.0f,// Top-right
//             x0, y1, 0.0f, r, g, b, 0.0f, 0.0f // Top-left
//         };
//
//         const std::vector<UInt32_T> indices{ 0, 1, 2, 2, 3, 0 };
//
//         m_VertexBuffer = VertexBuffer::Create( vertices, s_DefaultBufferLayout );
//         m_IndexBuffer = IndexBuffer::Create( indices );
//     }
//
//     auto FontGlyph::CreateTextureFromBitmap() -> void {
//         FT_GlyphSlot glyphSlot{ m_Face->glyph };
//
//         if(glyphSlot->bitmap.pixel_mode != FT_PIXEL_MODE_GRAY || glyphSlot->bitmap.num_grays != 256) {
//             MKT_THROW_RUNTIME_ERROR("unsupported pixel mode");
//         }
//
//         constexpr Int32_T channelCount{ 4 };
//
//         const Int32_T width{ static_cast<Int32_T>( glyphSlot->bitmap.width ) };
//         const Int32_T height{ static_cast<Int32_T>( glyphSlot->bitmap.rows ) };
//         const Size_T bufferSize { static_cast<Size_T>( width * height * channelCount ) };
//
//         if(bufferSize == 0) {
//             return;
//         }
//
//         std::vector<Byte_T> buffer( bufferSize );
//
//         const Byte_T* src{ glyphSlot->bitmap.buffer };
//
//         Int32_T dst{ 0 };
//
//         // The bitmap provided by FreeType is an 8-bit grayscale format.
//         // I’m using a dynamic texture atlas for all my textures, so it is a
//         // full 32-bit RGBA format, and this method does the conversion for
//         // each pixel to a full white, with the original grayscale value as the alpha component.
//
//         for(Int32_T y{ 0 }; y < height; ++y) {
//
//             for(Int32_T x{ 0 }; x < width; ++x) {
//                 const Byte_T value{ *src };
//
//                 src++;
//
//                 buffer[dst++] = 0xff;
//                 buffer[dst++] = 0xff;
//                 buffer[dst++] = 0xff;
//                 buffer[dst++] = value;
//             }
//         }
//
//         m_Texture = FontManager::CreateTexture(width, height, 4, buffer);
//     }
// }