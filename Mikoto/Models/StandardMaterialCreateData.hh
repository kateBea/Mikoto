//
// Created by kate on 1/4/25.
//

#ifndef STANDARDMATERIALDATA_HH
#define STANDARDMATERIALDATA_HH
#include <Material/Texture/Texture2D.hh>
#include <memory>

namespace Mikoto {
    struct StandardMaterialCreateData {
        std::shared_ptr<Texture2D> DiffuseMap{ nullptr };
        std::shared_ptr<Texture2D> SpecularMap{ nullptr };
    };
}
#endif //STANDARDMATERIALDATA_HH
