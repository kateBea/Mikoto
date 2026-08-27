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

#ifndef MIKOTO_D3D11BUFFER_HH
#define MIKOTO_D3D11BUFFER_HH

#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>

#include <Renderer/Rhi/Buffer.hh>
#include <Renderer/Rhi/DeviceObject.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )

#include <d3d11.h>
#include <d3dcommon.h>
#include <dxgi1_3.h>
#include <wrl.h>

#include <Renderer/Rhi/D3D11/Direct3D11Libraries.hh>

namespace mikoto::renderer::d3d11 {

    class Buffer final : public rhi::IBuffer {
    public:
        explicit Buffer( const rhi::BufferCreateDescription &description );

        MKT_NODISCARD auto GetGpuDeviceAddress() -> rhi::DeviceAddress override;

        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object override;

        auto SetDebugName( const eastl::string_view name ) -> void override;

        ~Buffer() override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        Microsoft::WRL::ComPtr<ID3D11Buffer> mBuffer{};
    };
}// namespace mikoto::renderer::d3d11

#endif

#endif//MIKOTO_D3D11BUFFER_HH
