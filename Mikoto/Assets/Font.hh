//
// Created by zanet on 3/2/2025.
//

#ifndef FONT_HH
#define FONT_HH

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {
    struct FontLoadInfo {
        Path_T Path{};
        Size_T Size{ 12 };
    };

    class Font {
    public:
        explicit Font( const FontLoadInfo &loadInfo );

        MKT_NODISCARD auto GetSize() const -> Size_T { return m_Size; }

        auto SetSize(const Size_T value) -> void { if (value != 0) m_Size = value; }

        MKT_NODISCARD static auto Create( const FontLoadInfo &loadInfo ) -> Scope_T<Font>;

    private:
        Size_T m_Size{ 12 };
    };

}// namespace Mikoto

#endif //FONT_HH
