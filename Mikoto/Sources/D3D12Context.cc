//    Copyright 2026 ケイト
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
#include <Core/Exception.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

#include <Renderer/D3D12/D3D12Context.hh>

namespace Mikoto {

    auto D3D12Context::Init() -> bool {
        // Init the device when the context is ready
        m_Device = GpuDevice::Create({ .Api = GraphicsAPI::DIRECTX_12 });
        if (!m_Device) {
            MKT_THROW_RUNTIME_ERROR( "D3D12Context::Create - Could not initialize DIRECTX_12 GPU Device." );
        }
        m_Device->Init();

        return true;
    }

    auto D3D12Context::Shutdown() -> void {

    }

    auto D3D12Context::SubmitFrame() -> void {

    }

    auto D3D12Context::PrepareFrame() -> void {

    }

    auto D3D12Context::Present() -> void {

    }

    auto D3D12Context::SetPresentTarget( TextureHandle texture ) -> void {

    }

    auto D3D12Context::EnableVSync() -> void {

    }

    auto D3D12Context::DisableVSync() -> void {

    }
}

#endif
