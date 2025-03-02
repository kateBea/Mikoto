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

        MKT_NODISCARD static auto Create( const FontLoadInfo &loadInfo );

    private:

    };

}// namespace Mikoto

#endif //FONT_HH
