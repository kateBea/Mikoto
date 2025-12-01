//
// Created by zanet on 3/2/2025.
//

#ifndef FONT_HH
#define FONT_HH

#include <string>

#include <ankerl/unordered_dense.h>

#include <Renderer/Core/GpuDevice.hh>
#include <Common/Common.hh>
#include <Library/Data/ResourcePool.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    // Represents the specific characters of a given font
    class FontGlyph final {
    public:

        MKT_NODISCARD bool IsSpace() const { return m_Codepoint == ' '; }
        MKT_NODISCARD bool IsLineFeed() const { return m_Codepoint == '\n'; }

        MKT_NODISCARD auto GetUVMin() const -> const glm::vec2& { return m_UVMin; }
        MKT_NODISCARD auto GetUVMax() const -> const glm::vec2& { return m_UVMax; }
        MKT_NODISCARD auto GetPlaneMin() const -> const glm::vec2& { return m_PlaneBoundsMin; }
        MKT_NODISCARD auto GetPlaneMax() const -> const glm::vec2& { return m_PlaneBoundsMax; }

        MKT_NODISCARD auto GetSize() const -> const glm::ivec2& { return m_Size; }
        MKT_NODISCARD auto GetBearing() const -> const glm::ivec2& { return m_Bearing; }
        MKT_NODISCARD auto GetAdvance() const -> UInt32 { return m_Advance; }


    public:
        // adjust
        UInt32 m_Codepoint{};
        glm::ivec2 m_Size{};
        glm::ivec2 m_Bearing{};
        UInt32 m_Advance{};

        glm::vec2 m_UVMin{};
        glm::vec2 m_UVMax{};
        glm::vec2 m_PlaneBoundsMin{};
        glm::vec2 m_PlaneBoundsMax{};


        // we will go with these
        double m_Width{};
        double m_Height{};
        double m_BearingX{};
        double m_BearingY{};
        double m_AdvanceX{};
        double m_BearingUnderline{};
        Vec4F m_AtlasBounds{};
        Vec4F m_PlaneBounds{};

    };

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
        MKT_NODISCARD auto GetPath() const -> const Path&;

        /**
        * @brief Gets the name of the font.
        * @return Reference to the font name.
        */
        MKT_NODISCARD auto GetName() const -> const std::string&;

        /**
        * @brief Gets the font atlas containing glyph textures.
        * @return Pointer to the FontAtlas.
        */
        MKT_NODISCARD auto GetAtlas() const -> TextureHandle;

        /**
        * @brief Retrieves the glyph data for a given character.
        * @param characterCode Unicode code point of the character.
        * @return Pointer to the corresponding FontGlyph, or nullptr if not found.
        */
        MKT_NODISCARD auto GetGlyph( UInt32 characterCode ) const -> const FontGlyph&;
        MKT_NODISCARD auto GetGlyph( UInt32 characterCode ) -> FontGlyph&;

        MKT_NODISCARD auto GetMaxHeight() const -> UInt32 { return m_MaxHeight; }

        auto RegisterGlyph( UInt32 characterCode, const FontGlyph& glyph ) -> void;

        auto SetName( std::string_view name ) -> void;
        auto SetPath( std::string_view path ) -> void;

        MKT_NODISCARD auto GetGlyphCount() const -> Size;

        auto GetSize() -> double { return m_PixelSize; }

        /**
        * @brief Default destructor.
        */
        ~Font() override = default;

    private:
        // Parameters filled from FontFactory class
        friend class FontFactory;

    protected:

        Path m_Path{};
        std::string m_Name{};
        TextureHandle m_Atlas{};

        double m_PixelSize{ 5.0 };
        double m_MaxHeight{};
        ankerl::unordered_dense::map<UInt32, FontGlyph> m_Glyphs{};
    };

    using FontHandle = Ref<Font>;
}

#endif//FONT_HH
