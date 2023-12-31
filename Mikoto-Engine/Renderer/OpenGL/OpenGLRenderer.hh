/**
 * OpenGLRenderer.hh
 * Created by kate on 6/6/23.
 * */

#ifndef MIKOTO_OPENGL_RENDERER_HH
#define MIKOTO_OPENGL_RENDERER_HH

// C++ Standard Library
#include <memory>

// Third-Party Library
#include "glm/glm.hpp"

// Project Headers
#include "Common/Common.hh"
#include "OpenGLDefaultMaterial.hh"
#include "OpenGLFrameBuffer.hh"
#include "OpenGLIndexBuffer.hh"
#include "OpenGLVertexArray.hh"
#include "OpenGLVertexBuffer.hh"
#include "Renderer/Buffers/IndexBuffer.hh"
#include "Renderer/Buffers/VertexBuffer.hh"
#include "Renderer/RendererBackend.hh"

namespace Mikoto {
    /**
     * There's no need to have multiple VAOs, we can just use a single one and setup the attributes properly before
     * a draw call with glEnableVertexAttribArray() && glVertexAttribPointer().
     *
     * Currently we are using CORE_OPENGL_PROFILE which is specified when we create an OpenGL context
     * with the OpenGLContext class therefore OpenGL requires that we use a VAO so it knows what to do with
     * our vertex inputs. If we fail to bind a VAO, OpenGL will most likely refuse to draw anything.
     *
     * See: https://learnopengl.com/Getting-started/Hello-Triangle
     * A vertex array object (also known as VAO) can be bound just like a vertex buffer
     * object and any subsequent vertex attribute calls from that point on will be stored
     * inside the VAO. This has the advantage that when configuring vertex attribute pointers
     * you only have to make those calls once and whenever we want to draw the object, we can just
     * bind the corresponding VAO. This makes switching between different vertex data and attribute
     * configurations as easy as binding a different VAO. All the state we just set is stored inside the VAO.
     *
     * In compatibility mode, OpenGL already offers a default Vertex Array.
     * See: https://www.khronos.org/opengl/wiki/Vertex_Specification
     * */
    class OpenGLRenderer : public RendererBackend {
    public:
        explicit OpenGLRenderer() = default;

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto EnableWireframeMode() -> void override;
        auto DisableWireframeMode() -> void override;

        auto QueueForDrawing(std::shared_ptr<DrawData> &&) -> void override;
        auto Draw() -> void override;
        auto Flush() -> void override;
        auto SetClearColor(const glm::vec4& color) -> void override;
        auto SetClearColor(float red, float green, float blue, float alpha) -> void override;
        auto SetViewport(float x, float y, float width, float height) -> void override;

        auto GetColorAttachment() -> OpenGLFrameBuffer& { return m_DefaultFrameBuffer; }

        MKT_NODISCARD auto GetDefaultFrameBuffer() -> OpenGLFrameBuffer& { return m_DefaultFrameBuffer; }

        ~OpenGLRenderer() override = default;

    public:
        DISABLE_COPY_AND_MOVE_FOR(OpenGLRenderer);

    private:
        auto CreateFrameBuffers() -> void;
        auto DrawIndexed(const OpenGLVertexBuffer &vertexBuffer, const OpenGLIndexBuffer &indexBuffer) -> void;

    private:
        OpenGLVertexArray m_VertexArray{};
        OpenGLFrameBuffer m_DefaultFrameBuffer{};
        std::vector<DrawData> m_DrawQueue{};
        OpenGLDefaultMaterial* m_CurrentDefaultMaterial{};
    };
}


#endif // MIKOTO_OPENGL_RENDERER_HH
