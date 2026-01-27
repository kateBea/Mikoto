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

#include <Core/Platform.hh>

#include <Library/Utility/Types.hh>

#include <Renderer/Core/RenderService.hh>
#include <Renderer/Vulkan/VulkanContext.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)
#include <Renderer/D3D11/D3D11Context.hh>
#include <Renderer/D3D12/D3D12Context.hh>
#endif

namespace Mikoto {

    auto RenderContext::Create( const RenderContextCreateInfo& config ) -> Unique<RenderContext> {
        Unique<RenderContext> result{ nullptr };
        switch (config.Api) {
            case GraphicsAPI::VULKAN_API:
                result = CreateScope<VulkanContext>( config );
                break;
#if defined(MIKOTO_PLATFORM_WINDOWS)
            case GraphicsAPI::DIRECTX_11:
                result = CreateScope<D3D11Context>( config );
                break;
            case GraphicsAPI::DIRECTX_12:
                result = CreateScope<D3D12Context>( config );
#endif

            default:;
        }

        return result;
    }
}