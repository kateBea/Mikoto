/**
* kateOpenGLVeo.hh
* Created by kate on 6/4/23.
* */

#ifndef KATE_ENGINE_VIB_HH
#define KATE_ENGINE_VIB_HH

// C++ Standard Library
#include <vector>
#include <cstdint>

// Third-Party Libraries
#include <GL/glew.h>

// Project Headers
#include <Utility/Common.hh>
#include <Renderer/Buffers/IndexBuffer.hh>

namespace Mikoto {
    class OpenGLIndexBuffer : public IndexBuffer {
    public:
        explicit OpenGLIndexBuffer() = default;
        /**
         * Creates a new Vertex index buffer and initializes it with the data
         * from indices. If no data is provided simply creates a new index buffer object with a valid id
         * @param indices buffer containing all the indices values
         * */
        explicit OpenGLIndexBuffer(const std::vector<UInt32_T> &indices, GLbitfield usage = GL_DYNAMIC_STORAGE_BIT);

        KT_NODISCARD auto GetID() const -> UInt32_T { return m_Id; }

        /**
         * Mark this Vertex index buffer as current
         * */
        auto Bind() const -> void { glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GetID()); }

        /**
         * Releases the currently bound Vertex element buffer.
         * NOTE: Buffer set to zero effectively unbinds any buffer object
         * previously bound, and restores client memory usage for that buffer object
         * target (if supported for that target). See: https://docs.gl/gl4/glBindBuffer
         * */
        auto Unbind() const -> void { glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); }

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
         * Creates and initializes a buffer object's immutable data store. Firstly, if this OpenGL index buffer
         * does not contain a valid identifier, it is created.
         * */
        auto Upload(const std::vector<UInt32_T>& indices, GLbitfield flags = GL_DYNAMIC_STORAGE_BIT) -> void;

        /**
         * Releases resources from this Vertex index buffer
         * */
        ~OpenGLIndexBuffer() override { glDeleteBuffers(1, &m_Id); }

    public:
        // Forbidden operations
        OpenGLIndexBuffer(const OpenGLIndexBuffer&) = delete;
        auto operator=(const OpenGLIndexBuffer&) -> OpenGLIndexBuffer& = delete;

    private:
        UInt32_T m_Id{};
        /**
         * Tells whether this VAO holds a valid OpenGL shader program id.
         * For internal usage for now mainly
         * */
        bool m_ValidId{};
    };
}

#endif // KATE_ENGINE_VIB_HH