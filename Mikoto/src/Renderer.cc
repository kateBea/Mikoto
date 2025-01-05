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
#include <Renderer/Core/RenderCommand.hh>
#include <Renderer/Core/Renderer.hh>
#include <Renderer/Vulkan/VulkanContext.hh>

namespace Mikoto {
    auto Renderer::BeginScene(const ScenePrepareData& prepareData) -> void {
        s_DrawData->RuntimeCamera = prepareData.RuntimeCamera;
        s_DrawData->StaticCamera = prepareData.StaticCamera;
    }

    auto Renderer::Submit(RenderSubmitInfo &&info) -> void {
        if (info.Data->MeshData.Data) {
            s_RenderingStats->IncrementVertexCount(info.Data->MeshData.Data->GetVertexBuffer()->GetCount());
            s_RenderingStats->IncrementIndexCount(info.Data->MeshData.Data->GetIndexBuffer()->GetCount());

            s_RenderingStats->IncrementObjectsCount(1);

            s_RenderingStats->IncrementMeshesCount(1);
        }

        RenderCommand::AddToRenderQueue(info.Id, std::move(info.Data), std::move(info.MatInfo));

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
        s_Spec = spec;

        s_ActiveAPI = s_Spec.Backend;
        s_ActiveRendererAPI = RendererBackend::Create(s_ActiveAPI);

        if (s_ActiveRendererAPI) {
            s_ActiveRendererAPI->Init();
        } else {
            MKT_THROW_RUNTIME_ERROR("Could not pick a valid render backend");
        }

        RenderCommand::Init(s_ActiveRendererAPI);

        // Initialize rendering structures
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


    auto Renderer::Submit( const std::string& id, const GameObject& objectData, const glm::mat4& transform,
                           std::shared_ptr<Material>& material ) -> void {
        auto data{ std::make_shared<GameObject>() };

        data->ModelName = objectData.ModelName;
        data->ModelPath = objectData.ModelPath;
        data->Color = objectData.Color;

        // The transform parameter comes from the entity's Transform component
        data->Transform.Transform = transform;
        data->MeshData = objectData.MeshData;

        data->Transform.Projection = s_DrawData->StaticCamera->GetProjection();
        data->Transform.View = s_DrawData->StaticCamera->GetViewMatrix();

        Submit(
                std::move( RenderSubmitInfo{
                        .Id = id,
                        .Data = data,
                        .MatInfo = material,
                } ) );
    }
    auto Renderer::RemoveFromDrawQueue(const std::string& id) -> bool {
        return RenderCommand::RemoveFromRenderQueue(id);
    }


    auto Renderer::UpdateRendererStatistics() -> void {
        switch (s_Spec.Backend) {
            case GraphicsAPI::VULKAN_API:
                auto& stats{ VulkanContext::GetDetailedStatistics() };
                s_Statistics.VRAMUsage = stats.total.statistics.allocationBytes;
                break;
        }
    }


    auto Renderer::SetLightsViewPos( const glm::vec4& viewPos ) -> void {
        s_LightViewPos = viewPos;
    }


    auto Renderer::SetPointLightInfo( PointLight& info, Size_T index ) -> void {
        s_PointLights[index] = info;
    }


    auto Renderer::SetDirLightInfo( DirectionalLight& info, Size_T index ) -> void {
        s_DirectionalLights[index] = info;
    }


    auto Renderer::SetSpotLightInfo( SpotLight& info, Size_T index ) -> void {
        s_SpotLights[index] = info;
    }

}
