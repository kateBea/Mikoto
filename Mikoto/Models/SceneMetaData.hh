//
// Created by kate on 12/17/24.
//

#ifndef SCENEMETADATA_HH
#define SCENEMETADATA_HH

#include <STL/Utility/Types.hh>

namespace Mikoto {
    struct SceneMetaData {
        UInt64_T EntityCount{};
        UInt64_T LightsCount{};
        UInt64_T ActiveLightCount{};
        UInt64_T DirLightsCount{};
        UInt64_T DirActiveLightCount{};
        UInt64_T SpotLightsCount{};
        UInt64_T SpotActiveLightCount{};
        UInt64_T PointLightsCount{};
        UInt64_T PointActiveLightCount{};
    };

}

#endif //SCENEMETADATA_HH
