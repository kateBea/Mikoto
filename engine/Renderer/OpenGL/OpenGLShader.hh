/**
 * Shader.hh
 * Created by kate on 6/4/23.
 */

#ifndef MIKOTO_OPENGL_SHADER_HH
#define MIKOTO_OPENGL_SHADER_HH

// C++ Standard Library
#include <string_view>
#include <filesystem>

// Third-Party Libraries
#include <GL/glew.h>
#include <glm/glm.hpp>

// Project headers
#include <Utility/Common.hh>
#include <Renderer/Material/Shader.hh>

namespace Mikoto {
    class OpenGLShader : public Shader {
    public:
        /**
         * Default initialization for Shader. DOES NOT Create a valid shader program
         * */
        explicit OpenGLShader() = default;

        /**
         * Move constructor
         * */
        OpenGLShader(OpenGLShader && other) noexcept;

        /**
         * Move assignment
         * @return *this
         * */
        OpenGLShader& operator=(OpenGLShader && other) noexcept;

        /**
         * Construct Shader program from path to Vertex Shader source file directory
         * and pixel Shader source file directory
         * @param vertexSourceDir directory to the Vertex Shader source file
         * @param fragmentSourceDir directory to the pixel Shader source file
         * */
        OpenGLShader(const Path_T& vertexSourceDir, const Path_T& fragmentSourceDir);

        /**
         * Loads the shaders specified from paths
         * @param vShaderPath path to vertex shader path
         * @param fShaderPath path to pixel/fragment shader path
         * @throws std::runtime_error exception if any of the shader files could not be opened
         * */
        auto Upload(const Path_T& vShaderPath, const Path_T& fShaderPath) -> void;

        /**
         * Use this Shader program
         * */
        auto Bind() const -> void { glUseProgram(m_Id); }

        /**
         * Unbinds any currently bound shader program
         * */
        static auto Unbind() -> void { glUseProgram(0); }

        /**
         * Get Shader program ID
         * @return Shader program ID
         * */
        MKT_NODISCARD auto GetProgram() const -> UInt32_T { return m_Id; }

        /**
         * Sets the given boolean value to the uniform identified by "name",
         * it has no effect if this Shader has no uniform with given name. This function
         * ensures this shader is being used before passing the data to the shader uniform, so
         * a previous call to Shader::use() is unnecessary
         * @param name name of the uniform
         * @param value value to be set
         * */
        auto SetUniformBool(std::string_view name, bool value) const -> void;

        /**
         * Sets the given integer value to the uniform identified by "name",
         * it has no effect if this Shader has no uniform with given identifier. This function
         * ensures this shader is being used before passing the data to the shader uniform, so
         * a previous call to Shader::use() is unnecessary
         * @param name name of the uniform.
         * @param value value to be set
         * */
        auto SetUniformInt(std::string_view name, Int32_T value) const -> void;

        /**
         * Sets the given floating value to the uniform identified by "name",
         * it has no effect if this Shader has no uniform with given identifier. This function
         * ensures this shader is being used before passing the data to the shader uniform, so
         * a previous call to Shader::use() is unnecessary
         * @param name name of the uniform
         * @param value value to be set
         * */
        auto SetUniformFloat(std::string_view name, float value) const -> void;

        /**
         * Sets the given 3D vector to the uniform specified by the name. This function
         * ensures this shader is being used before passing the data to the shader uniform, so
         * a previous call to Shader::use() is unnecessary
         * @param name name of the uniform
         * @param vec value for the uniform
         * */
        auto SetUniformVec2(std::string_view name, const glm::vec2& vec) const -> void;

        /**
         * Sets the given 3D vector to the uniform specified by the name. This function
         * ensures this shader is being used before passing the data to the shader uniform, so
         * a previous call to Shader::use() is unnecessary
         * @param name name of the uniform
         * @param vec value for the uniform
         * */
        auto SetUniformVec3(std::string_view name, const glm::vec3& vec) const -> void;

        /**
         * Sets the given 4D vector to the uniform specified by the name. This function
         * ensures this shader is being used before passing the data to the shader uniform, so
         * a previous call to Shader::use() is unnecessary
         * @param name name of the uniform
         * @param vec value for the uniform
         * */
        auto SetUniformVec4(std::string_view name, const glm::vec4& vec) const -> void;

        /**
         * Sets the given matrix to the uniform matrix specified by the name. This function
         * ensures this shader is being used before passing the data to the shader uniform, so
         * a previous call to Shader::use() is unnecessary
         * @param name name of the uniform
         * @param mat value for the uniform
         * */
        auto SetUniformMat3(std::string_view name, const glm::mat3& mat) const -> void;

        /**
         * Sets the given matrix to the uniform matrix specified by the name. This function
         * ensures this shader is being used before passing the data to the shader uniform, so
         * a previous call to Shader::use() is unnecessary
         * @param name name of the uniform
         * @param mat value for the uniform
         * */
        auto SetUniformMat4(std::string_view name, const glm::mat4& mat) const -> void;

        ~OpenGLShader() override { glDeleteProgram(GetProgram()); }
    public:
        // Forbidden operations
        OpenGLShader(const OpenGLShader&) = delete;
        OpenGLShader & operator=(const OpenGLShader&) = delete;

        /**
         * Compiles the given shader and returns its corresponding identifier
         * @param content file contents of the shader
         * @param shaderType type of shader to be compiled
         * @return identifier of the compiled shader, 0 if there was an error
         * */
        static auto Compile(const char* content, GLenum shaderType) -> UInt32_T;

        /**
         * Compiles and links the given shaders to this program shader.
         * @param vShader file contents of the vertex shader
         * @param fShader file contents of the fragment shader
         * */
        auto Build(const char* vShader, const char* fShader) const -> void;

        /**
         * Helper function to retrieve Shader status
         * @param objectId identifier of the shader object
         * @param str error message indicating the type of shader
         * @param status
         * */
        static auto ShowShaderStatus(UInt32_T objectId, ShaderStage type, GLenum status) -> void;

        /**
         * Helper function to retrieve program status
         * @param objectId identifier of the program object
         * @param str error message indicating the type of shader
         * @param status
         * */
        auto ShowProgramStatus(GLenum status) const -> void;

    private:
        /*************************************************************
        * MEMBER VARIABLES
        * ***********************************************************/

        /**
         * Identifier of this Shader program
         * */
        UInt32_T m_Id{};

        /**
         * Tells whether this shader holds a valid OpenGL shader program id.
         * For internal usage for now mainly
         * */
         bool m_ValidId{};

         // Could somehow store all uniform locations when the shader program gets linked
         // to avoid multiple calls to getUniformLocation for performance purposes
         // std::unordered_map<std::string, UInt32_T> m_UniformLocations{}; where the string is the name and the integer is the location
    };
}

#endif // MIKOTO_OPENGL_SHADER_HH