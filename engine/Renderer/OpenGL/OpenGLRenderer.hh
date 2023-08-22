/**
 * OpenGLRenderer.hh
 * Created by kate on 6/6/23.
 * */

#ifndef MIKOTO_OPENGL_RENDERER_HH
#define MIKOTO_OPENGL_RENDERER_HH

// C++ Standard Library
#include <memory>

// Third-Party Library
#include <glm/vec4.hpp>

// Project Headers
#include <Utility/Common.hh>
#include <Renderer/RendererAPI.hh>
#include <Renderer/Buffers/IndexBuffer.hh>
#include <Renderer/Buffers/VertexBuffer.hh>
#include <Renderer/OpenGL/OpenGLFrameBuffer.hh>
#include <Renderer/OpenGL/OpenGLVertexArray.hh>
#include <Renderer/OpenGL/OpenGLDefaultMaterial.hh>

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
    class OpenGLRenderer : public RendererAPI {
    public:
        /*************************************************************
        * PUBLIC MEMBERS
        * ***********************************************************/
        explicit OpenGLRenderer() = default;

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto EnableWireframeMode() -> void override;
        auto DisableWireframeMode() -> void override;

        auto Draw(const DrawData & data) -> void override;
        auto SetClearColor(const glm::vec4& color) -> void override;
        auto SetClearColor(float red, float green, float blue, float alpha) -> void override;
        auto SetViewport(UInt32_T x, UInt32_T y, UInt32_T width, UInt32_T height) -> void override;

        auto GetColorAttachment() -> OpenGLFrameBuffer& { return m_DefaultFrameBuffer; }

        auto OnEvent(Event& event) -> void override;

        MKT_NODISCARD auto GetDefaultFrameBuffer() -> OpenGLFrameBuffer& { return m_DefaultFrameBuffer; }

        ~OpenGLRenderer() override = default;

    public:
        /*************************************************************
        * DELETED OPERATIONS
        * ***********************************************************/
        OpenGLRenderer(const OpenGLRenderer&) = delete;
        auto operator=(const OpenGLRenderer&) -> OpenGLRenderer& = delete;

        OpenGLRenderer(OpenGLRenderer&&) = delete;
        auto operator=(OpenGLRenderer&&) -> OpenGLRenderer& = delete;

    private:
        /*************************************************************
        * HELPERS
        * ***********************************************************/
        auto CreateFrameBuffers() -> void;
        auto DrawIndexed(const std::shared_ptr<VertexBuffer> &vertexBuffer, const std::shared_ptr<IndexBuffer> &indexBuffer) -> void;

    private:
        /*************************************************************
        * MEMBER VARIABLES
        * ***********************************************************/
        OpenGLVertexArray m_VertexArray{};
        OpenGLFrameBuffer m_DefaultFrameBuffer{};
        std::shared_ptr<OpenGLDefaultMaterial> m_CurrentDefaultMaterial{};
    };
}


#endif // MIKOTO_OPENGL_RENDERER_HH
