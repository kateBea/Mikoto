//
// Created by zanet on 1/26/2025.
//

#ifndef TEXTURE_HH
#define TEXTURE_HH

#include <string_view>
#include <unordered_map>

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>
#include <Models/Enums.hh>

namespace Mikoto {
    struct TextureLoadInfo {
        Path_T Path{};
        MapType Type{};
    };

    class Texture {
    public:

        MKT_NODISCARD auto GetID() const -> const GlobalUniqueID& { return m_Id; }

        virtual ~Texture() = default;

    private:
        GlobalUniqueID m_Id{};
    };
}

#endif //TEXTURE_HH
