//
// Created by kate on 11/9/23.
//

#ifndef MIKOTO_DEFAULT_MATERIAL_HH
#define MIKOTO_DEFAULT_MATERIAL_HH

#include <Renderer/Material/Material.hh>

namespace Mikoto {
    class DefaultMaterial : public Material {
    public:



    private:
        float m_Shininess{};
    };
}

#endif// MIKOTO_DEFAULT_MATERIAL_HH
