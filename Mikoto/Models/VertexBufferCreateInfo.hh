//
// Created by kate on 1/4/25.
//

#ifndef VERTEXBUFFERCREATEINFO_HH
#define VERTEXBUFFERCREATEINFO_HH

#include <vector>
#include "BufferLayout.hh"

namespace Mikoto {
    struct VertexBufferCreateInfo {
        std::vector<float> Data{};
        BufferLayout Layout{};
        bool RetainData{};
    };
}
#endif //VERTEXBUFFERCREATEINFO_HH
