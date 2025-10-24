//
// Created by kate on 10/23/25.
//

#ifndef MIKOTO_VULKAN_PASSES_HH
#define MIKOTO_VULKAN_PASSES_HH

#include <volk.h>
#include <ankerl/unordered_dense.h>

#include <Renderer/GpuDevice.hh>
#include <Renderer/RendererBackend.hh>
#include <Renderer/RenderPassBase.hh>

#include <Renderer/Pipeline.hh>

namespace Mikoto::VulkanPasses {

    class ShadingPass final : public IRenderPass {
    public:
        auto Init(GpuDevice* device) -> void override;
        auto Shutdown() -> void override;

        auto Begin(CommandListHandle cmd) -> void override;
        auto End() -> void override;

        auto Render(Scene* scene) -> void override;
        auto OnResize(UInt32 width, UInt32 height) -> void override;

        auto BindDefaultSets(VkDescriptorSet) -> void;

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
            MeshNode* Mesh = nullptr;
            std::vector<ShadingPassMeshBufferUBO> Instances;
        };

        auto InitInstanceData() -> void;
        auto UploadInstanceData() -> void;
        auto CreateMeshesStorageDescriptorSet() -> void;
        auto DrawMeshBatch(CommandListHandle cmd, const MeshBatch& batch) -> void;

    private:
        GpuDevice* m_Device{};

        PipelineHandle m_Pipeline{};
        AttachmentInfo m_ColorTarget{};
        AttachmentInfo m_DepthTarget{};

        bool m_WantStoreOP{};
        Vec4F m_ClearColor{};

        CommandListHandle m_CmdList{};

        std::unordered_map<MeshNode*, MeshBatch> m_MeshBatches{};
        std::unordered_map<MeshNode*, Size> m_BatchOffsetMap{};

        BufferHandle m_InstanceSSBO{};
        VkDescriptorSet m_MeshDataSet{};
    };

    // Dummy compute pipeline we will use for testing only for now
    class ComputeBasic final : public IPass {
    public:
        auto Init(GpuDevice* device) -> void override;
        auto Shutdown() -> void override;

        auto Begin(CommandListHandle cmd) -> void override;
        auto End() -> void override;

        auto Execute() -> void override;

    private:
        GpuDevice* m_Device{};
        PipelineHandle m_Pipeline{};

        CommandListHandle m_CmdList{};
    };
} // Mikoto

#endif
