//
// Created by zanet on 3/15/2025.
//

#ifndef FREETYPEFONT_HH
#define FREETYPEFONT_HH

#include <memory>
#include <unordered_map>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <Library/Utility/Types.hh>

#include <Assets/Font.hh>
#include <Renderer/Text/FreeTypeGlyph.hh>

namespace Mikoto {
    class FreeTypeFont : public Font {
    public:

        explicit FreeTypeFont(const FontLoadInfo &loadInfo);

        MKT_NODISCARD auto GetGlyphList() const -> decltype( auto ) { return (m_Glyphs); }
        MKT_NODISCARD auto GetGlyphCount() const -> Size_T { return m_Glyphs.size(); }

        MKT_NODISCARD auto GetGlyph( const UInt64_T characterCode) -> FreeTypeGlyph* {
            auto glyphIndex{ FT_Get_Char_Index( m_Face, characterCode ) };
            const auto findIt{ m_Glyphs.find( glyphIndex ) };

            if (findIt != m_Glyphs.end()) {
                return findIt->second.get();
            }

            // Try loading the missing character
            auto [insertIt, success] {
                m_Glyphs.try_emplace( glyphIndex,
                    CreateScope<FreeTypeGlyph>( m_Face, glyphIndex ) )
            };

            return success ? insertIt->second.get() : nullptr;
        }

        ~FreeTypeFont() override;

    protected:
        FT_Face m_Face{};
        std::unordered_map<UInt64_T, Scope_T<FreeTypeGlyph>> m_Glyphs{};
    };
}



#endif //FREETYPEFONT_HH
