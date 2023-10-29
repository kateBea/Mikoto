/**
 * OpenGLVertexBuffer.hh
 * Created by kate on 6/4/23.
 * */

#ifndef MIKOTO_OPENGL_VERTEX_BUFFER_HH
#define MIKOTO_OPENGL_VERTEX_BUFFER_HH

// C++ Standard Library
#include <vector>
#include <cstdint>

// Third-Party Libraries
#include "GL/glew.h"

// Project Libraries
#include "Common/Common.hh"
#include "Renderer/Buffers/VertexBuffer.hh"

namespace Mikoto {
    class OpenGLVertexBuffer : public VertexBuffer {
    public:
        explicit OpenGLVertexBuffer() = default;

        /**
         * Creates a new Vertex buffer and initializes it with the data
         * from indices. If no data is provided simply creates a new index
         * buffer object with a valid id
         * @param vertices buffer containing all the vertices
         * */
        explicit OpenGLVertexBuffer(VertexBufferCreateInfo&& createInfo, GLenum usage = GL_STATIC_DRAW);

        /**
         * Move constructor. If this operation is successful
         * other becomes invalid and using it may result in undefined behaviour
         * @param other moved from Vertex buffer
         * */
        OpenGLVertexBuffer(OpenGLVertexBuffer&& other) noexcept;

        /**
         * Move constructor assignment. If this operation is successful
         * other becomes invalid and using it may result in undefined behaviour
         * @param other moved from Vertex buffer
         * */
        OpenGLVertexBuffer & operator=(OpenGLVertexBuffer && other) noexcept;

        /**
         * Mark this Vertex buffer as current
         * */
        auto Bind() const -> void { glBindBuffer(GL_ARRAY_BUFFER, GetID()); }

        MKT_NODISCARD auto GetID() const -> UInt32_T { return m_Id; }

        /**
         * Releases the currently bound Vertex buffer object.
         * NOTE: Buffer set to zero effectively unbinds any buffer object
         * previously bound, and restores client memory usage for that buffer object
         * target (if supported for that target). See: https://docs.gl/gl4/glBindBuffer
         * */
        auto Unbind() const -> void { glBindBuffer(GL_ARRAY_BUFFER, 0); }

        auto Upload(const std::vector<float>& vertices, GLenum usage = GL_STATIC_DRAW) -> void;

        /**
         * Releases resources from this Vertex buffer
         * */
        ~OpenGLVertexBuffer() override { glDeleteBuffers(1, &m_Id); }

    public:
        // Forbidden operations
        OpenGLVertexBuffer(const OpenGLVertexBuffer & other) = delete;
        auto operator=(const OpenGLVertexBuffer & other) -> OpenGLVertexBuffer & = delete;

    private:
        /**
         * Tells whether this VAO holds a valid OpenGL shader program id.
         * For internal usage for now mainly
         * */
        bool m_ValidId{};
        UInt32_T m_Id{};
    };
}

#endif	// MIKOTO_OPENGL_VERTEX_BUFFER_HH