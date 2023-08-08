//
// Created by kate on 6/8/23.
//

#ifndef KATE_ENGINE_BASE_SHADER_HH
#define KATE_ENGINE_BASE_SHADER_HH

#include <filesystem>

#include <glm/glm.hpp>

#include <Utility/Common.hh>

namespace kaTe {
    enum ShaderStage {
        NONE,
        VERTEX_STAGE = BIT_SET(1),
        FRAGMENT_STAGE = BIT_SET(2),
        COUNT,
    };

    class Shader {
    public:
        Shader() = default;
        virtual ~Shader() = default;

        static auto CreateShader(const Path_T& vertStage, const Path_T& pixelStage) -> std::shared_ptr<Shader>;
    };
}


#endif//KATE_ENGINE_BASE_SHADER_HH
