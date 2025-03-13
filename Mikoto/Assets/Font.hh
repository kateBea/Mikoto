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
        float Size{ 12 };
    };

    class Font {
    public:
        explicit Font( const FontLoadInfo &loadInfo );

        MKT_NODISCARD auto GetSize() const -> float { return m_Size; }
        MKT_NODISCARD auto GetSpacing() const -> float { return m_Spacing; }
        MKT_NODISCARD auto GetName() const -> const std::string& { return m_Name; }
        MKT_NODISCARD auto GetPath() const -> const Path_T& { return m_Path; }

        auto SetSize(const float value) -> void { if (value != 0) m_Size = value; }
        auto SetSpacing(const float value) -> void { if (value != 0) m_Spacing = value; }

        MKT_NODISCARD static auto Create( const FontLoadInfo &loadInfo ) -> Scope_T<Font>;

        virtual ~Font() = default;

    private:
        Path_T m_Path{};
        std::string m_Name{};

        float m_Size{ 12 };
        float m_Spacing{ 0 };
    };

}// namespace Mikoto

#endif //FONT_HH
