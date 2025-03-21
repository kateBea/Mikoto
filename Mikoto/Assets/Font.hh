//
// Created by zanet on 3/2/2025.
//

#ifndef FONT_HH
#define FONT_HH

#include <string>

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {
    struct FontLoadInfo {
        Path_T Path{};
        float Size{ 48 };
        float Spacing{ 1 };
    };

    class Font {
    public:
        explicit Font( const FontLoadInfo &loadInfo );

        MKT_NODISCARD auto GetName() const -> const std::string& { return m_Name; }
        MKT_NODISCARD auto GetPath() const -> const Path_T& { return m_Path; }

        MKT_NODISCARD static auto Create( const FontLoadInfo &loadInfo ) -> Scope_T<Font>;

        virtual ~Font() = default;

    protected:
        Path_T m_Path{};
        std::string m_Name{};

        float m_Size{};
        float m_Spacing{};
    };

}// namespace Mikoto

#endif //FONT_HH
