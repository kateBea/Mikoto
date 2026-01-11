//
// Created by kate on 11/24/25.
//

#ifndef MIKOTO_FRAMEPASS_HH
#define MIKOTO_FRAMEPASS_HH

#include <ankerl/unordered_dense.h>

#include <Assets//Texture.hh>
#include <Assets/Model.hh>
#include <Library/Data/ResourcePool.hh>
#include <Library/Utility/Types.hh>
#include <Renderer/Core/Buffer.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Core/GraphicsContext.hh>
#include <Renderer/Passes/ShaderRenderParams.hh>
#include <Scene/Scene.hh>
#include <string>
#include <string_view>
#include <vector>

#include "Assets/Font.hh"
#include "Scene/Camera.hh"

namespace Mikoto {

    class FramePass {
    public:
        enum class PassType { RENDER, COMPUTE };

        using ResourceHandle = Ref<IResource>;

        virtual ~FramePass() = default;

        virtual auto Setup(FrameGraphBuilder& builder) -> void = 0;
        virtual auto Execute(PassCommandList& cmdList) -> void = 0;

        MKT_NODISCARD auto GetPassType() const -> PassType { return m_PassType; }

        MKT_NODISCARD auto IsCompute() const -> bool { return m_PassType == PassType::COMPUTE; }
        MKT_NODISCARD auto IsRender() const -> bool { return m_PassType == PassType::RENDER; }

        MKT_NODISCARD auto GetName() const -> const std::string& { return m_Name; }

    protected:
        explicit FramePass( std::string_view name, PassType passType )
            : m_Name{ name } {}

    protected:
        PassType m_PassType{};
        std::string m_Name{};
    };

    enum class RenderResolution {
        FULL_HD,
        QUAD_HD,
        ULTRA_HD,
    };

    // function to infer image size based on render resolution

    class SkyboxPass final : public FramePass {
    public:
        explicit SkyboxPass(GpuDevice* device)
           : FramePass{ "SkyboxPass", PassType::RENDER } {}

        auto Setup(FrameGraphBuilder& builder) -> void override;
        auto Execute(PassCommandList& commandList) -> void override;

        auto SetCamera(const Camera* camera) -> void;
        auto SetCubeMap(TextureHandle cubeMap) -> void;

    private:

        struct SkyboxUBO {
            Mat4F View{};
            Mat4F Projection{};
        };

    private:
        SamplerHandle m_Sampler{};

        SkyboxUBO m_SkyboxUBO{};
        TextureHandle m_CubeMap{};
    };

    class TextRenderPass final : public FramePass {
    public:

        explicit TextRenderPass()
            : FramePass{ "TextRenderPass", PassType::RENDER } {}

        auto Setup(FrameGraphBuilder& builder) -> void override;
        auto Execute(PassCommandList& commandList) -> void override;

        auto SetScene(Scene* scene) -> void;
        auto SetCamera( const Camera* camera ) -> void;

    private:
        auto TraverseTextList(PassCommandList& commandList) -> void;

        auto SetupRenderParams(PassCommandList &commandList) -> void;
        auto SetupTextForRender(FontHandle font, Vec4F position, std::string_view text, double fontSize, Vec4F color, PassCommandList& commandList) -> void;

    private:
        struct alignas(16) TextRenderParams {
            Vec4F Position{};
            Vec4F Size{};
            Vec4F Color{};
            Vec2F TexCoords[4]{};
            UInt32 TexIndex{};
        };

        struct alignas(16) TextParamsUBO {
            glm::mat4 Proj{};
            glm::mat4 View{};
            Vec4F OutlineColor{ 1.0f, 1.0f, 1.0f, 1.0f };
            float OutlineWidth{ 2.0f };
        };

        struct FontVertex {
            glm::vec3 Pos{};
            UInt32 TexIndex{};
        };

        static constexpr UInt32 MAX_STRING{ 8096 * 10 };

        std::vector<TextRenderParams> m_TextRenderParams{};

        std::array<FontVertex, 4> VERTICES{
                FontVertex{ { 0.0f, 0.0f, 0.0f }, 0 },
                FontVertex{ { 1.0f, 0.0f, 0.0f }, 1 },
                FontVertex{ { 1.0f, 1.0f, 0.0f }, 2 },
                FontVertex{ { 0.0f, 1.0f, 0.0f }, 3 }
        };

        std::array<UInt32, 6> INDICES{
            0, 1, 2,  // first triangle
            2, 3, 0   // second triangle
        };

        Scene* m_Scene{};
        const Camera* m_Camera{};

        TextParamsUBO m_TextRenderUBO{};

        Vec4F m_ClearColor{ 0.1f, 0.3f, 0.4f, 1.0f };
    };

    class FinalCompositionPass final : public FramePass {
    public:

        explicit FinalCompositionPass()
            : FramePass{ "FinalCompositionPass", PassType::RENDER } {}

        auto Setup(FrameGraphBuilder& device) -> void override;
        auto Execute(PassCommandList& commandList) -> void override;

        auto SetScene(Scene* scene) -> void;
        auto SetCamera( const Camera* camera ) -> void;
        auto SetCamera( const Vec4F& color ) -> void;

        auto EnableSkybox(bool enable) -> void;
        auto SetClearColor(const Vec4F& vec ) -> void;

    private:

        auto UploadInstanceData(PassCommandList& commandList) -> void;
        auto TraverseMeshList(PassCommandList& commandList) -> void;

    public:

        struct MeshInstanceInfo {
            DrawIndexedState InstanceDrawState{};
            ankerl::unordered_dense::map<UInt64, ShaderMaterialParams> InstanceInfos{};
        };
    private:
        ShaderLightListParams m_LightsInfo{};
        ShaderCameraParams m_FrameUBO{};

        bool m_UseSkybox{ false };

        Scene* m_Scene{};
        Vec4F m_ClearColor{ 0.1f, 0.3f, 0.4f, 1.0f };

        std::array<ShaderMaterialParams, MAX_RENDERABLE_ENTITIES> m_Meshes{};
        ankerl::unordered_dense::map<MeshNode*, MeshInstanceInfo> m_MeshDrawState{};
    };

}


#endif//MIKOTO_FRAMEPASS_HH
