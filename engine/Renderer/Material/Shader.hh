/**
 * Shader.hh
 * Created by kate on 6/8/23.
 * */

#ifndef MIKOTO_BASE_SHADER_HH
#define MIKOTO_BASE_SHADER_HH

// C++ Standard Library
#include <memory>
#include <string_view>

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
        explicit Shader() = default;
        virtual ~Shader() = default;

        static auto Create(const Path_T& vertStage, const Path_T& pixelStage) -> std::shared_ptr<Shader>;

        /**
         * Returns an error message indicating the type of shader
         * This is a helper function for showing compilation status on Shader::compile()
         * @param type type of shader
         * */
        static constexpr auto GetShaderTypeStr(ShaderStage type) -> std::string_view {
            switch (type) {
                case ShaderStage::VERTEX_STAGE: return "VERTEX_STAGE";
                case ShaderStage::FRAGMENT_STAGE:  return "FRAGMENT_STAGE";
                default: return "Unknown type of shader";
            }
        }
    };
}

#endif // MIKOTO_BASE_SHADER_HH
