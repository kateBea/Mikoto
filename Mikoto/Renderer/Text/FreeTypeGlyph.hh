//
// Created by zanet on 3/15/2025.
//

#ifndef GLYPH_HH
#define GLYPH_HH

#include <ft2build.h>
#include FT_FREETYPE_H

#include <Assets/Texture.hh>
#include <Common/Common.hh>
#include <Library/Utility/Types.hh>
#include <Material/Texture/Texture2D.hh>
#include <Renderer/Buffer/IndexBuffer.hh>
#include <Renderer/Buffer/VertexBuffer.hh>
#include <glm/glm.hpp>

namespace Mikoto {
    // Represents the specific characters of a given font
    class FreeTypeGlyph final {
    public:

        explicit FreeTypeGlyph(FT_Face face, FT_UInt characterIndex);

        MKT_NODISCARD static auto GetDefaultBufferLayout() -> decltype( auto ) {
            return (s_DefaultBufferLayout);
        }

        MKT_NODISCARD auto IsSpace() const -> bool {
            return FT_Get_Char_Index( m_Face, ' ' ) == m_GlyphIndex;
        }

        MKT_NODISCARD auto IsLineFeed() const -> bool {
            return FT_Get_Char_Index( m_Face, '\n' ) == m_GlyphIndex;
        }

        MKT_NODISCARD auto GetSize() const -> const glm::ivec2& { return m_Size; }
        MKT_NODISCARD auto GetBearing() const -> const glm::ivec2& { return m_Bearing; }
        MKT_NODISCARD auto GetAdvance() const -> UInt32_T { return m_Advance; }

        MKT_NODISCARD auto GetTexture() const -> Texture2D* { return dynamic_cast<Texture2D *>(m_Texture.get()); }
        MKT_NODISCARD auto GetVertexBuffer() const -> VertexBuffer* { return m_VertexBuffer.get(); }
        MKT_NODISCARD auto GetIndexBuffer() const -> IndexBuffer* { return m_IndexBuffer.get(); }

    private:

        auto CreateBuffers() -> void;
        auto CreateTextureFromBitmap() -> void;

    private:
        static inline BufferLayout s_DefaultBufferLayout{
                { ShaderDataType::FLOAT3_TYPE, "a_Position" },
                { ShaderDataType::FLOAT3_TYPE, "a_Color" },
                { ShaderDataType::FLOAT2_TYPE, "a_TextureCoordinates" }
        };

        FT_Face m_Face{};
        FT_UInt m_GlyphIndex{};

        glm::ivec2   m_Size{};       // Size of glyph
        glm::ivec2   m_Bearing{};    // Offset from baseline to left/top of glyph
        UInt32_T m_Advance{};    // Offset to advance to next glyph

        Scope_T<Texture> m_Texture{};

        Scope_T<VertexBuffer> m_VertexBuffer{};
        Scope_T<IndexBuffer> m_IndexBuffer{};
    };
}



#endif //GLYPH_HH
