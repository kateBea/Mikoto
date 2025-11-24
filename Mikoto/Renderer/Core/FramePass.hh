//
// Created by kate on 11/24/25.
//

#ifndef MIKOTO_FRAMEPASS_HH
#define MIKOTO_FRAMEPASS_HH

#include <Assets//Texture.hh>
#include <Assets/Model.hh>
#include <Library/Data/ResourcePool.hh>
#include <Library/Utility/Types.hh>
#include <Renderer/Core/Buffer.hh>
#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Core/GraphicsContext.hh>
#include <Renderer/Core/Pipeline.hh>
#include <Scene/Scene.hh>
#include <string>
#include <string_view>
#include <vector>

namespace Mikoto {

    class FramePass {
    public:
        using ResourceHandle = Ref<IResource>;

        virtual ~FramePass() = default;

        virtual auto Setup(GpuDevice* device) -> void = 0;
        virtual auto Execute(GraphicsContext& context) -> void = 0;

        virtual auto RegisterInput(ResourceHandle resource) -> void = 0;

        MKT_NODISCARD auto GetName() const -> const std::string& { return m_Name; }

    protected:
        explicit FramePass(std::string_view name )
            : m_Name{ name } {}

    protected:

        std::vector<ResourceHandle> m_Inputs{};
        std::vector<ResourceHandle> m_Outputs{};

        std::string m_Name{};
    };

    class FinalCompositionPass final : public FramePass {
    public:

        explicit FinalCompositionPass()
            : FramePass{ "FinalCompositionPass" } {}

        auto Setup(GpuDevice* device) -> void override;
        auto Execute(GraphicsContext& context) -> void override;

        auto RegisterInput(ResourceHandle resource) -> void override;

        auto SetScene(Scene* scene) -> void;

    private:
        GpuDevice* m_Device{};

        Scene* m_Scene{};

        UVec2 m_Scissor{};
        UVec2 m_Viewport{};

        PipelineHandle m_Pipeline{};
        TextureHandle m_ColorTarget{};
        TextureHandle m_DepthTarget{};

        Vec4F m_ClearColor{ 0.1f, 0.3f, 0.4f, 1.0f };
    };

    class ShadowPass final : public FramePass {
    public:

        explicit ShadowPass()
            : FramePass{ "ShadowPass" } {}

        auto Setup(GpuDevice* device) -> void override;
        auto Execute(GraphicsContext& context) -> void override;

        auto RegisterInput(ResourceHandle resource) -> void override;

        auto SetScene(Scene* scene) -> void;

    private:
        GpuDevice* m_Device{};

        Scene* m_Scene{};

        UVec2 m_Scissor{};
        UVec2 m_Viewport{};

        PipelineHandle m_Pipeline{};
        TextureHandle m_ColorTarget{};
        TextureHandle m_DepthTarget{};

        Vec4F m_ClearColor{ 0.1f, 0.3f, 0.4f, 1.0f };
    };

    class TextPass final : public FramePass {
    public:
        explicit TextPass()
            : FramePass{ "TextPass" } {}

        auto Setup(GpuDevice* device) -> void override;
        auto Execute(GraphicsContext& context) -> void override;

        auto RegisterInput(ResourceHandle resource) -> void override;

    private:
        GpuDevice* m_Device{};

        UVec2 m_Scissor{};
        UVec2 m_Viewport{};

        PipelineHandle m_Pipeline{};

        // These come from the final composition
        // this pass is supposed to run at the very end
        TextureHandle m_ColorTarget{};
        TextureHandle m_DepthTarget{};
    };

    // This class is kept for debug purposes
    class SimpleComputePass final : public FramePass {
    public:
        explicit SimpleComputePass()
            : FramePass{ "SimpleComputePass" } {}

        auto Setup(GpuDevice* device) -> void override;
        auto Execute(GraphicsContext& context) -> void override;

        auto RegisterInput(ResourceHandle resource) -> void override;

    private:
        GpuDevice* m_Device{};

        // Prime numbers up until this value
        UInt32 m_Limit{ 30 };

        PipelineHandle m_Pipeline{};
        BufferHandle m_DepthTarget{};

        BufferHandle m_StorageBuffer{};
    };

}


#endif//MIKOTO_FRAMEPASS_HH
