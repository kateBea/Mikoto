//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MIKOTO_FONT_HH
#define MIKOTO_FONT_HH

#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/ResourcePool.hh>
#include <Core/Types.hh>
#include <Filesystem/Path.hh>
#include <Renderer/Core/GpuDevice.hh>


namespace mikoto::renderer {

    // Represents the specific characters of a given font
    class FontGlyph final {
    public:
        explicit FontGlyph( const u32 characterCode ) noexcept
            : mCodepoint{ characterCode } {}

        MKT_NODISCARD constexpr auto IsSpace() const noexcept -> bool { return mCodepoint == as<u32>( L' ' ); }
        MKT_NODISCARD constexpr auto IsLineFeed() const noexcept -> bool { return mCodepoint == as<u32>( L'\n' ); }

        MKT_NODISCARD constexpr auto GetCodepoint() const noexcept -> u32 { return mCodepoint; }

        MKT_NODISCARD constexpr auto GetWidth() const noexcept -> f64 { return mWidth; }
        MKT_NODISCARD constexpr auto GetHeight() const noexcept -> f64 { return mHeight; }

        MKT_NODISCARD constexpr auto GetBearingX() const noexcept -> f64 { return mBearingX; }
        MKT_NODISCARD constexpr auto GetBearingY() const noexcept -> f64 { return mBearingY; }

        MKT_NODISCARD constexpr auto GetAdvanceX() const noexcept -> f64 { return mAdvanceX; }

        MKT_NODISCARD constexpr auto GetBearingUnderline() const noexcept -> f64 { return mBearingUnderline; }

        MKT_NODISCARD constexpr auto GetAtlasBounds() const noexcept -> const float4& { return mAtlasBounds; }
        MKT_NODISCARD constexpr auto GetPlaneBounds() const noexcept -> const float4& { return mPlaneBounds; }

        auto SetCodepoint( const u32 cp ) noexcept -> void { mCodepoint = cp; }

        auto SetWidth( const f64 width ) noexcept -> void { mWidth = width; }
        auto SetHeight( const f64 height ) noexcept -> void { mHeight = height; }

        auto SetBearingX( const f64 bx ) noexcept -> void { mBearingX = bx; }
        auto SetBearingY( const f64 by ) noexcept -> void { mBearingY = by; }

        auto SetAdvanceX( const f64 adv ) noexcept -> void { mAdvanceX = adv; }

        auto SetBearingUnderline( const f64 bu ) noexcept -> void { mBearingUnderline = bu; }

        auto SetAtlasBounds( const float4& bounds ) noexcept -> void { mAtlasBounds = bounds; }
        auto SetPlaneBounds( const float4& bounds ) noexcept -> void { mPlaneBounds = bounds; }

    public:
        // adjust
        u32 mCodepoint{};

        // we will go with these
        f64 mWidth{};
        f64 mHeight{};
        f64 mBearingX{};
        f64 mBearingY{};
        f64 mAdvanceX{};
        f64 mBearingUnderline{};

        float4 mAtlasBounds{};
        float4 mPlaneBounds{};
    };

    class Font final : public ReferenceCounted {
    public:
        explicit Font( TextureHandle fontAtlas, f32 pixelSize, Path path );

        MKT_NODISCARD auto GetAtlas() const -> TextureHandle;

        MKT_NODISCARD auto GetPath() const -> const Path&;
        MKT_NODISCARD auto GetName() const -> eastl::string_view;

        MKT_NODISCARD auto GetGlyph( u32 characterCode ) const -> const FontGlyph&;

        MKT_NODISCARD auto GetMaxHeight() const -> f64;
        MKT_NODISCARD auto GetMaxWidth() const -> f64;

        auto RegisterGlyph( u32 characterCode, const FontGlyph& glyph ) -> void;

        auto SetMaxHeight( f64 maxHeight ) -> void;
        auto SetMaxWidth( f64 maxWidth ) -> void;

        MKT_NODISCARD auto HasGlyph( u32 unicodePoint ) const -> bool;

        MKT_NODISCARD auto GetGlyphCount() const -> size_t;
        MKT_NODISCARD auto GetSize() const -> f64;

        /**
        * @brief Default destructor.
        */
        ~Font() override = default;

    private:
        // Parameters filled from FontFactory class
        friend class FontFactory;

    protected:
        Path mPath{};
        TextureHandle mAtlas{};

        double mPixelSize{ 5.0 };
        double mMaxHeight{};
        double mMaxWidth{};
        ankerl::unordered_dense::map<u32, FontGlyph> mGlyphs{};
    };

    using FontHandle = core::Ref<Font>;
}// namespace mikoto::renderer

#endif//MIKOTO_FONT_HH
