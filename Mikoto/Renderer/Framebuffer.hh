//
// Created by zanet on 4/9/2025.
//

#ifndef FRAMEBUFFER_HH
#define FRAMEBUFFER_HH

#include <Library/Utility/Types.hh>
#include <Library/Data/ResourcePool.hh>
#include <Renderer/DeviceObject.hh>

namespace Mikoto {

    struct FramebufferDescription {
        Int32 Width{};
        Int32 Height{};

        TextureFormat ColorFormat{ TextureFormat::TEXTURE_FORMAT_RGBA8 };
        TextureFormat DepthFormat{ TextureFormat::TEXTURE_FORMAT_RGBA8 };

        TextureHandle DepthAttachment{};
        std::vector<TextureHandle> ColorAttachments{};

        auto AddAttachment( TextureHandle color ) -> FramebufferDescription&;
        auto AddDepthAttachment( TextureHandle depth ) -> FramebufferDescription&;

        auto WithWidth( Int32 width ) -> FramebufferDescription&;
        auto WithHeight( Int32 height ) -> FramebufferDescription&;
        auto WithColorFormat( TextureFormat format ) -> FramebufferDescription&;
        auto WithDepthFormat( TextureFormat format ) -> FramebufferDescription&;
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

    };

    using FramebufferHandle = Ref<Framebuffer>;
}



#endif //FRAMEBUFFER_HH
