/**
 * OpenGLVertexArray.hh
 * Created by kate on 6/4/23.
 * */

// Third-Party Libraries
#include <GL/glew.h>

// Project Headers
#include <Utility/Common.hh>
#include <Core/Logger.hh>
#include <Core/Assert.hh>
#include <Renderer/OpenGL/OpenGLVertexArray.hh>
#include <Renderer/OpenGL/OpenGLVertexBuffer.hh>

namespace Mikoto {
    OpenGLVertexArray::OpenGLVertexArray(OpenGLVertexArray&& other) noexcept
        :   m_Id{ other.GetId() }, m_ValidId{ other.m_ValidId } { other.m_Id = 0; }

    auto OpenGLVertexArray::operator=(OpenGLVertexArray&& other) noexcept -> OpenGLVertexArray& {
        m_Id = other.GetId();
        other.m_Id = 0;
        return *this;
    }

    auto OpenGLVertexArray::UseVertexBuffer(const std::shared_ptr<VertexBuffer>& buffer) const -> void {
        MKT_ASSERT(buffer, "Vertex Buffer is NULL");
        MKT_ASSERT(!buffer->IsEmpty(), "Vertex Buffer is empty");
        Bind();
        std::dynamic_pointer_cast<OpenGLVertexBuffer>(buffer)->Bind();

        UInt32_T attributeIndex{};
        for (const auto& bufferElement : buffer->GetBufferLayout()) {
            glEnableVertexAttribArray(attributeIndex);

            Int32_T size{ static_cast<Int32_T>(bufferElement.GetAttributeCount()) };
            GLenum dataType{ bufferElement.GetOpenGLAttributeDataType() };
            GLenum normalized{ static_cast<GLenum>(!bufferElement.IsNormalized() ? GL_FALSE : GL_TRUE) };
            GLsizei stride{ static_cast<GLsizei>(buffer->GetBufferLayout().GetStride()) };
            const void* pointer{ reinterpret_cast<const void*>(bufferElement.GetOffset()) };

            glVertexAttribPointer(attributeIndex, size, dataType, normalized, stride, pointer);
            ++attributeIndex;
        }
    }


}