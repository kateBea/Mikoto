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

#ifndef MIKOTO_RENDERER_RHI_OPENGL_OPENGL_TEXTURE_HH
#define MIKOTO_RENDERER_RHI_OPENGL_OPENGL_TEXTURE_HH

#include <Renderer/Rhi/OpenGL/OpenGLDevice.hh>
#include <Renderer/Rhi/Texture.hh>

namespace mikoto::renderer::opengl {


    class Sampler final : public renderer::rhi::ISampler {
    public:
        Sampler( const renderer::rhi::SamplerCreateDescription &desc, eastl::shared_ptr<DeviceState> state );

        MKT_NODISCARD auto GetMinFilter() const -> rhi::SamplerFilter override;
        MKT_NODISCARD auto GetMagFilter() const -> rhi::SamplerFilter override;

        MKT_NODISCARD auto GetWrapU() const -> rhi::SamplerWrapMode override;
        MKT_NODISCARD auto GetWrapV() const -> rhi::SamplerWrapMode override;
        MKT_NODISCARD auto GetWrapW() const -> rhi::SamplerWrapMode override;

        MKT_NODISCARD auto GetMipLevels() const -> float override;

        MKT_NODISCARD auto GetNativeHandle( renderer::rhi::ObjectType type ) -> renderer::rhi::Object override;

        auto Name() const -> GLuint;

        ~Sampler() override;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        core::f32 mMipLevels{ 1.0f };

        rhi::SamplerFilter mMinFilter{ rhi::SamplerFilter::eNearest };
        rhi::SamplerFilter mMagFilter{ rhi::SamplerFilter::eNearest };
        rhi::SamplerWrapMode mWrapU{ rhi::SamplerWrapMode::eRepeat };
        rhi::SamplerWrapMode mWrapV{ rhi::SamplerWrapMode::eRepeat };
        rhi::SamplerWrapMode mWrapW{ rhi::SamplerWrapMode::eRepeat };

        rhi::Color mBorderColor{};

        renderer::rhi::SamplerCreateDescription mDesc{};
        // Context ownership and native resource state.
        eastl::shared_ptr<DeviceState> mState{};
        GLuint mName{};
    };

    class Texture final : public renderer::rhi::ITexture {
    public:
        Texture( const renderer::rhi::TextureCreateDescription &desc, eastl::shared_ptr<DeviceState> state, GLuint native = 0 );

        MKT_NODISCARD auto GetSizeBytes() const -> core::usize override;
        MKT_NODISCARD auto GetStorageSizeBytes() const -> core::usize override;

        MKT_NODISCARD auto GetFormat() const -> rhi::Format override;

        MKT_NODISCARD auto GetWidth() const -> core::u32 override;
        MKT_NODISCARD auto GetHeight() const -> core::u32 override;
        MKT_NODISCARD auto GetDepth() const -> core::u32 override;
        MKT_NODISCARD auto GetTextureUsage() const -> rhi::TextureUsageFlags override;

        MKT_NODISCARD auto GetImage() const -> asset::ImageHandle override;

        MKT_NODISCARD auto GetDimension() const -> rhi::TextureDimension override;
        MKT_NODISCARD auto GetSampleCount() const -> rhi::Multisampling override;
        MKT_NODISCARD auto GetMipLevelCount() const -> core::u32 override;

        MKT_NODISCARD auto GetNativeHandle( renderer::rhi::ObjectType type ) -> renderer::rhi::Object override;

        auto Name() const -> GLuint;

        auto Target() const -> GLenum;
        auto Layers() const -> core::u32;

        auto Upload( const void *data, core::usize size ) -> void;

        auto SampledView( const renderer::rhi::BindingTableItem &item ) const -> GLuint;

        auto Attach( GLuint framebuffer, GLenum attachment, const renderer::rhi::TextureSubresourceSet &subresources ) const -> void;

        ~Texture() override;

    private:
        // Cached views of the texture's subresources.
        struct View {
            renderer::rhi::TextureSubresourceSet mRange{};
            renderer::rhi::TextureDimension mDimension{};
            renderer::rhi::Format mFormat{};
            GLuint mName{};
        };

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        core::u32 mWidth{};
        core::u32 mHeight{};
        core::u32 mDepth{ 1 };
        core::u32 mMipCount{ 1 };

        asset::ImageHandle mImageData{};
        memory::BufferSpanHandle mBufferSpan{};

        rhi::Format mFormat{ rhi::Format::eUnknown };
        rhi::TextureDimension mDimension{ rhi::TextureDimension::eInvalid };

        const rhi::ResourceStates mInitialState;

        rhi::TextureUsageFlags mTextureUsage{ rhi::TextureUsageFlagsBits::ShaderResource };

        rhi::Multisampling mMultisampling{ rhi::Multisampling::eMsaaX1 };

        rhi::TextureSubresourceSet mSubResources{};

        // Context ownership and native resource state.
        eastl::shared_ptr<DeviceState> mState{};

        GLuint mName{};
        GLenum mTarget{};
        core::u32 mLayers{ 1 };
        core::usize mAllocationBytes{};

        bool mOwned{ true };
        bool mKeepInitializers{};

        mutable eastl::vector<View> mViews{};
    };
}// namespace mikoto::renderer::opengl

#endif// MIKOTO_RENDERER_RHI_OPENGL_OPENGL_TEXTURE_HH
