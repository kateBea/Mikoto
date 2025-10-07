//
// Created by zanet on 3/2/2025.
//

#ifndef FONT_HH
#define FONT_HH
#include <string>


#include <ankerl/unordered_dense.h>


#include <Common/Common.hh>
#include <Library/Data/ResourcePool.hh>
#include <Library/Utility/Types.hh>
#include <Renderer/FontGlyph.hh>
#include <Renderer/GpuDevice.hh>

namespace Mikoto {

    /**
    * @class Font
    * @brief Represents a font and its associated glyphs.
    *
    * The `Font` class provides functionality for managing fonts, including
    * loading, retrieving glyphs, and accessing font metadata. It supports
    * font atlas generation and is designed for use in rendering systems.
    */
    class Font final : public ReferenceCounted {
    public:
        /**
         * @brief Constructs a Font object from the provided load information.
         * @param fontAtlas
         * @param pixelSize
         */
        explicit Font( TextureHandle fontAtlas, float pixelSize );

        /**
        * @brief Gets the path of the font file.
        * @return Reference to the font file path.
        */
        MKT_NODISCARD auto GetPath() const -> const Path& { return m_Path; }

        /**
        * @brief Gets the name of the font.
        * @return Reference to the font name.
        */
        MKT_NODISCARD auto GetName() const -> const std::string& { return m_Name; }

        /**
        * @brief Gets the font atlas containing glyph textures.
        * @return Pointer to the FontAtlas.
        */
        MKT_NODISCARD auto GetAtlas() const -> TextureHandle { return m_Atlas; }

        /**
        * @brief Retrieves the glyph data for a given character.
        * @param characterCode Unicode code point of the character.
        * @return Pointer to the corresponding FontGlyph, or nullptr if not found.
        */
        MKT_NODISCARD auto GetGlyph( UInt32 characterCode ) -> FontGlyph;

        /**
        * @brief Default destructor.
        */
        ~Font() override = default;

    protected:

        Path m_Path{};

        std::string m_Name{};

        float m_PixelSize{ 0.0f };

        TextureHandle m_Atlas{ nullptr };

        ankerl::unordered_dense::map<UInt32, FontGlyph> m_Glyphs{};
    };

    using FontHandle = Ref<Font>;
}

#endif//FONT_HH
