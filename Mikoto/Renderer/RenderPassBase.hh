//
// Created by zanet on 4/14/2025.
//

#ifndef RENDERPASS_HH
#define RENDERPASS_HH

#include <Renderer/GpuDevice.hh>
#include <Assets/Texture.hh>
#include <Renderer/RendererBackend.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    struct AttachmentInfo {
        TextureHandle Image{};

        Vec4F ClearColor{ 0.4f, 0.33f, 0.55f, 1.0f };

        float ClearDepth{ 1.0f };

        bool Clear{ true };
        bool Store{ true };
    };

    class IPass {
    public:
        virtual ~IPass() = default;

        virtual auto Init(GpuDevice* device) -> void = 0;
        virtual auto Shutdown() -> void = 0;

        virtual auto Execute() -> void {}

        virtual auto Begin(CommandListHandle cmd) -> void = 0;
        virtual auto End() -> void = 0;
    };


    class IRenderPass : public IPass {
    public:
        ~IRenderPass() override = default;

        virtual auto Render(Scene* scene) -> void = 0;
        virtual auto OnResize(UInt32 width, UInt32 height) -> void {}
    };

}

#endif//RENDERPASS_HH
