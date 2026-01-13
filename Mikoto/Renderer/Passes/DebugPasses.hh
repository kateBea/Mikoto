//
// Created by zanet on 1/5/2026.
//

#ifndef MIKOTO_DEBUG_PASSES_HH
#define MIKOTO_DEBUG_PASSES_HH

#include <string_view>

#include <Library/Utility/Types.hh>
#include <Renderer/Core/GraphicsContext.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/FramePass.hh>
#include <Scene/Scene.hh>

namespace Mikoto {

    class ObjectOutlinePass final : public FramePass {
    public:
        explicit ObjectOutlinePass()
            : FramePass{ "ObjectOutlinePass", FramePassType::RENDER } {}

        auto Setup(FrameGraphBuilder& builder) -> void override;
        auto Execute(PassCommandList& cmdList) -> void override;
    };

    class WireFramePass final : public FramePass {
    public:
        explicit WireFramePass()
            : FramePass{ "WireFramePass", FramePassType::RENDER } {}

        auto Setup(FrameGraphBuilder& builder) -> void override;
        auto Execute(PassCommandList& cmdList) -> void override;
    };

    // Material Pass that renders a sphere with a texture on a sphere
    // Uses IBL precomputed info
    class MaterialPreviewPass final : public FramePass {
    public:
        explicit MaterialPreviewPass()
            : FramePass{ "MaterialPreviewPass", FramePassType::RENDER } {}

        auto Setup(FrameGraphBuilder& builder) -> void override;
        auto Execute(PassCommandList& cmdList) -> void override;
    };

    class TextPass final : public FramePass {
    public:
        explicit TextPass()
            : FramePass{ "TextPass", FramePassType::RENDER } {}

        auto Setup(FrameGraphBuilder& builder) -> void override;
        auto Execute(PassCommandList& cmdList) -> void override;

        auto SetScene(Scene* scene) -> void;

    private:
        Scene* m_Scene{};
    };

    // This class is kept for debug purposes
    // just computes prime numbers
    class SimpleComputePass final : public FramePass {
    public:
        explicit SimpleComputePass()
            : FramePass{ "SimpleComputePass", FramePassType::COMPUTE } {}

        auto Setup(FrameGraphBuilder& builder) -> void override;
        auto Execute(PassCommandList& cmdList) -> void override;


    };

    // Simple pass for testing purposes
    // A triangle with interpolation
    class HelloTrianglePass final : public FramePass {
    public:
        explicit HelloTrianglePass()
            : FramePass{ "HelloTrianglePass", FramePassType::RENDER } {}

        auto Setup(FrameGraphBuilder& builder) -> void override;
        auto Execute(PassCommandList& cmdList) -> void override;
    };

    // Simple pass for testing purposes
    // Displays a texture
    class HelloTexture final : public FramePass {
    public:
        explicit HelloTexture()
            : FramePass{ "HelloTexture", FramePassType::RENDER } {}

        auto Setup(FrameGraphBuilder& builder) -> void override;
        auto Execute(PassCommandList& cmdList) -> void override;

    private:

        struct HelloTextureUniformBuffer {
            Int32 TextureIndex{ SRGTextures::INVALID_TEXTURE_INDEX };
        };
    };

    // Simple pass for testing purposes
    // A colored/textured cube
    class HelloCubePass final : public FramePass {
    public:
        explicit HelloCubePass()
            : FramePass{ "HelloTrianglePass", FramePassType::RENDER } {}

        auto Setup(FrameGraphBuilder& builder) -> void override;
        auto Execute(PassCommandList& cmdList) -> void override;
    };

}

#endif //MIKOTO_DEBUG_PASSES_HH