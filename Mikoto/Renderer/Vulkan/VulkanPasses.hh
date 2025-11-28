//
// Created by kate on 10/23/25.
//

#ifndef MIKOTO_VULKAN_PASSES_HH
#define MIKOTO_VULKAN_PASSES_HH

#include <utility>

#include <ankerl/unordered_dense.h>
#include <volk.h>

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

        auto BindDefaultSets(CommandListHandle cmd, VkDescriptorSet& set, UInt32 setIndex ) -> void;

        auto GetPipeline() const -> PipelineHandle { return m_Pipeline; }

        auto WantStoreOP(bool enable) -> void;
        auto SetClearColor(const Vec4F& color) -> void;

        auto GetFinalComposition() const -> TextureHandle;

    private:
        struct ShadingPassMeshBufferUBO {
            glm::mat4 Transform{};
            Vec4F Albedo{};
            Vec4F Factors{};
            Int32 AlbedoIndex{};
            Int32 NormalIndex{};
            Int32 MetallicIndex{};
            Int32 RoughnessIndex{};
            Int32 AoIndex{};
        };

        struct MeshBatch {
            MeshNode* Mesh{ nullptr };
            ankerl::unordered_dense::map<UInt64, ShadingPassMeshBufferUBO> Instances{};
        };

        auto InitInstanceData() -> void;
        auto UploadInstanceData() -> void;
        auto CreateMeshesStorageDescriptorSet() -> void;
        auto DrawMeshBatch(const MeshBatch& batch) -> void;

    private:
        GpuDevice* m_Device{};

        VkViewport m_Viewport{};
        VkRect2D m_Scissor{};

        PipelineHandle m_Pipeline{};
        AttachmentInfo m_ColorTarget{};
        AttachmentInfo m_DepthTarget{};

        DescriptorSetLayoutHandle m_EntitiesSetLayout{  };

        bool m_WantStoreOP{};
        Vec4F m_ClearColor{ 0.1f, 0.3f, 0.4f, 1.0f };

        CommandListHandle m_CmdList{};

        std::unordered_map<MeshNode*, MeshBatch> m_MeshBatches{};
        std::unordered_map<MeshNode*, Size> m_BatchOffsetMap{};

        BufferHandle m_InstanceSSBO{};
        VkDescriptorSet m_MeshDataSet{};
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

        auto RegisterTextureForRender( TextureHandle texture ) -> void;

    private:
        auto UpdateBindlessTextureDescriptor( Int32 index, VulkanTexture* texture ) const -> void;

    private:
        GpuDevice* m_Device{};

        VkViewport m_Viewport{};
        VkRect2D m_Scissor{};

        PipelineHandle m_Pipeline{};
        AttachmentInfo m_ColorTarget{};
        AttachmentInfo m_DepthTarget{};

        MaterialHandle m_Material{ };

#if defined( MKT_USE_VULKAN_BINDLESS )
        VkDescriptorSet m_TexturesSet{ VK_NULL_HANDLE };
        bool m_UpdateTextureDescriptor{ false };
        ankerl::unordered_dense::map<Texture*, TextureHandle> m_BindlessTextures{};
#endif

        bool m_WantStoreOP{};
        Vec4F m_ClearColor{ 0.1f, 0.3f, 0.4f, 1.0f };

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
