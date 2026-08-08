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

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>

namespace mikoto::renderer {

    // Represents the specific characters of a given font
    class FontGlyph final {
    public:
        explicit FontGlyph( const core::u32 characterCode ) noexcept
            : mCodepoint{ characterCode } {}

        MKT_NODISCARD constexpr auto IsSpace() const noexcept -> bool { return mCodepoint == core::as<core::u32>( L' ' ); }
        MKT_NODISCARD constexpr auto IsLineFeed() const noexcept -> bool { return mCodepoint == core::as<core::u32>( L'\n' ); }

        MKT_NODISCARD constexpr auto GetCodepoint() const noexcept -> core::u32 { return mCodepoint; }

        MKT_NODISCARD constexpr auto GetWidth() const noexcept -> core::f64 { return mWidth; }
        MKT_NODISCARD constexpr auto GetHeight() const noexcept -> core::f64 { return mHeight; }

        MKT_NODISCARD constexpr auto GetBearingX() const noexcept -> core::f64 { return mBearingX; }
        MKT_NODISCARD constexpr auto GetBearingY() const noexcept -> core::f64 { return mBearingY; }

        MKT_NODISCARD constexpr auto GetAdvanceX() const noexcept -> core::f64 { return mAdvanceX; }

        MKT_NODISCARD constexpr auto GetBearingUnderline() const noexcept -> core::f64 { return mBearingUnderline; }

        MKT_NODISCARD constexpr auto GetAtlasBounds() const noexcept -> const core::float4& { return mAtlasBounds; }
        MKT_NODISCARD constexpr auto GetPlaneBounds() const noexcept -> const core::float4& { return mPlaneBounds; }

        auto SetCodepoint( const core::u32 cp ) noexcept -> void { mCodepoint = cp; }

        auto SetWidth( const core::f64 width ) noexcept -> void { mWidth = width; }
        auto SetHeight( const core::f64 height ) noexcept -> void { mHeight = height; }

        auto SetBearingX( const core::f64 bx ) noexcept -> void { mBearingX = bx; }
        auto SetBearingY( const core::f64 by ) noexcept -> void { mBearingY = by; }

        auto SetAdvanceX( const core::f64 adv ) noexcept -> void { mAdvanceX = adv; }

        auto SetBearingUnderline( const core::f64 bu ) noexcept -> void { mBearingUnderline = bu; }

        auto SetAtlasBounds( const core::float4& bounds ) noexcept -> void { mAtlasBounds = bounds; }
        auto SetPlaneBounds( const core::float4& bounds ) noexcept -> void { mPlaneBounds = bounds; }

    public:
        // adjust
        core::u32 mCodepoint{};

        // we will go with these
        core::f64 mWidth{};
        core::f64 mHeight{};
        core::f64 mBearingX{};
        core::f64 mBearingY{};
        core::f64 mAdvanceX{};
        core::f64 mBearingUnderline{};

        core::float4 mAtlasBounds{};
        core::float4 mPlaneBounds{};
    };

    class Font final : public core::ReferenceCounted {
    public:
        explicit Font( rhi::TextureHandle fontAtlas, core::f32 pixelSize, filesystem::Path path );

        MKT_NODISCARD auto GetAtlas() const -> rhi::TextureHandle;

        MKT_NODISCARD auto GetPath() const -> const filesystem::Path&;
        MKT_NODISCARD auto GetName() const -> eastl::string_view;

        MKT_NODISCARD auto GetGlyph( core::u32 characterCode ) const -> const FontGlyph&;

        MKT_NODISCARD auto GetMaxHeight() const -> core::f64;
        MKT_NODISCARD auto GetMaxWidth() const -> core::f64;

        auto RegisterGlyph( core::u32 characterCode, const FontGlyph& glyph ) -> void;

        auto SetMaxHeight( core::f64 maxHeight ) -> void;
        auto SetMaxWidth( core::f64 maxWidth ) -> void;

        MKT_NODISCARD auto HasGlyph( core::u32 unicodePoint ) const -> bool;

        MKT_NODISCARD auto GetGlyphCount() const -> size_t;
        MKT_NODISCARD auto GetSize() const -> core::f64;

        /**
        * @brief Default destructor.
        */
        ~Font() override = default;

    private:
        // Parameters filled from FontFactory class
        friend class FontFactory;

    protected:
        filesystem::Path mPath{};
        rhi::TextureHandle mAtlas{};

        double mPixelSize{ 5.0 };
        double mMaxHeight{};
        double mMaxWidth{};
        ankerl::unordered_dense::map<core::u32, FontGlyph> mGlyphs{};
    };

    using FontHandle = core::Ref<Font>;
}// namespace mikoto::renderer

#endif//MIKOTO_FONT_HH
