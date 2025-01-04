//
// Created by kate on 1/4/25.
//

#ifndef SHADERCREATEINFO_HH
#define SHADERCREATEINFO_HH

#include <Models/Enums.hh>
#include <STL/Utility/Types.hh>

namespace Mikoto {
    struct ShaderCreateInfo {
        ShaderStage Stage{};
        Path_T Directory{};
    };
}

#endif //SHADERCREATEINFO_HH
