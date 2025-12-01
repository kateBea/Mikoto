//
// Created by kate on 10/23/25.
//

#ifndef MIKOTO_VULKAN_PASSES_HH
#define MIKOTO_VULKAN_PASSES_HH

#include <utility>

#include <ankerl/unordered_dense.h>
#include <volk.h>

#include <Assets/Font.hh>
#include <Assets/Texture.hh>

#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Core/Pipeline.hh>
#include <Renderer/Core/RenderPassBase.hh>
#include <Renderer/Core/RendererBackend.hh>
#include <Renderer/Vulkan/VulkanTexture.hh>
#include <Renderer/Vulkan/VulkanDescriptorManager.hh>

namespace Mikoto::VulkanPasses {

    class ShadingPass final : public IRenderPass {
    public:
        auto Init(GpuDevice* device) -> void override;
        auto Shutdown() -> void override;

        auto Begin(CommandListHandle cmd) -> void override;
        auto End() -> void override;

        auto Render(Scene* scene) -> void override;
        auto OnResize(UInt32 width, UInt32 height) -> void override;

        auto SetCamera( const Camera* camera ) -> void;

        auto GetPipeline() const -> PipelineHandle { return m_Pipeline; }

        auto SetClearColor(const Vec4F& color) -> void;

        auto GetFinalComposition() const -> TextureHandle;

    private:
        // TODO: temporary matches pbr_instance shaders layout
#define MAX_LIGHTS 50

        struct FrameUBO {
            glm::mat4 View{};
            glm::mat4 Projection{};
            Vec4F CameraPosition{};
        };

        struct LightTypeInfo {
            Vec4F Position{};
            Vec4F Direction{};
            Vec4F CutOffValues{};

            Vec4F Diffuse{};

            // x=cutOff, y=outerCutOff, z=intensity, w=radius
            Vec4F AttenuationParams{};

            // Meet shader uniform buffer alignment requirements
            alignas(sizeof(Vec4F)) Int32 ActiveLightType{};
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

        Unique<LightInfo> m_LightsInfo{};

    private:
        // Per frame data
        BufferHandle m_FrameUBOBuffer{};
        BufferHandle m_LightsBuffer{};
        VkDescriptorSet m_FrameSet{ VK_NULL_HANDLE };


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

        auto UpdateLightInformation(Scene* scene) -> void;
        auto InitGlobalShaderBuffers() -> void;

        auto UploadInstanceData() -> void;
        auto UpdateMeshInstanceData() -> void;

    private:
        GpuDevice* m_Device{};

        VkViewport m_Viewport{};
        VkRect2D m_Scissor{};

        PipelineHandle m_Pipeline{};
        AttachmentInfo m_ColorTarget{};
        AttachmentInfo m_DepthTarget{};

        Vec4F m_ClearColor{ 0.1f, 0.3f, 0.4f, 1.0f };

        CommandListHandle m_CmdList{};

        // For every mesh I store the count of elements and its buffer with its instance data
        bool m_UpdateInstanceData{ false };

        // For every mesh node i store the vertex buffer that contains all the data for all instances of that mesh and the ID and instance data for individual meshes to know if an instance already exists
        std::unordered_map<MeshNode*, std::pair<BufferHandle, std::unordered_map<UInt64, ShadingPassMeshBufferUBO>>> m_MeshInstanceData{};

    };

    class TextureRenderPass final : public IRenderPass {
    public:
        auto Init(GpuDevice* device) -> void override;
        auto Shutdown() -> void override;

        auto Begin(CommandListHandle cmd) -> void override;
        auto End() -> void override;

        auto Render(Scene* scene) -> void override;
        auto OnResize(UInt32 width, UInt32 height) -> void override;

        auto GetFinalComposition() const -> TextureHandle;
        auto SetMaterialPreviewMat(MaterialHandle ref ) -> void;
        auto SetMaterialPreviewViewport(float width, float height ) -> void;


    private:
        GpuDevice* m_Device{};

        VkViewport m_Viewport{};
        VkRect2D m_Scissor{};

        PipelineHandle m_Pipeline{};
        AttachmentInfo m_ColorTarget{};
        AttachmentInfo m_DepthTarget{};

        MaterialHandle m_Material{ };

        bool m_WantStoreOP{};
        Vec4F m_ClearColor{ 0.1f, 0.3f, 0.4f, 1.0f };

        CommandListHandle m_CmdList{};
    };

    class FontRenderPass final : public IRenderPass {
    public:
        auto Init(GpuDevice* device) -> void override;
        auto Shutdown() -> void override;

        auto Begin(CommandListHandle cmd) -> void override;
        auto End() -> void override;

        auto Render(Scene* scene) -> void override;
        auto OnResize(UInt32 width, UInt32 height) -> void override;

        auto GetFinalComposition() const -> TextureHandle;

    private:
        auto CreateAttributeBuffers() -> void;
        auto InitBuffersAndSets() -> void;

        auto DrawText(glm::vec2 position, const std::string& text, double fontSize, Vec4F color) -> void;

    private:
        struct FontParams {
            alignas(8) glm::vec2 Pos{};
            alignas(8) glm::vec2 Size{};
            alignas(16) glm::vec4 Color{};
            alignas(4) UInt32 TexIndex{};
            alignas(8) glm::vec2 TexCoords[4]{};
        };

        struct UBO {
            glm::mat4 Proj{};
        };

        struct FontVertex {
            glm::vec2 Pos{};
            glm::vec2 Uv{};
            UInt32 TexIndex{};
        };

        std::vector<FontVertex> m_Vertices{
            { { 0.0f, 0.0f }, { 0.0f, 0.0f }, 0 },
            { { 1.0f, 0.0f }, { 1.0f, 0.0f }, 1 },
            { { 1.0f, 1.0f }, { 1.0f, 1.0f }, 2 },
            { { 0.0f, 1.0f }, { 0.0f, 1.0f }, 3 }
        };

        std::vector<UInt32> m_Indices{
            0, 1, 3, 2
        };

        BufferHandle m_IndexBuffer{};
        BufferHandle m_VertexBuffer{};

    private:
        std::vector<FontParams> m_FontRenderParams{};
        GpuDevice* m_Device{};

        VkViewport m_Viewport{};
        VkRect2D m_Scissor{};

        FontHandle m_FontTest{};

        PipelineHandle m_Pipeline{};
        AttachmentInfo m_ColorTarget{};
        AttachmentInfo m_DepthTarget{};

        Vec4F m_ClearColor{ 0.4f, 0.2f, 0.7f, 1.0f };

        BufferHandle m_UBO{};
        VkDescriptorSet m_FontParamsSet{};

        // For now i will not try instancing
        BufferHandle m_FontParamsSSBO{};

        CommandListHandle m_CmdList{};
    };

    // Dummy compute pipeline we will use for testing only for now
    // This computes just calculates first prime numbers up until a limit
    class ComputeBasic final : public IComputePass {
    public:
        auto Init(GpuDevice* device) -> void override;
        auto Shutdown() -> void override;

        auto Begin(CommandListHandle cmd) -> void override;
        auto End() -> void override;

        auto Execute() -> void override;

    private:
        GpuDevice* m_Device{};
        PipelineHandle m_Pipeline{};

        // Prime numbers up until this value
        UInt32 m_Limit{ 30 };

        BufferHandle m_StorageBuffer{};
        VkDescriptorSet m_DescriptorSet{};

        CommandListHandle m_CmdList{};
    };
} // Mikoto

#endif
