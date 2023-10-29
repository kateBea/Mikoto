/**
 * Renderer.cc
 * Created by kate on 6/5/23.
 * */

// C++ Standard Library
#include <memory>
#include <utility>

// Project Headers
#include <Common/Common.hh>

#include <Core/TimeManager.hh>

#include <Assets/AssetsManager.hh>

#include <Renderer/Renderer.hh>
#include <Renderer/RenderCommand.hh>
#include <Renderer/Vulkan/VulkanContext.hh>

namespace Mikoto {
    auto Renderer::BeginScene(const ScenePrepareData& prepareData) -> void {
        s_DrawData->SceneRuntimeCamera = prepareData.RuntimeCamera;
        s_DrawData->SceneEditCamera = prepareData.StaticCamera;
    }

    auto Renderer::Submit(std::shared_ptr<DrawData>&& data) -> void {
        // Compute render stats
        if (data->MeshMeta) {
            for (auto& meta : *data->MeshMeta) {
                s_RenderingStats->IncrementVertexCount(meta.ModelMesh->GetVertexBuffer()->GetCount());
                s_RenderingStats->IncrementIndexCount(meta.ModelMesh->GetIndexBuffer()->GetCount());

                s_RenderingStats->IncrementModelsCount(1);
                s_RenderingStats->IncrementObjectsCount(1);
                s_RenderingStats->IncrementMeshesCount(data->MeshMeta->size());
            }
        }

        RenderCommand::AddToRenderQueue(std::move(data));

        // At the moment, we need a draw call per mesh because every
        // mesh has a single vertex and index buffers which are used for rendering
        s_RenderingStats->IncrementDrawCallCount(1);
    }

    auto Renderer::EndScene() -> void {
        *s_SavedSceneStats = *s_RenderingStats;
        s_RenderingStats->Reset();
    }

    auto Renderer::Flush() -> void {
        RenderCommand::Flush();
    }

    auto Renderer::Init(RendererSpec&& spec) -> void {
        s_Spec = std::move(spec);

        s_ActiveAPI = s_Spec.Backend;
        s_ActiveRendererAPI = RendererBackend::Create(s_ActiveAPI);

        if (s_ActiveRendererAPI) {
            s_ActiveRendererAPI->Init();
        }
        else {
            MKT_THROW_RUNTIME_ERROR("Could not pick a valid render backend");
        }

        RenderCommand::Init(s_ActiveRendererAPI);

        s_DrawData = std::make_unique<RendererDrawData>();
        s_RenderingStats = std::make_unique<RenderingStats>();
        s_SavedSceneStats = std::make_unique<RenderingStats>();
    }

    auto Renderer::Shutdown() -> void {
        if (s_ActiveRendererAPI)
            s_ActiveRendererAPI->Shutdown();

        delete s_ActiveRendererAPI;
    }

    auto Renderer::GetRendererStatistics() -> const RendererStatistics& {
        // Update this data every 5 seconds
        auto elapsed{ TimeManager::GetTime() };
        static constexpr double updateInterval{ 5.0 };
        static double lastTimeUpdate{ elapsed };

        if ((elapsed - lastTimeUpdate) >= updateInterval) {
            UpdateRendererStatistics();
            lastTimeUpdate = elapsed;
        }

        return s_Statistics;
    }

    auto Renderer::Submit(const SceneObjectData& objectData, const glm::mat4& transform) -> void {
        auto data{ std::make_shared<DrawData>() };

        data->Color = objectData.Color;
        data->MeshMeta = std::addressof(objectData.MeshMeta);
        data->TransformData.Transform = transform;
        data->TransformData.Projection = s_DrawData->SceneEditCamera->GetProjection();
        data->TransformData.View = s_DrawData->SceneEditCamera->GetViewMatrix();

        // Rendering submission
        Renderer::Submit(std::move(data));
    }

    auto Renderer::UpdateRendererStatistics() -> void {
        switch (s_Spec.Backend) {
            case GraphicsAPI::OPENGL_API:
                // TODO: get statistics
                break;
            case GraphicsAPI::VULKAN_API:
                auto& stats{ VulkanContext::GetDetailedStatistics() };
                s_Statistics.VRAMUsage = stats.total.statistics.allocationBytes;
                break;
        }
    }
}
