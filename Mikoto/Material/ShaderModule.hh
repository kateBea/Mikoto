/**
 * ShaderModule.hh
 * Created by kate on 6/8/23.
 * */

#ifndef MIKOTO_BASE_SHADER_HH
#define MIKOTO_BASE_SHADER_HH

// C++ Standard Library

// Remember to pass in the stage name not a symbol name e.g., a variable
#define SHADER_STAGE_STR( STAGE_NAME ) #STAGE_NAME

// Project Headers
#include <Common/Common.hh>
#include <Renderer/DeviceObject.hh>

namespace Mikoto {

    /**
     * @class ShaderModule
     * @brief Represents a shader resource used in the graphics pipeline.
     *
     * This class encapsulates the properties of a shader, including its stage and the file it was loaded from.
     * Shaders are an essential part of the graphics pipeline and are used for tasks like vertex transformation,
     * pixel coloring, and compute tasks.
     */
    class ShaderModule : public DeviceObject {
    public:

        /**
         * @brief Gets the file associated with the shader.
         *
         * @return A pointer to the file from which the shader was loaded.
         */
        MKT_NODISCARD auto GetContents() const -> CStr_T {
            return m_Contents.c_str();
        }

        /**
         * @brief Gets the file associated with the shader.
         *
         * @return A pointer to the file from which the shader was loaded.
         */
        MKT_NODISCARD auto GetContentSize() const -> Size_T {
            return m_Contents.size();
        }

        /**
         * @brief Gets the file associated with the shader.
         *
         * @return A pointer to the file from which the shader was loaded.
         */
        MKT_NODISCARD auto HasContents() const -> Size_T {
            return !m_Contents.empty();
        }

        /**
        * @brief Gets the stage of the shader.
        *
        * @return The stage of the shader (vertex, fragment, or compute).
        */
        MKT_NODISCARD auto GetStage() const -> ShaderStage {
            return m_Stage;
        }

        /**
         * @brief Destructor for the Shader class.
         *
         * Ensures proper cleanup of resources when the shader is destroyed.
         */
        ~ShaderModule() override = default;

    protected:
        /**
         * @brief Constructs a shader with the specified stage and file.
         *
         * Initializes the shader with the stage (vertex, fragment, compute) and the file from which it is loaded.
         *
         * @param stage The stage of the shader (vertex, fragment, or compute).
         * @param file A pointer to the file from which the shader is loaded.
         */
        explicit ShaderModule( const ShaderStage stage, std::string_view contents)
            : m_Contents{ contents }, m_Stage{ stage } {}

    protected:
        std::string m_Contents{};
        ShaderStage m_Stage{ ShaderStage::VERTEX_STAGE };
    };

}

#endif// MIKOTO_BASE_SHADER_HH
