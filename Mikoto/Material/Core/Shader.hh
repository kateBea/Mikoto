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
#include <Common/Common.hh>
#include <STL/Utility/Types.hh>
#include <Models/Enums.hh>

namespace Mikoto {


    class Shader {
    public:
        /**
         * Returns the stage of this shader
         * @returns this shader's stage
         * */
        MKT_NODISCARD auto GetStage() -> ShaderStage { return m_Stage; }


        /**
         * Returns an error message indicating the type of shader
         * This is a helper function for showing compilation status on Shader::compile()
         * @param type type of shader
         * */
        MKT_NODISCARD static constexpr auto GetShaderTypeStr(ShaderStage type) -> std::string_view {
            switch (type) {
                case ShaderStage::SHADER_VERTEX_STAGE: return "VERTEX_STAGE";
                case ShaderStage::SHADER_FRAGMENT_STAGE:  return "FRAGMENT_STAGE";
                case ShaderStage::SHADER_GEOMETRY_STAGE:  return "GEOMETRY_STAGE";
                case ShaderStage::SHADER_TESSELATION_STAGE:  return "TESSELATION_STAGE";
                default: return "Unknown type of shader";
            }
        }


        /**
         * Destroy this shader
         * */
        virtual ~Shader() = default;

    protected:
        /**
         * Default constructs this shader
         * @param stage tells the stage this shader is part of
         * */
        explicit Shader(ShaderStage stage = ShaderStage::SHADER_VERTEX_STAGE) : m_Stage{ stage } {};

    protected:

        ShaderStage m_Stage{};
    };
}

#endif // MIKOTO_BASE_SHADER_HH
