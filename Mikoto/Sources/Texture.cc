//
// Created by zanet on 3/28/2025.
//

#include <Library/Utility/Types.hh>
#include <Assets/Texture.hh>

namespace Mikoto {

    auto TextureLoadInfo::WithPath( const Path_T& path ) -> TextureLoadInfo& {
        Path = path;
        return *this;
    }

    auto TextureLoadInfo::WithType( const TextureType type ) -> TextureLoadInfo& {
        this->Type = type;
        return *this;
    }
}