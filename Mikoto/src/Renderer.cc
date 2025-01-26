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
#include <Renderer/Core/RenderQueue.hh>
#include <Renderer/Core/Renderer.hh>
#include <Renderer/Vulkan/VulkanContext.hh>

namespace Mikoto {

    auto Renderer::BeginScene(const ScenePrepareData& prepareData) -> void {
        s_DrawData->RuntimeCamera = prepareData.RuntimeCamera;
        s_DrawData->StaticCamera = prepareData.StaticCamera;
        s_DrawData->CameraPosition = prepareData.CameraPosition;
    }

    auto Renderer::EndScene() -> void {
        Flush();
    }

    auto Renderer::Flush() -> void {
        // Execute all the commands, render commands can include stuff not directly related to the API
        // For instance if we want to change the size of the viewport that is not a command
        // that is directly related to the API, but it is a command that is necessary for rendering
        // Even tho it is later needed to determine the size of the image we render to
        RenderQueue::Flush();

        // Request active API to flush all of its commands
        s_ActiveRendererAPI->Flush();
    }

    auto Renderer::Init(RendererSpec&& spec) -> void {
        s_Spec = spec;

        s_ActiveAPI = s_Spec.Backend;
        s_ActiveRendererAPI = IRendererBackend::Create(s_ActiveAPI);

        if (s_ActiveRendererAPI) {
            s_ActiveRendererAPI->Init();
        } else {
            MKT_THROW_RUNTIME_ERROR("Renderer - Could not pick a valid render API.");
        }

        s_DrawData = std::make_unique<ScenePrepareData>();
    }

    auto Renderer::Shutdown() -> void {
        if (s_ActiveRendererAPI)
            s_ActiveRendererAPI->Shutdown();

        delete s_ActiveRendererAPI;
    }

    auto Renderer::GetRendererStatistics() -> const RendererStatistics& {
        const auto elapsed{ TimeManager::GetTime() };

        // Update interval in seconds
        constexpr double UPDATE_INTERVAL{ 5.0 };
        static double lastTimeUpdate{ elapsed };

        if ( elapsed - lastTimeUpdate >= UPDATE_INTERVAL ) {
            UpdateRendererStatistics();
            lastTimeUpdate = elapsed;
        }

        return s_Statistics;
    }

    auto Renderer::AddLightObject( const std::string& id, const LightRenderInfo& info ) -> void {
        auto [itLightInfo, inserted]{ s_LightObjects.try_emplace( id, info ) };

        itLightInfo->second = info;

        if (inserted) {

            switch ( info.Type ) {
                case LightType::DIRECTIONAL_LIGHT_TYPE:
                    s_ActiveDirectionalLights++;
                break;
                case LightType::POINT_LIGHT_TYPE:
                    s_ActivePointLights++;
                break;
                case LightType::SPOT_LIGHT_TYPE:
                    s_ActiveSpotLights++;
                break;
            }
        }
    }

    auto Renderer::RemoveLightObject( const std::string& id ) -> void {
        auto it{ s_LightObjects.find( id ) };

        if (it != s_LightObjects.end()) {
            it->second.IsActive = false;

            switch ( it->second.Type ) {
                case LightType::DIRECTIONAL_LIGHT_TYPE:
                    s_ActiveDirectionalLights--;
                break;
                case LightType::POINT_LIGHT_TYPE:
                    s_ActivePointLights--;
                break;
                case LightType::SPOT_LIGHT_TYPE:
                    s_ActiveSpotLights--;
                break;
            }
        }
    }

    // auto Renderer::Submit( const std::string& id, const GameObject& objectData, const glm::mat4& transform, const std::shared_ptr<Material>& material ) -> void {
    //     const auto data{ std::make_shared<GameObject>() };
    //
    //     data->ModelName = objectData.ModelName;
    //     data->ModelPath = objectData.ModelPath;
    //     data->Color = objectData.Color;
    //
    //     // The transform parameter comes from the entity's Transform component
    //     data->Transform.Transform = transform;
    //     data->MeshData = objectData.MeshData;
    //
    //     data->Transform.Projection = s_DrawData->StaticCamera->GetProjection();
    //     data->Transform.View = s_DrawData->StaticCamera->GetViewMatrix();
    //
    //     Submit(
    //             std::move( RenderSubmitInfo{
    //                     .Id = id,
    //                     .Data = data,
    //                     .MatInfo = material,
    //             } ) );
    // }

    auto Renderer::UpdateRendererStatistics() -> void {
        switch (s_Spec.Backend) {
            case GraphicsAPI::VULKAN_API:
                auto& stats{ VulkanContext::GetDetailedStatistics() };
                s_Statistics.VRAMUsage = stats.total.statistics.allocationBytes;
                break;
        }
    }
}
