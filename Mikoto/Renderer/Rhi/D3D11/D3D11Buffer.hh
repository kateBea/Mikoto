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

#ifndef MIKOTO_RENDERER_RHI_D3D11_D3D11_BUFFER_HH
#define MIKOTO_RENDERER_RHI_D3D11_D3D11_BUFFER_HH

#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>

#include <Renderer/Rhi/Buffer.hh>
#include <Renderer/Rhi/DeviceObject.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )

#include <Platform/Windows/Windows.hh>

#include <d3d11.h>
#include <d3dcommon.h>
#include <dxgi1_3.h>
#include <wrl.h>

#include <Renderer/Rhi/D3D11/Direct3D11Libraries.hh>

namespace mikoto::renderer::d3d11 {

    class Buffer final : public rhi::IBuffer {
    public:
        explicit Buffer( const rhi::BufferCreateDescription &description );
        
        auto SetDebugName( const eastl::string_view name ) -> void override;

        MKT_NODISCARD auto GetGpuDeviceAddress() -> rhi::DeviceAddress override;
        MKT_NODISCARD auto GetShaderDeviceAddress() -> rhi::DeviceAddress override;

        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object override;

        MKT_NODISCARD auto GetUsage() const -> rhi::BufferUsageFlags override;
        MKT_NODISCARD auto GetDataType() const -> rhi::BufferDataType override;
        MKT_NODISCARD auto GetData() const -> memory::BufferSpanHandle override;
        MKT_NODISCARD auto GetSizeBytes() const -> core::usize override;
        
        MKT_NODISCARD auto GetFormat() const -> rhi::Format override;
        MKT_NODISCARD auto GetCount() const -> core::usize override;

        MKT_NODISCARD operator ID3D11Resource*() const;

        ~Buffer() override;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        memory::BufferSpanHandle mUploadContents{};

        // These 2 attributes specify that the buffer is typed
        // meaning it will hold mElementCount elements of mElementSize size in bytes
        // This is useful for the backend API to manage alignment as it considers necessary
        // If it is going to be treated as raw bytes
        // then mElementSize holds the total size in bytes
        core::usize mElementCount{};
        core::usize mElementSize{};

        rhi::BufferDataType mDataType{ rhi::BufferDataType::eInvalid };
        rhi::BufferUsageFlags mUsage{ rhi::BufferUsageFlagsBits::None };

        const rhi::ResourceStates mInitialState;

        bool mIsVolatile{};
        core::usize mMaxVersions{ 0 };

        rhi::Format mFormat{ rhi::Format::eUnknown };

        Microsoft::WRL::ComPtr<ID3D11Buffer> mBuffer{};
    };
}// namespace mikoto::renderer::d3d11

#endif

#endif// MIKOTO_RENDERER_RHI_D3D11_D3D11_BUFFER_HH
