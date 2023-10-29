/**
 * Shader.cc
 * Created by kate on 6/4/23.
 * */

// C++ Standard Libraries
#include <filesystem>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <string>

// Third-Party Libraries
#include "GL/glew.h"
#include "fmt/core.h"
#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"

// Project Libraries
#include "../Common/Common.hh"
#include "Core/Logger.hh"
#include "Renderer/OpenGL/OpenGLShader.hh"


namespace Mikoto {
    OpenGLShader::OpenGLShader(const Path_T& vertexSourceDir, const Path_T& fragmentSourceDir) {
        m_Id = glCreateProgram();
        if (m_Id == 0)
            throw std::runtime_error("Error when creating shader program");

        m_ValidId = true;
        Upload(vertexSourceDir, fragmentSourceDir);
    }

    auto OpenGLShader::Upload(const Path_T& vShaderPath, const Path_T& fShaderPath) -> void {
        if (!m_ValidId) {
            m_Id = glCreateProgram();
            if (m_Id == 0)
                throw std::runtime_error("Error when creating shader program");
        }

        auto vertexShaderData{ GetFileData(vShaderPath) };
        auto fragmentShaderData{ GetFileData(fShaderPath) };

        Build(vertexShaderData.c_str(), fragmentShaderData.c_str());
    }

    auto OpenGLShader::Compile(const char* content, GLenum shaderType) -> UInt32_T {
        UInt32_T shaderId{ glCreateShader(shaderType) };
        glShaderSource(shaderId, 1, &content, nullptr);
        glCompileShader(shaderId);
        return shaderId;
    }

    auto OpenGLShader::Build(const char* vShader, const char* fShader) const -> void {
        UInt32_T vertexShaderID{ Compile(vShader, GL_VERTEX_SHADER) };
        ShowShaderStatus(vertexShaderID, ShaderStage::SHADER_VERTEX_STAGE, GL_COMPILE_STATUS);

        UInt32_T pixelShaderID{ Compile(fShader, GL_FRAGMENT_SHADER) };
        ShowShaderStatus(pixelShaderID, ShaderStage::SHADER_FRAGMENT_STAGE, GL_COMPILE_STATUS);

        // Create and link program against compiled Shader binaries
        glAttachShader(GetProgram(), vertexShaderID);
        glAttachShader(GetProgram(), pixelShaderID);
        glLinkProgram(GetProgram());

        ShowProgramStatus(GL_LINK_STATUS);

        glDetachShader(GetProgram(), vertexShaderID);
        glDetachShader(GetProgram(), pixelShaderID);
        glDeleteShader(vertexShaderID);
        glDeleteShader(pixelShaderID);
    }

    auto OpenGLShader::SetUniformBool(std::string_view name, bool value) const -> void {
        Bind();
        auto ret{ glGetUniformLocation(GetProgram(), name.data()) };
        if (ret == -1)
            MKT_CORE_LOGGER_ERROR("Error: [{}] is not a valid uniform name for this program shader", name);
        else
            glUniform1i(ret, static_cast<Int32_T>(value));
    }

    auto OpenGLShader::SetUniformInt(std::string_view name, Int32_T value) const -> void {
        Bind();
        auto ret{ glGetUniformLocation(GetProgram(), name.data()) };
        if (ret == -1)
            MKT_CORE_LOGGER_ERROR("Error: [{}] is not a valid uniform name for this program shader", name);
        else
            glUniform1i(ret, value);
    }

    auto OpenGLShader::SetUniformFloat(std::string_view name, float value) const -> void {
        Bind();
        auto ret{ glGetUniformLocation(GetProgram(), name.data()) };
        if (ret == -1)
            MKT_CORE_LOGGER_ERROR("Error: [{}] is not a valid uniform name for this program shader", name);
        else
            glUniform1f(ret, value);
    }

    auto OpenGLShader::ShowShaderStatus(UInt32_T objectId, ShaderStage type, GLenum status) -> void {
        Int32_T success{};

        glGetShaderiv(objectId, status, &success);
        switch (status) {
            case GL_COMPILE_STATUS:
                if (success == GL_FALSE) {
                    Int32_T length{};
                    glGetShaderiv(objectId, GL_INFO_LOG_LENGTH, &length);
                    std::string outStr(length, '\0');

                    glGetShaderInfoLog(objectId, length, &length, outStr.data());
                    MKT_CORE_LOGGER_ERROR("Error on {} shader compilation", GetShaderTypeStr(type));
                    MKT_COLOR_PRINT_FORMATTED(MKT_FMT_COLOR_RED, "\n{}", outStr);
                }
                else
                    MKT_COLOR_PRINT_FORMATTED(MKT_FMT_COLOR_LIME_GREEN, "Shader compilation successful. Type: {}\n", GetShaderTypeStr(type));
                break;
        }
    }

    auto OpenGLShader::ShowProgramStatus(GLenum status) const -> void {
        std::string outStr(1024, '\0');
        Int32_T success{};

        glGetProgramiv(m_Id, status, &success);
        switch (status) {
            case GL_LINK_STATUS:
                if (success == GL_FALSE) {
                    glGetProgramInfoLog(m_Id, outStr.size(), nullptr, outStr.data());
                    MKT_CORE_LOGGER_ERROR("Error on shader program Linking");
                    MKT_COLOR_PRINT_FORMATTED(MKT_FMT_COLOR_RED, "\n{}", outStr);
                }
                else {
                    MKT_CORE_LOGGER_INFO("Shader program linking successful");
                }

                break;
        }
    }

    auto OpenGLShader::SetUniformMat4(std::string_view name, const glm::mat4& value) const -> void {
        Bind();
        auto ret{ glGetUniformLocation(GetProgram(), name.data()) };
        if (ret == -1) {
            MKT_CORE_LOGGER_ERROR("Error: [{}] is not a valid uniform name for this program shader", name);
        }
        else {
            /*
             * If transpose is GL_FALSE, each matrix is assumed to be supplied in column major order.
             * If transpose is GL_TRUE, each matrix is assumed to be supplied in row major order.
             * The count argument indicates the number of matrices to be passed. A count of 1
             * should be used if modifying the value of a single matrix, and a count greater
             * than 1 can be used to modify an array of matrices.
             * from: https://docs.gl/gl4/glUniform
             *
             * we pass GL_FALSE because glm::mat4 has each row stored contiguously in memory by default,
             * meaning the elements of the first row are stored first, followed by the
             * elements of the second row, and so on.
             * */
            glUniformMatrix4fv(ret, 1, GL_FALSE, glm::value_ptr(value));
        }
    }

    auto OpenGLShader::SetUniformVec3(std::string_view name, const glm::vec3 &value) const -> void {
        Bind();
        auto ret{ glGetUniformLocation(GetProgram(), name.data()) };
        if (ret == -1)
            MKT_CORE_LOGGER_ERROR("Error: [{}] is not a valid uniform name for this program shader", name);
        else {
            // we pass we 1 because the shader uniform is not expected to be an array
            glUniform3fv(ret, 1, glm::value_ptr(value));
        }
    }

    auto OpenGLShader::SetUniformVec4(std::string_view name, const glm::vec4& value) const -> void {
        Bind();
        auto ret{ glGetUniformLocation(GetProgram(), name.data()) };

        if (ret == -1)
            MKT_CORE_LOGGER_ERROR("Error: [{}] is not a valid uniform name for this program shader", name);
        else {
            // we pass we 1 because the shader uniform is not expected to be an array
            glUniform4fv(ret, 1, glm::value_ptr(value));
        }
    }

    OpenGLShader::OpenGLShader(OpenGLShader &&other) noexcept
        :   m_Id{other.GetProgram() }
    {
        // Assign 0 so that it can be safely passed to glDeleteProgram()
        // when the destructor is called. We avoid deleting a valid program this way
        other.m_Id = 0;
    }

    auto OpenGLShader::operator=(OpenGLShader &&other) noexcept -> OpenGLShader & {
        m_Id = other.GetProgram();
        other.m_Id = 0;
        return *this;
    }

    auto OpenGLShader::SetUniformVec2(std::string_view name, const glm::vec2 &vec) const -> void {
        Bind();
        auto ret{ glGetUniformLocation(GetProgram(), name.data()) };

        if (ret == -1)
            MKT_CORE_LOGGER_ERROR("Error: [{}] is not a valid uniform name for this program shader", name);
        else {
            // we pass we 1 because the shader uniform is not expected to be an array
            glUniform2fv(ret, 1, glm::value_ptr(vec));
        }
    }

    auto OpenGLShader::SetUniformMat3(std::string_view name, const glm::mat3& value) const -> void {
        Bind();
        auto ret{ glGetUniformLocation(GetProgram(), name.data()) };

        if (ret == -1)
            MKT_CORE_LOGGER_ERROR("Error: [{}] is not a valid uniform name for this program shader", name);
        else {
            glUniformMatrix3fv(ret, 1, GL_FALSE, glm::value_ptr(value));
        }
    }
}