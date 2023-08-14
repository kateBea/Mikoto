/**
 * kateOpenGLVao.hh
 * Created by kate on 6/4/23.
 * */

#ifndef KATE_ENGINE_VAO_HH
#define KATE_ENGINE_VAO_HH

// C++ Standard Library
#include <cstdint>
#include <array>
#include <memory>
#include <deque>


// Third-Party Libraries
#include <GL/glew.h>

// Project Libraries
#include <Utility/Common.hh>

#include <Renderer/Buffers/VertexBuffer.hh>

namespace Mikoto {
    class OpenGLVertexArray {
    public:
        explicit OpenGLVertexArray() { glCreateVertexArrays(1, &m_Id); m_ValidId = m_Id != 0; }

        /**
         * Move constructor
         * @param other other Vao from which we move data
         * */
        OpenGLVertexArray(OpenGLVertexArray && other) noexcept;

        /**
         * Move constructor assignment
         * @param other other Vao from which we move data
         * @retunr *this
         * */
        auto operator=(OpenGLVertexArray && other) noexcept -> OpenGLVertexArray&;

        /**
         * Returns the identifier of this Vertex Array Object
         * @return identifier of this vao
         * */
        KT_NODISCARD auto GetId() const -> UInt32_T { return m_Id; }

        /**
         * Mark this Vertex Array Object as current
         * */
        auto Bind() const -> void { glBindVertexArray(GetId()); }

        /**
         * Unbinds the currently bound Vertex Array Object
         * */
        static auto Unbind() -> void { glBindVertexArray(0); }

        auto UseVertexBuffer(const std::shared_ptr<VertexBuffer>& buffer) const -> void;

        ~OpenGLVertexArray() { glDeleteVertexArrays(1, &m_Id); }
    public:
        // Forbidden operations
        OpenGLVertexArray(const OpenGLVertexArray & other) = delete;
        auto operator=(const OpenGLVertexArray & other) -> OpenGLVertexArray & = delete;
    private:
        UInt32_T m_Id{ 0 };

        /**
         * Tells whether this VAO holds a valid OpenGL shader program id.
         * For internal usage for now mainly
         * */
        bool m_ValidId{ false };
    };
}

#endif	// KATE_ENGINE_VAO_HH