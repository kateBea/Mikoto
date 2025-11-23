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

    auto Font::GetGlyph( UInt32 characterCode ) const -> const FontGlyph& {
        return m_Glyphs.at( characterCode );
    }

    auto Font::RegisterGlyph( UInt32 characterCode, const FontGlyph& glyph ) -> void {
        m_Glyphs.emplace( characterCode, glyph );
    }

    auto Font::SetName( std::string_view name ) -> void {
        m_Name = name;
    }
    auto Font::SetPath( std::string_view path ) -> void {
        m_Path = path;
    }


    auto Font::GetGlyphCount() const -> Size {
        return m_Glyphs.size();
    }

}