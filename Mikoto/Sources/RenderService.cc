//    Copyright 2025 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <memory>
#include <utility>

#include <Common/Common.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>
#include <Logging/Logger.hh>
#include <ImGui/ImGuiService.hh>
#include <Renderer/Core/RenderService.hh>

namespace Mikoto {

    RenderService::RenderService(const RenderServiceCreateInfo& options)
        : m_Options{ options }
    {}

    auto RenderService::Init() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_INFO("Initializing RenderService...");

        InitContext();

        InitShaderLibrary();

        if (m_Options.EnableImGui) {
            InitGuiService();
        }

        m_IsInitialized = true;
    }

    auto RenderService::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!m_IsInitialized) {
            return;
        }

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down RenderService..." );

        m_ImguiService->Shutdown();
        m_ImguiService.reset();

        m_ShaderLibrary->Shutdown();
        m_ShaderLibrary.reset();

        m_Context->Shutdown();
        m_Context = nullptr;

        m_IsInitialized = false;
    }

    auto RenderService::Update(float) -> void {
        MKT_BEGIN_PROFILER_NAMED();

    }

    auto RenderService::PrepareFrame() const -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_Context->PrepareFrame();
        m_ImguiService->PrepareFrame();
    }

    auto RenderService::EndFrame() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_ImguiService->EndFrame();
        m_Context->SubmitFrame();
    }

    auto RenderService::PresentFrame() -> void {
        m_Context->Present();
    }

    auto RenderService::SetPresentTarget( TextureHandle texture ) -> void {
        return m_Context->SetPresentTarget( texture );
    }

    auto RenderService::IsGraphicsActive( const GraphicsAPI api ) const -> bool {
        return m_ActiveAPI == api;
    }

    auto RenderService::InitContext() -> void {
        const RenderContextCreateInfo createInfo{
            .Api{ m_Options.RendererAPI },
            .TargetWindow{ m_Options.TargetWindow },
        };

        m_Context = RenderContext::Create(createInfo);
        if (!m_Context->Init()) {
            MKT_THROW_RUNTIME_ERROR( "RenderSystem::Init - Could not initialize Render context." );
        }
    }

    auto RenderService::InitShaderLibrary() -> void {
        MKT_CORE_LOGGER_INFO( "Initializing ShaderLibrary..." );

        const ShaderLibraryDescription description{
            .RootPath{ "Resources/Shaders/vulkan-spirv" },
            .Device{ GetGpuDevice() },
        };

        m_ShaderLibrary = CreateScope<ShaderLibrary>( description );
        if (m_ShaderLibrary) {
            m_ShaderLibrary->Init();
        }

    }

    auto RenderService::InitGuiService() -> void {
        // Imgui service
        ImGuiServiceDescription imguiServiceCreateInfo{
            .Device{ GetGpuDevice() },
            .BackendApi{ m_ActiveAPI },
            .TargetWindow{ m_Options.TargetWindow }
        };

        m_ImguiService = CreateScope<ImGuiService>( imguiServiceCreateInfo );
        m_ImguiService->Init();
    }
}// namespace Mikoto
