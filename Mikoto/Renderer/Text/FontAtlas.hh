//
// Created by zanet on 3/18/2025.
//

#ifndef FONTATLAS_HH
#define FONTATLAS_HH

#include <Library/Utility/Types.hh>
#include <Material/Texture/Texture2D.hh>

namespace Mikoto {
    class FontAtlas final {
    public:
        explicit FontAtlas( const Path_T& fontPath )
            : m_Path{ fontPath } {
        }

        auto Init() -> void;

        MKT_NODISCARD auto GetAtlasTexture() const -> Texture2D* {
            return m_Texture.get();
        }

    private:
        Path_T m_Path{};
        Scope_T<Texture2D> m_Texture{};
    };
}// namespace Mikoto


#endif//FONTATLAS_HH
