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

#ifndef MIKOTO_RENDERER_RHI_D3D11_D3D11_TEXTURE_HH
#define MIKOTO_RENDERER_RHI_D3D11_D3D11_TEXTURE_HH

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>

#include <Assets/Image.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Texture.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

#include <Platform/Windows/Windows.hh>

#include <d3d11.h>
#include <d3dcommon.h>
#include <dxgi1_3.h>
#include <wrl.h>

#include <Renderer/Rhi/D3D11/Direct3D11Libraries.hh>

namespace mikoto::renderer::d3d11 {

    class Sampler final : public rhi::ISampler {
    public:
        auto GetMinFilter() const -> rhi::SamplerFilter override;
        auto GetMagFilter() const -> rhi::SamplerFilter override;
        auto GetWrapU() const -> rhi::SamplerWrapMode override;
        auto GetWrapV() const -> rhi::SamplerWrapMode override;
        auto GetWrapW() const -> rhi::SamplerWrapMode override;
        auto GetMipLevels() const -> float override;

    private:
        float mMipLevels{ 1.0f };

        rhi::SamplerFilter mMinFilter{ rhi::SamplerFilter::eNearest };
        rhi::SamplerFilter mMagFilter{ rhi::SamplerFilter::eNearest };
        rhi::SamplerWrapMode mWrapU{ rhi::SamplerWrapMode::eRepeat };
        rhi::SamplerWrapMode mWrapV{ rhi::SamplerWrapMode::eRepeat };
        rhi::SamplerWrapMode mWrapW{ rhi::SamplerWrapMode::eRepeat };

        rhi::Color mBorderColor{};

    public:
        explicit Sampler( const rhi::SamplerCreateDescription& desc );

        auto SetDebugName( const eastl::string_view name ) -> void override;

        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object override;

        ~Sampler() override;

    private:
        auto Destroy() -> void override;
        auto Initialize() -> void override;

    private:
        Microsoft::WRL::ComPtr<ID3D11SamplerState> mSampler{};
    };

    class Texture final : public renderer::rhi::ITexture  {
    public:
        explicit Texture( const renderer::rhi::TextureCreateDescription& data );

        MKT_NODISCARD auto GetStorageSizeBytes() const -> core::usize override;

        MKT_NODISCARD auto GetFormat() const -> rhi::Format override;
        MKT_NODISCARD auto GetWidth() const -> core::u32 override;
        MKT_NODISCARD auto GetHeight() const -> core::u32 override;

        MKT_NODISCARD auto GetTextureUsage() const -> rhi::TextureUsageFlags override;

        MKT_NODISCARD auto GetImage() const -> asset::ImageHandle override;


        MKT_NODISCARD auto GetSizeBytes() const -> core::usize override;
        MKT_NODISCARD auto GetDimension() const -> rhi::TextureDimension override;
        MKT_NODISCARD auto GetSampleCount() const -> rhi::Multisampling override;

        MKT_NODISCARD auto GetMipLevelCount() const -> core::u32 override;

        auto SetDebugName( eastl::string_view name ) -> void override;

        MKT_NODISCARD auto GetNativeHandle( renderer::rhi::ObjectType type ) -> renderer::rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( renderer::rhi::ObjectType type ) const -> renderer::rhi::Object override;

        auto EnableUsage( renderer::rhi::ResourceStates state ) -> void;

        MKT_NODISCARD operator ID3D11Resource*() const;
        MKT_NODISCARD operator ID3D11RenderTargetView*() const;

        ~Texture() override;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        core::u32 mWidth{};
        core::u32 mHeight{};
        core::u32 mMipCount{ 1 };

        asset::ImageHandle mImageData{};
        memory::BufferSpanHandle mBufferSpan{};

        rhi::Format mFormat{ rhi::Format::eUnknown };
        rhi::TextureDimension mDimension{ rhi::TextureDimension::eInvalid };

        const rhi::ResourceStates mInitialState;
        rhi::TextureUsageFlags mTextureUsage{ rhi::TextureUsageFlagsBits::ShaderResource };
        rhi::Multisampling mMultisampling{ rhi::Multisampling::eMsaaX1 };
        rhi::TextureSubresourceSet mSubResources{};

        Microsoft::WRL::ComPtr<ID3D11Texture2D> mTexture{};
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> mRenderTargetView{};
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView> mDepthStencilTargetView{};
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mShaderResourceView{};

        bool mKeepInitializerResources{ false };
    };
}

#endif

#endif// MIKOTO_RENDERER_RHI_D3D11_D3D11_TEXTURE_HH

