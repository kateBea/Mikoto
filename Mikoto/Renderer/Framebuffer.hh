//
// Created by zanet on 4/9/2025.
//

#ifndef FRAMEBUFFER_HH
#define FRAMEBUFFER_HH

#include <any>

#include <Library/Utility/Types.hh>
#include <Library/Data/ResourcePool.hh>
#include <Renderer/DeviceObject.hh>
#include <Renderer/GpuUtility.hh>

namespace Mikoto {

    struct FramebufferDescription {
        Int32 Width{};
        Int32 Height{};

        TextureFormat ColorFormat{ TextureFormat::TEXTURE_FORMAT_RGBA8_SNORM };
        TextureFormat DepthFormat{ TextureFormat::TEXTURE_FORMAT_RGBA8_SNORM };

        std::vector<TextureHandle> DepthAttachment{};
        std::vector<TextureHandle> ColorAttachments{};

        // Can optionally pass in spec info
        std::any NativeHandleSpec{};

        auto WithSpecInfo(std::any nativeSpec) -> FramebufferDescription&;
        auto AddAttachment( TextureHandle color ) -> FramebufferDescription&;
        auto AddDepthAttachment( TextureHandle depth ) -> FramebufferDescription&;

        auto WithWidth( Int32 width ) -> FramebufferDescription&;
        auto WithHeight( Int32 height ) -> FramebufferDescription&;
    };

    /**
    * @brief Represents a framebuffer object used for off-screen rendering.
    *
    * This class encapsulates the functionality of a framebuffer, allowing for
    * rendering to textures instead of directly to the screen. It provides methods
    * for creating, binding, and managing the framebuffer and its associated
    * textures.
    */
    class Framebuffer : public DeviceObject  {
    public:
        MKT_NODISCARD auto GetDescription() const -> const FramebufferDescription& { return m_Spec; }
        MKT_NODISCARD auto GetWidth() const -> Int32 { return m_Spec.Width; }
        MKT_NODISCARD auto GetHeight() const -> Int32 { return m_Spec.Height; }

        MKT_NODISCARD auto GetColorAttachments() const -> const std::vector<TextureHandle>& { return m_Spec.ColorAttachments; }
        MKT_NODISCARD auto GetDepthAttachments() const -> const std::vector<TextureHandle>& { return m_Spec.DepthAttachment; }

        MKT_NODISCARD auto HasDepthAttachment() const -> bool { return !m_Spec.DepthAttachment.empty(); }
        MKT_NODISCARD auto HasColorAttachment() const -> bool { return !m_Spec.ColorAttachments.empty(); }

        MKT_NODISCARD auto GetNativeHandleSpec() const -> const std::any& { return m_Spec.NativeHandleSpec; }

    protected:
        explicit Framebuffer(const FramebufferDescription& desc)
            : m_Spec{ desc }
        {}

    protected:
        FramebufferDescription m_Spec{};
    };

    using FramebufferHandle = Ref<Framebuffer>;
}



#endif //FRAMEBUFFER_HH
