//
// Created by kate on 11/24/25.
//

#ifndef MIKOTO_FRAMEPASS_HH
#define MIKOTO_FRAMEPASS_HH

#include <string>
#include <string_view>
#include <vector>

#include <ankerl/unordered_dense.h>

#include <Assets//Texture.hh>
#include <Assets/Model.hh>
#include <Library/Data/ResourcePool.hh>
#include <Library/Utility/Types.hh>
#include <Renderer/Core/Buffer.hh>
#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Core/GraphicsContext.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Passes/ShardRenderParams.hh>
#include <Scene/Scene.hh>

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

        explicit FinalCompositionPass(GpuDevice* device)
            : FramePass{ "FinalCompositionPass", PassType::RENDER }, m_Device{ device } {}

        auto Setup(FrameGraphBuilder& device) -> void override;
        auto Execute(PassCommandList& commandList) -> void override;

        auto SetScene(Scene* scene) -> void;
        auto SetCamera( const Camera* camera ) -> void;

    private:

        auto UploadInstanceData() -> void;
        auto UpdateInstancedData() -> void;
        auto TraverseMeshList(PassCommandList& commandList) -> void;

    public:
        struct ShadingPassMeshBufferUBO {
            Vec4F i_TransformCol0{};
            Vec4F i_TransformCol1{};
            Vec4F i_TransformCol2{};
            Vec4F i_TransformCol3{};

            Vec4F Albedo{};
            Vec4F Factors{};
            Int32 AlbedoIndex{};
            Int32 NormalIndex{};
            Int32 MetallicIndex{};
            Int32 RoughnessIndex{};
            Int32 AoIndex{};
        };

        static constexpr UInt32 MAX_LIGHTS{ 5096 };

        struct FrameUBO {
            glm::mat4 View{};
            glm::mat4 Projection{};
            Vec4F CameraPosition{};
        };

        struct alignas(sizeof(Vec4F)) LightTypeInfo {
            Vec4F Position{};
            Vec4F Direction{};

            Vec3F Diffuse{};

            float CutOff{};
            float OuterCutOff{};
            float Intensity{};
            float Radius{};

            Int32 ActiveLightType{};
        };

        struct LightInfo {
            enum class DisplayModes {
                DISPLAY_NORMAL = 1,
                DISPLAY_COLOR = 2,
                DISPLAY_METAL = 3,
                DISPLAY_AO = 4,
                DISPLAY_ROUGH = 5,
            };

            enum class ActiveLightType {
                LIGHT_TYPE_INACTIVE = -1,
                LIGHT_TYPE_POINT = 1,
                LIGHT_TYPE_SPOT = 2,
                LIGHT_TYPE_DIRECTIONAL = 3,
            };

            std::array<LightTypeInfo, MAX_LIGHTS> Lights{};

            Int32 ActiveLightsCount{};
            Int32 DisplayMode{};
        };

    private:
        LightInfo m_LightsInfo{};
        FrameUBO m_FrameUBO{};

        GpuDevice* m_Device{};

        Scene* m_Scene{};
        Vec4F m_ClearColor{ 0.1f, 0.3f, 0.4f, 1.0f };

        // For every mesh I store the count of elements and its buffer with its instance data
        bool m_UpdateInstanceData{ false };

        // For every mesh node i store the vertex buffer that contains all the data for all instances of that mesh and the ID and instance data for individual meshes to know if an instance already exists
        ankerl::unordered_dense::map<MeshNode*, std::pair<BufferHandle, ankerl::unordered_dense::map<UInt64, ShadingPassMeshBufferUBO>>> m_MeshInstanceData{};
    };

    class AABBGenComp final : public FramePass {
    public:
        constexpr static UInt32 MAX_LIGHT_CLUSTERS{ 256 };
        struct CameraUBO {
            glm::mat4 ViewMatrix{};
            glm::mat4 InverseProjection{};

            glm::vec4 GridSize{};
            glm::vec4 ViewPosition{};

            // xy = Planes, zw = ScreenDimensions
            glm::vec4 Screen{};

            // x = show heat map
            glm::vec4 LightInfo{};
        };

        struct alignas(sizeof(glm::vec4)) Cluster  {
            glm::vec4 Center{};
            glm::vec4 ClosestPoint{};
            glm::vec4 DistanceSquared{};

            glm::vec4 MinPoint{};
            glm::vec4 MaxPoint{};
            UInt32 Count{};
            UInt32 LightIndices[MAX_LIGHT_CLUSTERS];
        };

        explicit AABBGenComp()
            : FramePass{ "AABBGenComp", PassType::COMPUTE } {}

        auto Setup(FrameGraphBuilder& builder) -> void override;
        auto Execute(PassCommandList& cmdList) -> void override;

        auto SetCamera(const Camera* camera) -> void;
        auto SetHeatMap(bool enable) -> void;

        MKT_NODISCARD auto GetClusterCount() const -> UInt32 { return m_NumClusters; }

    private:
        UInt32 m_GridSizeX{ 12 };
        UInt32 m_GridSizeY{ 12 };
        UInt32 m_GridSizeZ{ 24 };
        UInt32 m_NumClusters{ m_GridSizeX * m_GridSizeY * m_GridSizeZ };

        const Camera* m_Camera{};
        CameraUBO m_CameraUBO{};
    };

    class LightCullingComp final : public FramePass {
    public:
        explicit LightCullingComp()
            : FramePass{ "LightCullingComp", PassType::COMPUTE } {}

        auto Setup(FrameGraphBuilder& builder) -> void override;
        auto Execute(PassCommandList& commandList) -> void override;

        auto SetClusterCount(UInt32 clusterCount) -> void;

        auto SetScene(Scene* scene) -> void;

    private:
        auto TraverseLights( const PassCommandList & commandList ) -> void;

        struct alignas(16) LightCullingUBO {
            UInt32 LightCount{};
        };

    private:

        UInt32 m_LocalSize{ 128 }; // from light culling comp shader
        UInt32 m_NumClusters{ 0 };

        LightCullingUBO m_LightCullingUBO{};

        Scene* m_Scene{};
        std::array<FinalCompositionPass::LightTypeInfo, FinalCompositionPass::MAX_LIGHTS> m_Lights{};
    };

    class ShadowPass final : public FramePass {
    public:

        explicit ShadowPass()
            : FramePass{ "ShadowPass", PassType::RENDER } {}

        auto Setup(FrameGraphBuilder& device) -> void override;
        auto Execute(PassCommandList& commandList) -> void override;

        auto SetScene(Scene* scene) -> void;

    private:
        GpuDevice* m_Device{};

        Scene* m_Scene{};

        Vec4F m_ClearColor{ 0.1f, 0.3f, 0.4f, 1.0f };
    };

}


#endif//MIKOTO_FRAMEPASS_HH
