//
// Created by kate on 11/24/25.
//

#ifndef MIKOTO_FRAMEPASS_HH
#define MIKOTO_FRAMEPASS_HH

#include <string>
#include <string_view>
#include <vector>

#include <Assets//Texture.hh>
#include <Assets/Model.hh>
#include <Library/Data/ResourcePool.hh>
#include <Library/Utility/Types.hh>
#include <Renderer/Core/Buffer.hh>
#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Core/GraphicsContext.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/Pipeline.hh>
#include <Scene/Scene.hh>

namespace Mikoto {

    class FramePass {
    public:
        using ResourceHandle = Ref<IResource>;

        virtual ~FramePass() = default;

        virtual auto Setup(GraphicsContext* device) -> void = 0;
        virtual auto Execute(PassCommandList& cmdList) -> void = 0;

        MKT_NODISCARD auto GetName() const -> const std::string& { return m_Name; }

    protected:
        explicit FramePass(std::string_view name )
            : m_Name{ name } {}

        auto RegisterInput(std::string_view name) -> void;
        auto RegisterOutput(std::string_view name) -> void;
        auto RegisterResource(std::string_view name) -> void;

    protected:

        std::string m_Name{};

        std::vector<std::string> m_Inputs{};
        std::vector<std::string> m_Outputs{};
        std::vector<std::string> m_Resources{};

        // std::vector<FrameResource> m_Inputs{};
        // std::vector<FrameResource> m_Outputs{};
        // std::vector<FrameResource> m_Resources{};
    };

    class FinalCompositionPass final : public FramePass {
    public:

        explicit FinalCompositionPass()
            : FramePass{ "FinalCompositionPass" } {}

        auto Setup(GraphicsContext* device) -> void override;
        auto Execute(PassCommandList& cmdList) -> void override;

        auto SetScene(Scene* scene) -> void;

    private:
        GpuDevice* m_Device{};

        Scene* m_Scene{};

        Vec4F m_ClearColor{ 0.1f, 0.3f, 0.4f, 1.0f };
    };

    class ShadowPass final : public FramePass {
    public:

        explicit ShadowPass()
            : FramePass{ "ShadowPass" } {}

        auto Setup(GraphicsContext* device) -> void override;
        auto Execute(PassCommandList& cmdList) -> void override;

        auto SetScene(Scene* scene) -> void;

    private:
        GpuDevice* m_Device{};

        Scene* m_Scene{};

        Vec4F m_ClearColor{ 0.1f, 0.3f, 0.4f, 1.0f };
    };

    class TextPass final : public FramePass {
    public:
        explicit TextPass()
            : FramePass{ "TextPass" } {}

        auto Setup(GraphicsContext* device) -> void override;
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
            : FramePass{ "SimpleComputePass" } {}

        auto Setup(GraphicsContext* device) -> void override;
        auto Execute(PassCommandList& cmdList) -> void override;


    };

}


#endif//MIKOTO_FRAMEPASS_HH
