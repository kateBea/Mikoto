/**
 * Shader.hh
 * Created by kate on 6/8/23.
 * */

#ifndef MIKOTO_BASE_SHADER_HH
#define MIKOTO_BASE_SHADER_HH

// C++ Standard Library
#include <memory>

// Project Headers
#include <Utility/Common.hh>

namespace Mikoto {
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


#endif // MIKOTO_BASE_SHADER_HH
