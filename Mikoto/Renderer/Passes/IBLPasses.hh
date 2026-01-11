//
// Created by kate on 1/12/26.
//

#ifndef MIKOTO_IBLPASSES_HH
#define MIKOTO_IBLPASSES_HH

#include <Library/Utility/Types.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/GraphicsContext.hh>
#include <Renderer/Core/FramePass.hh>

namespace Mikoto {

    class EnvCubePass final : public FramePass {
    public:
        explicit EnvCubePass()
           : FramePass{ "EnvCubePass", PassType::RENDER } {}

        auto Setup(FrameGraphBuilder& builder) -> void override;
        auto Execute(PassCommandList& commandList) -> void override;


    };

    class IrradiancePass final : public FramePass {
    public:
        explicit IrradiancePass()
           : FramePass{ "IrradiancePass", PassType::RENDER } {}

        auto Setup(FrameGraphBuilder& builder) -> void override;
        auto Execute(PassCommandList& commandList) -> void override;


    };

    class PrefilterPass final : public FramePass {
    public:
        explicit PrefilterPass()
           : FramePass{ "PrefilterPass", PassType::RENDER } {}

        auto Setup(FrameGraphBuilder& builder) -> void override;
        auto Execute(PassCommandList& commandList) -> void override;


    };

    class BRDFLutPass final : public FramePass {
    public:
        explicit BRDFLutPass()
           : FramePass{ "BRDFLutPass", PassType::RENDER } {}

        auto Setup(FrameGraphBuilder& builder) -> void override;
        auto Execute(PassCommandList& commandList) -> void override;


    };
}


#endif//MIKOTO_IBLPASSES_HH
