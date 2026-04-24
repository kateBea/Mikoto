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

#ifndef MIKOTO_D3D11_TEXTURE_HH
#define MIKOTO_D3D11_TEXTURE_HH

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>

#include <Assets/Image.hh>

#include <Renderer/Core/Rhi.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

#include <d3d11.h>
#include <dxgi1_3.h>
#include <wrl.h>
#include <d3dcommon.h>

#include <Renderer/D3D11/Direct3D11Libraries.hh>

namespace mikoto::renderer::d3d11 {

    class Sampler final : public rhi::ISampler {
    public:
        explicit Sampler( const rhi::SamplerCreateDescription& desc );

        auto SetDebugName( const eastl::string_view name ) -> void override;

        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object override;

        ~Sampler() override;

    private:
        auto Release() -> void override;
        auto Initialize() -> void override;

    private:
        Microsoft::WRL::ComPtr<ID3D11SamplerState> mSampler{};
    };

    class Texture final : public ITexture  {
    public:
        explicit Texture( const TextureCreateDescription& data );

        auto SetDebugName( eastl::string_view name ) -> void override;

        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;
        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) const -> Object override;

        auto EnableUsage( ResourceStates state ) -> void;

        ~Texture() override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        Microsoft::WRL::ComPtr<ID3D11Texture2D> mTexture{};
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> mRenderTargetView{};
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView> mDepthStencilTargetView{};
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mShaderResourceView{};

        bool mKeepInitializerResources{ false };
    };
}

#endif

#endif//MIKOTO_D3D11_TEXTURE_HH
