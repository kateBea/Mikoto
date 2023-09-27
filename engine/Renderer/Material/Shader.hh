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
#include <Utility/Types.hh>
#include <Utility/Common.hh>

namespace Mikoto {
    enum ShaderStage {
        SHADER_VERTEX_STAGE         = BIT_SET(1),
        SHADER_FRAGMENT_STAGE       = BIT_SET(2),
        SHADER_GEOMETRY_STAGE       = BIT_SET(3),
        SHADER_TESSELATION_STAGE    = BIT_SET(4),
    };

    class Shader {
    public:
        /**
         * Returns the stage of this shader
         * @returns this shader's stage
         * */
        MKT_NODISCARD auto GetStage() -> ShaderStage { return m_Stage; }

        /**
         * Creates a Shader module from the given paths.
         * @param vertStage path to the file containing the source code of the vertex shader
         * @param pixelStage path to the file containing the source code of the fragment/pixel shader
         * @returns pointer to newly created shader module
         * @deprecated this function was first created for easy usage with the OpenGL backend
         * */
        MKT_NODISCARD static auto Create(const Path_T& vertStage, const Path_T& pixelStage) -> std::shared_ptr<Shader>;

        /**
         * Creates a Shader module for the specified stage using the source code from the given path
         * @param src path to the file containing the source code of the shader
         * @param stage shader module stage
         * @returns pointer to newly created shader module
         * */
        MKT_NODISCARD static auto Create(const Path_T& src, ShaderStage stage) -> std::shared_ptr<Shader>;

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
        /*************************************************************
         * DATA MEMBERS
         * ***********************************************************/
        ShaderStage m_Stage{};
    };
}

#endif // MIKOTO_BASE_SHADER_HH
