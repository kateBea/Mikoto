/**
 * FrameBuffer.hh
 * Created by kate on 6/23/23.
 * */

#ifndef MIKOTO_FRAMEBUFFER_HH
#define MIKOTO_FRAMEBUFFER_HH

// C++ Standard Library
#include <memory>

// Project Headers
#include <Utility/Common.hh>

namespace Mikoto {
    struct FrameBufferCreateInfo {
        Int32_T width{};
        Int32_T height{};
        UInt32_T samples{};

        /**
         * NOTES:
         * Vulkan does not have the concept of a "default framebuffer", hence it
         * requires an infrastructure that will own the buffers we will render
         * to before we visualize them on the screen.
         * */

        // If it's true, we render to the default frame buffer which is going to
        // be the whole window and not to a specific Viewport
        bool UseDefaultFrameBufferTarget{};
    };

    // Temporary interface, contains OpenGL specific functionality
    class FrameBuffer {
    public:
        explicit FrameBuffer() = default;
        virtual ~FrameBuffer() = default;

        virtual auto Resize(UInt32_T width, UInt32_T height) -> void = 0;

        MKT_NODISCARD virtual auto GetFrameBufferProperties() const -> const FrameBufferCreateInfo& = 0;

        MKT_NODISCARD static auto CreatFrameBuffer(const FrameBufferCreateInfo& properties) -> std::shared_ptr<FrameBuffer>;
        MKT_NODISCARD static auto CreatFrameBufferRawPtr(const FrameBufferCreateInfo& properties) -> FrameBuffer*;
    };
}



#endif // MIKOTO_FRAMEBUFFER_HH
