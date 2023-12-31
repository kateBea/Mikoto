/**
 * OpenGLVertexArray.hh
 * Created by kate on 6/4/23.
 * */

// Third-Party Libraries
#include "GL/glew.h"

// Project Headers
#include "../Common/Common.hh"
#include "Core/Assert.hh"
#include "Core/Logger.hh"
#include "Renderer/OpenGL/OpenGLVertexArray.hh"
#include "Renderer/OpenGL/OpenGLVertexBuffer.hh"

namespace Mikoto {
    OpenGLVertexArray::OpenGLVertexArray(OpenGLVertexArray&& other) noexcept
        :   m_Id{ other.GetId() }, m_ValidId{ other.m_ValidId } { other.m_Id = 0; }

    auto OpenGLVertexArray::operator=(OpenGLVertexArray&& other) noexcept -> OpenGLVertexArray& {
        m_Id = other.GetId();
        other.m_Id = 0;
        return *this;
    }

    auto OpenGLVertexArray::Use(const OpenGLVertexBuffer& buffer) const -> void {
#if !defined(NDEBUG)
        if (buffer.IsEmpty())
            MKT_CORE_LOGGER_WARN("OpenGL vertex buffer with ID ({}) is empty", buffer.GetID());
#endif

        Bind();
        buffer.Bind();

        UInt32_T attributeIndex{};
        for (const auto& bufferElement : buffer.GetBufferLayout()) {
            glEnableVertexAttribArray(attributeIndex);

            Int32_T size{ static_cast<Int32_T>(bufferElement.GetAttributeCount()) };
            GLenum dataType{ bufferElement.GetOpenGLAttributeDataType() };
            GLenum normalized{ static_cast<GLenum>(!bufferElement.IsNormalized() ? GL_FALSE : GL_TRUE) };
            GLsizei stride{ static_cast<GLsizei>(buffer.GetBufferLayout().GetStride()) };
            const void* pointer{ reinterpret_cast<const void*>(bufferElement.GetOffset()) };

            glVertexAttribPointer(attributeIndex, size, dataType, normalized, stride, pointer);
            ++attributeIndex;
        }
    }


}