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

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/GraphicsContext.hh>

#include <Renderer/Vulkan/VulkanGraphicsContext.hh>

namespace Mikoto {
    auto PipelineDescription::UseShader( std::string_view path, ShaderStage stage ) -> void {
        Shaders[stage] = path;
    }

    // Should probably be created by the render service instead
    auto GraphicsContext::Create( GpuDevice* device ) -> Unique<GraphicsContext> {
        Unique<GraphicsContext> result{ nullptr };

        switch ( device->GetApi() ) {
            case GraphicsAPI::VULKAN_API:
                result = CreateScope<VulkanGraphicsContext>( device );
                break;
            default:
                MKT_CORE_LOGGER_CRITICAL( "RenderService::CreateRendererBackend - Error Unsupported renderer API!" );
                break;
        }

        return result;
    }
} // namespace Mikoto