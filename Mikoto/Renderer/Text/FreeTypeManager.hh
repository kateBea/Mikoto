//
// Created by zanet on 3/15/2025.
//

#ifndef FREETYPEMANAGER_HH
#define FREETYPEMANAGER_HH

#include <ft2build.h>

#include <Assets/Texture.hh>
#include <Common/Common.hh>

#include FT_FREETYPE_H

namespace Mikoto {
    class FreeTypeManager {
    public:

        static auto Init() -> void;
        static auto Shutdown() -> void;

        MKT_NODISCARD static auto GetLibrary() -> FT_Library& { return m_FreeTypeLibrary; }

        MKT_NODISCARD static auto CreateTexture(Int32_T width, Int32_T height, Int32_T channelCount, const std::vector<UInt8_T>& data) -> Scope_T<Texture>;

    private:
        inline static FT_Library m_FreeTypeLibrary{};
    };
}



#endif //FREETYPEMANAGER_HH
