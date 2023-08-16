/**
 * OpenGLFrameBuffer.hh
 * Created by kate on 6/23/23.
 * */

#ifndef MIKOTO_OPENGL_FRAME_BUFFER_HH
#define MIKOTO_OPENGL_FRAME_BUFFER_HH

#include <Utility/Common.hh>

#include <Renderer/Buffers/FrameBuffer.hh>

namespace Mikoto {
    class OpenGLFrameBuffer : public FrameBuffer {
    public:
        explicit OpenGLFrameBuffer() = default;
        auto OnCreate(const FrameBufferCreateInfo& properties) -> void;

        auto Bind() -> void;
        auto Unbind() -> void;
        auto Recreate() -> void;
        auto Resize(UInt32_T width, UInt32_T height) -> void override;

        KT_NODISCARD auto GetId() const -> UInt32_T { return m_Id; }
        KT_NODISCARD auto GetFrameBufferProperties() const -> const FrameBufferCreateInfo& override { return m_FrameBufferCreateInfo; }

        KT_NODISCARD auto GetColorAttachmentId() const -> UInt32_T { return m_ColorAttachment; }
        KT_NODISCARD auto GetDepthAttachmentId() const -> UInt32_T { return m_DepthAttachment; }

        ~OpenGLFrameBuffer() override;
    private:
        UInt32_T m_Id{};
        UInt32_T m_ColorAttachment{};
        UInt32_T m_DepthAttachment{};
        FrameBufferCreateInfo m_FrameBufferCreateInfo{};
    };

}


#endif // MIKOTO_OPENGL_FRAME_BUFFER_HH
