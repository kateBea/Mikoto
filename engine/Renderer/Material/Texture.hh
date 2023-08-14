//
// Created by kate on 6/8/23.
//

#ifndef KATE_ENGINE_TEXTURE_HH
#define KATE_ENGINE_TEXTURE_HH

#include <Utility/Common.hh>

namespace Mikoto {
    class Texture {
    public:
        Texture() = default;
        virtual ~Texture() = default;

        // Move to opengl textures
        virtual auto Bind(UInt32_T slot = 0) -> void = 0;
    };
}

#endif//KATE_ENGINE_TEXTURE_HH
