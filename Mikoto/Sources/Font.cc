//
// Created by zanet on 3/2/2025.
//

#include <Assets/Font.hh>

namespace Mikoto {

    Font::Font( TextureHandle fontAtlas, float pixelSize ) 
        : m_Atlas{ fontAtlas },
          m_PixelSize{ pixelSize }
    {}

    auto Font::GetPath() const -> const Path& {
        return m_Path;
    }

    auto Font::GetName() const -> const std::string& {
        return m_Name;
    }

    auto Font::GetAtlas() const -> TextureHandle {
        return m_Atlas;
    }

    auto Font::GetGlyph( const UInt32 characterCode ) const -> const FontGlyph& {
        return m_Glyphs.at( characterCode );
    }

    auto Font::GetGlyph( const UInt32 characterCode ) -> FontGlyph& {
        return m_Glyphs.at( characterCode );
    }

    auto Font::GetMaxHeight() const -> double {
        return m_MaxHeight;
    }

    auto Font::GetMaxWidth() const -> double {
        return m_MaxWidth;
    }

    auto Font::SetMaxHeight( const double maxHeight ) -> void {
        m_MaxHeight = maxHeight;
    }

    auto Font::SetMaxWidth( const double maxWidth ) -> void {
        m_MaxWidth = maxWidth;
    }

    auto Font::HasGlyph( const UInt32 unicodePoint ) const -> bool {
        return m_Glyphs.contains( unicodePoint );
    }

    auto Font::RegisterGlyph( UInt32 characterCode, const FontGlyph& glyph ) -> void {
        m_Glyphs.emplace( characterCode, glyph );
    }

    auto Font::SetName( const std::string_view name ) -> void {
        m_Name = name;
    }

    auto Font::SetPath( const std::string_view path ) -> void {
        m_Path = path;
    }

    auto Font::GetGlyphCount() const -> Size {
        return m_Glyphs.size();
    }

}