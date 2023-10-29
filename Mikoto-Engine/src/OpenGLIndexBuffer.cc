// C++ Standard Library
#include <vector>
#include <stdexcept>

// Third-Party Libraries
#include "GL/glew.h"

// Project Headers
#include "../Common/Types.hh"
#include "Core/Logger.hh"
#include "Renderer/OpenGL/OpenGLIndexBuffer.hh"

namespace Mikoto {
    OpenGLIndexBuffer::OpenGLIndexBuffer(const std::vector<UInt32_T>& indices, GLbitfield flags) {
        glCreateBuffers(1, &m_ObjectID);

        if (m_ObjectID == 0)
            throw std::runtime_error("Could not create OpenGL Index Buffer, identifier is 0!");

        Upload(indices, flags);
    }

    auto OpenGLIndexBuffer::Upload(const std::vector<UInt32_T>& indices, GLbitfield flags) -> void {
        if (indices.empty()) {
            glDeleteBuffers(1, &m_ObjectID);
            throw std::runtime_error("Vector of indices is empty");
        }

        m_Count = indices.size();
        m_Size = m_Count * sizeof(UInt32_T);
        m_DataType = GL_UNSIGNED_INT; // adjust in the future for more flexibility
        glNamedBufferStorage(m_ObjectID, static_cast<GLsizeiptr>(m_Count * sizeof(UInt32_T)), indices.data(), flags);
    }

    OpenGLIndexBuffer::OpenGLIndexBuffer(OpenGLIndexBuffer&& other) noexcept
        :   IndexBuffer{ other.GetCount(), other.GetSize() }
    {
        other.m_ObjectID = 0;
        other.m_Count = 0;
        other.m_Size = 0;
    }

    auto OpenGLIndexBuffer::operator=(OpenGLIndexBuffer&& other) noexcept -> OpenGLIndexBuffer& {
        m_ObjectID = other.GetID();
        m_Count = other.GetCount();
        m_Size = other.GetSize();

        other.m_ObjectID = 0;
        other.m_Count = 0;
        other.m_Size = 0;
        return *this;
    }
}