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

        explicit FontGlyph( const UInt32 characterCode ) noexcept
            : m_Codepoint{ characterCode } {}

        // Queries
        MKT_NODISCARD auto IsSpace() const noexcept -> bool { return m_Codepoint == ' '; }
        MKT_NODISCARD auto IsLineFeed() const noexcept -> bool { return m_Codepoint == '\n'; }

        MKT_NODISCARD auto GetCodepoint() const noexcept -> UInt32 { return m_Codepoint; }

        MKT_NODISCARD auto GetWidth() const noexcept -> double { return m_Width; }
        MKT_NODISCARD auto GetHeight() const noexcept -> double { return m_Height; }

        MKT_NODISCARD auto GetBearingX() const noexcept -> double { return m_BearingX; }
        MKT_NODISCARD auto GetBearingY() const noexcept -> double { return m_BearingY; }

        MKT_NODISCARD auto GetAdvanceX() const noexcept -> double { return m_AdvanceX; }

        MKT_NODISCARD auto GetBearingUnderline() const noexcept -> double { return m_BearingUnderline; }

        MKT_NODISCARD auto GetAtlasBounds() const noexcept -> const Vec4F& { return m_AtlasBounds; }
        MKT_NODISCARD auto GetPlaneBounds() const noexcept -> const Vec4F& { return m_PlaneBounds; }

        auto SetCodepoint( const UInt32 cp) noexcept -> void { m_Codepoint = cp; }

        auto SetWidth( const double width) noexcept -> void { m_Width = width; }
        auto SetHeight( const double height) noexcept -> void { m_Height = height; }

        auto SetBearingX( const double bx) noexcept -> void { m_BearingX = bx; }
        auto SetBearingY( const double by) noexcept -> void { m_BearingY = by; }

        auto SetAdvanceX( const double adv) noexcept -> void { m_AdvanceX = adv; }

        auto SetBearingUnderline( const double bu) noexcept -> void { m_BearingUnderline = bu; }

        auto SetAtlasBounds(const Vec4F& bounds) noexcept -> void { m_AtlasBounds = bounds; }
        auto SetPlaneBounds(const Vec4F& bounds) noexcept -> void { m_PlaneBounds = bounds; }

    public:
        // adjust
        UInt32 m_Codepoint{};

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

        MKT_NODISCARD auto GetMaxHeight() const -> double;
        MKT_NODISCARD auto GetMaxWidth() const -> double;

        auto RegisterGlyph( UInt32 characterCode, const FontGlyph& glyph ) -> void;

        auto SetName( std::string_view name ) -> void;
        auto SetPath( std::string_view path ) -> void;

        auto SetMaxHeight( double maxHeight ) -> void;
        auto SetMaxWidth(double maxWidth ) -> void;

        MKT_NODISCARD auto HasGlyph(UInt32 unicodePoint ) const -> bool;

        MKT_NODISCARD auto GetGlyphCount() const -> Size;
        MKT_NODISCARD auto GetSize() const -> double { return m_PixelSize; }

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
        double m_MaxWidth{};
        ankerl::unordered_dense::map<UInt32, FontGlyph> m_Glyphs{};
    };

    using FontHandle = Ref<Font>;
}

#endif//FONT_HH
