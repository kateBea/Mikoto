//
// Created by kate on 1/4/25.
//

#ifndef MODELLOADINFO_HH
#define MODELLOADINFO_HH

#include <STL/Utility/Types.hh>

namespace Mikoto {
    struct ModelLoadInfo {
        Path_T ModelPath{};
        bool InvertedY{};// Y down (for vulkan)
        bool WantTextures{ true };
    };
}
#endif //MODELLOADINFO_HH
