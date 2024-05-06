/**
 * OpenGLIndexBuffer.hh
 * Created by kate on 6/4/23.
 * */

#ifndef MIKOTO_OPENGL_INDEX_BUFFER_HH
#define MIKOTO_OPENGL_INDEX_BUFFER_HH

// C++ Standard Library
#include <vector>
#include <cstdint>

// Third-Party Libraries
#include "GL/glew.h"

// Project Headers
#include "Common/Common.hh"
#include "Common/Types.hh"
#include "Renderer/Buffers/IndexBuffer.hh"

namespace Mikoto {
    class OpenGLIndexBuffer : public IndexBuffer {
    public:
        /**
         * Constructs an OpenGL index buffer
         * */
        explicit OpenGLIndexBuffer() = default;

        /**
         * Move constructor
         * @param other moved from Vertex buffer
         * */
        OpenGLIndexBuffer(OpenGLIndexBuffer&& other) noexcept;

        /**
         * Move assigment
         * @param other moved from Vertex buffer
         * */
        auto operator=(OpenGLIndexBuffer&& other) noexcept -> OpenGLIndexBuffer&;

        /**
         * Creates a new index buffer and initializes it with the data from <code>indices</code>.
         * If no data is provided simply creates a new index buffer object with a valid id
         * @param indices buffer containing all the indices values
         * @param usage buffer usage
         * @throws std::runtime_error if a valid object could not be created
         * */
        explicit OpenGLIndexBuffer(const std::vector<UInt32_T>& indices, GLbitfield usage = GL_DYNAMIC_STORAGE_BIT);

        /**
         * Returns the object identifier of this index buffer
         * @returns this index buffer identifier
         * */
        MKT_NODISCARD auto GetID() const -> UInt32_T { return m_ObjectID; }

        /**
         * Returns the type of the data within this vertex buffer. For simplicity
         * sake now it going to be GL_UNSIGNED_INT for now
         * @returns data type of the indices
         * */
        MKT_NODISCARD auto GetBufferDataType() const -> Int32_T { return m_DataType; }

        /**
         * Mark this index buffer as current
         * */
        auto Bind() const -> void { glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GetID()); }

        /**
         * Releases the currently bound Vertex element buffer. Note that a call to this
         * function also unbinds any previously bound buffer (including vertex buffers)
         * @see https://docs.gl/gl4/glBindBuffer
         * */
        static auto Unbind() -> void { glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); }

        /**
         * Releases resources from this index buffer
         * */
        ~OpenGLIndexBuffer() override { glDeleteBuffers(1, &m_ObjectID); }

    public:
        // Forbidden operations
        OpenGLIndexBuffer(const OpenGLIndexBuffer&) = delete;
        auto operator=(const OpenGLIndexBuffer&) -> OpenGLIndexBuffer& = delete;

    private:
        /**
         * Creates and initializes a buffer object's immutable data store.
         * @param indices buffer containing all the indices values
         * @param usage buffer usage
         * @throws std::runtime_error if the list of indices is empty
         * */
        auto Upload(const std::vector<UInt32_T>& indices, GLbitfield flags = GL_DYNAMIC_STORAGE_BIT) -> void;

    private:
        UInt32_T m_ObjectID{};
        Int32_T m_DataType{};
    };
}

#endif // MIKOTO_OPENGL_INDEX_BUFFER_HH