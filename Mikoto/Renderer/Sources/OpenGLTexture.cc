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


#include <cstring>

#include <EASTL/algorithm.h>
#include <EASTL/utility.h>

#include <glm/glm.hpp>

#include <Core/Exception.hh>

#include <Renderer/Rhi/OpenGL/OpenGLHelpers.hh>
#include <Renderer/Rhi/OpenGL/OpenGLTexture.hh>

namespace mikoto::renderer::opengl {

    Texture::Texture( const renderer::rhi::TextureCreateDescription &desc, eastl::shared_ptr<DeviceState> state, GLuint native )
        : mWidth{ desc.mWidth }, mHeight{ desc.mHeight }, mDepth{ desc.mDimension == rhi::TextureDimension::eTexture3D ? desc.mDepth : 1u }, mMipCount{ desc.mMipCount }, mImageData{ desc.mImageHandle }, mBufferSpan{ desc.mBufferSpan }, mFormat{ desc.mFormat }, mDimension{ desc.mDimension }, mTextureUsage{ desc.mUsage }, mMultisampling{ desc.mMultisampling }, mInitialState{ desc.mInitialState }, mSubResources{ desc.mSubresourceSet }, mState{ eastl::move( state ) }, mName{ native }, mOwned{ native == 0 }, mKeepInitializers{ desc.mKeepInitializerResources } {
        mHeapType = desc.mHeapType;
        mResourceType = desc.mResourceType;
    }

    Texture::~Texture() {
        Destroy();
    }

    auto Texture::Initialize() -> void {
        mState->RequireOwner();

        if ( !mWidth || !mHeight || !mDepth || !mMipCount ) {
            MKT_THROW_INVALID_ARGUMENT( "Invalid texture dimensions." );
        }

        const auto format{ opengl::GetFormat( mFormat ) };
        mLayers = mSubResources.mNumArraySlices;

        if ( !mLayers || mLayers == renderer::rhi::TextureSubresourceSet::kAllArraySlices ) {
            mLayers = 1;
        }

        switch ( mDimension ) {
            case renderer::rhi::TextureDimension::eTexture2D:
                mTarget = GL_TEXTURE_2D;
                mLayers = 1;
                break;
            case renderer::rhi::TextureDimension::eTexture3D:
                mTarget = GL_TEXTURE_3D;
                mLayers = 1;
                break;
            case renderer::rhi::TextureDimension::eTexture2DArray:
                mTarget = GL_TEXTURE_2D_ARRAY;
                break;
            case renderer::rhi::TextureDimension::eTextureCube:
                mTarget = GL_TEXTURE_CUBE_MAP;
                mLayers = 6;
                break;
            case renderer::rhi::TextureDimension::eTextureCubeArray:
                mTarget = GL_TEXTURE_CUBE_MAP_ARRAY;

                if ( mLayers % 6 ) {
                    MKT_THROW_INVALID_ARGUMENT( "Cube arrays need a multiple of six layers." );
                }

                break;
            case renderer::rhi::TextureDimension::eTexture2DMS:
                mTarget = GL_TEXTURE_2D_MULTISAMPLE;
                mLayers = 1;
                break;
            case renderer::rhi::TextureDimension::eTexture2DMSArray:
                mTarget = GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
                break;
            default:
                Unsupported( "1D texture descriptions" );
        }

        const core::u32 samples{ 1u << static_cast<core::u32>( mMultisampling ) };
        const bool multisampled{ mTarget == GL_TEXTURE_2D_MULTISAMPLE || mTarget == GL_TEXTURE_2D_MULTISAMPLE_ARRAY };

        if ( ( samples > 1 ) != multisampled || ( multisampled && mMipCount != 1 ) ) {
            MKT_THROW_INVALID_ARGUMENT( "Texture dimension, mip count and sample count disagree." );
        }

        if ( !mOwned ) {

            if ( !glIsTexture( mName ) ) {
                MKT_THROW_INVALID_ARGUMENT( "Invalid native OpenGL texture." );
            }

            mIsAllocated = true;
            return;
        }

        glCreateTextures( mTarget, 1, &mName );

        if ( mTarget == GL_TEXTURE_2D_MULTISAMPLE ) {
            glTextureStorage2DMultisample( mName, samples, format.mInternal, mWidth, mHeight, GL_TRUE );
        }
        else if ( mTarget == GL_TEXTURE_2D_MULTISAMPLE_ARRAY ) {
            glTextureStorage3DMultisample( mName, samples, format.mInternal, mWidth, mHeight, mLayers, GL_TRUE );
        }
        else if ( mTarget == GL_TEXTURE_2D || mTarget == GL_TEXTURE_CUBE_MAP ) {
            glTextureStorage2D( mName, mMipCount, format.mInternal, mWidth, mHeight );
        }
        else {
            glTextureStorage3D( mName, mMipCount, format.mInternal, mWidth, mHeight, mTarget == GL_TEXTURE_3D ? mDepth : mLayers );
        }

        CheckError( "texture allocation" );

        for ( core::u32 mip{}; mip < mMipCount; ++mip ) {
            mAllocationBytes += core::usize{ glm::max( 1u, mWidth >> mip ) } * glm::max( 1u, mHeight >> mip ) * glm::max( 1u, mDepth >> mip ) * mLayers * samples * format.mBytes;
        }

        mState->mAllocatedBytes += mAllocationBytes;

        if ( !multisampled ) {
            glTextureParameteri( mName, GL_TEXTURE_MIN_FILTER,
                format.mInteger ? GL_NEAREST : ( mMipCount > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR ) );
            glTextureParameteri( mName, GL_TEXTURE_MAG_FILTER, format.mInteger ? GL_NEAREST : GL_LINEAR );
            glTextureParameteri( mName, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
            glTextureParameteri( mName, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
            glTextureParameteri( mName, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
            const auto data{ mBufferSpan ? mBufferSpan : ( mImageData ? mImageData->mBufferSpan : memory::BufferSpanHandle{} ) };

            if ( data ) {
                Upload( data->GetData(), data->GetSize() );
            }
        }

        if ( !mKeepInitializers ) {
            mImageData.Reset();
            mBufferSpan.Reset();
        }

        SetResourceState( mInitialState );
        mIsAllocated = true;
    }

    auto Texture::Upload( const void *data, core::usize size ) -> void {
        mState->RequireOwner();
        const auto format{ opengl::GetFormat( mFormat ) };

        if ( GetSampleCount() != renderer::rhi::Multisampling::eMsaaX1 ) {
            Unsupported( "uploading multisampled textures" );
        }

        GLint alignment{};
        glGetIntegerv( GL_UNPACK_ALIGNMENT, &alignment );
        glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
        const auto regions{ rhi::GetTextureUploadRegions( *this, size ) };
        for ( const auto &r: regions ) {
            // A null data pointer denotes offset zero in a bound unpack buffer.
            const auto pointer{ reinterpret_cast<const void *>( reinterpret_cast<uintptr_t>( data ) + r.mOffset ) };
            if ( mTarget == GL_TEXTURE_2D ) {
                glTextureSubImage2D( mName, r.mMip, 0, 0, r.mWidth, r.mHeight, format.mExternal, format.mType, pointer );
            }
            else {
                glTextureSubImage3D( mName, r.mMip, 0, 0, r.mLayer, r.mWidth, r.mHeight,
                    mTarget == GL_TEXTURE_3D ? r.mDepth : 1, format.mExternal, format.mType, pointer );
            }
        }
        glPixelStorei( GL_UNPACK_ALIGNMENT, alignment );
        if ( regions.size() == mLayers && mMipCount > 1 && !format.mInteger && !format.mDepth ) {
            glGenerateTextureMipmap( mName );
        }

        CheckError( "texture upload" );
    }

    auto Texture::Attach( GLuint framebuffer, GLenum attachment, const renderer::rhi::TextureSubresourceSet &subresources ) const -> void {
        if ( subresources.mBaseMipLevel >= mMipCount || subresources.mBaseArraySlice >= mLayers ) {
            MKT_THROW_OUT_OF_RANGE( "Framebuffer attachment subresource is invalid." );
        }

        if ( mLayers == 1 ) {
            glNamedFramebufferTexture( framebuffer, attachment, mName, subresources.mBaseMipLevel );
        }
        else if ( subresources.mNumArraySlices == renderer::rhi::TextureSubresourceSet::kAllArraySlices && subresources.mBaseArraySlice == 0 ) {
            glNamedFramebufferTexture( framebuffer, attachment, mName, subresources.mBaseMipLevel );
        }
        else if ( subresources.mNumArraySlices == 1 ) {
            glNamedFramebufferTextureLayer( framebuffer, attachment, mName, subresources.mBaseMipLevel, subresources.mBaseArraySlice );
        }
        else {
            Unsupported( "framebuffer attachment views spanning a subset of layers" );
        }
    }

    auto Texture::SampledView( const renderer::rhi::BindingTableItem &item ) const -> GLuint {
        mState->RequireOwner();
        auto range{ item.mSubResourceSet };

        if ( range.mBaseMipLevel >= mMipCount || range.mBaseArraySlice >= mLayers ) {
            MKT_THROW_OUT_OF_RANGE( "Invalid texture view base subresource." );
        }

        if ( range.mNumMipLevels == renderer::rhi::TextureSubresourceSet::kAllMipLevels ) {
            range.mNumMipLevels = mMipCount - range.mBaseMipLevel;
        }

        if ( range.mNumArraySlices == renderer::rhi::TextureSubresourceSet::kAllArraySlices ) {
            range.mNumArraySlices = mLayers - range.mBaseArraySlice;
        }

        if ( !range.mNumMipLevels || range.mNumMipLevels > mMipCount - range.mBaseMipLevel || !range.mNumArraySlices ||
             range.mNumArraySlices > mLayers - range.mBaseArraySlice ) {
            MKT_THROW_OUT_OF_RANGE( "Texture view range exceeds allocation." );
        }

        const auto dimension{ item.mDimension == renderer::rhi::TextureDimension::eInvalid ? mDimension : item.mDimension };
        const auto format{ item.mFormat == renderer::rhi::Format::eUnknown ? mFormat : item.mFormat };

        if ( dimension == mDimension && format == mFormat && range.mBaseMipLevel == 0 && range.mNumMipLevels == mMipCount &&
             range.mBaseArraySlice == 0 && range.mNumArraySlices == mLayers ) {
            return mName;
        }

        for ( const auto &view: mViews ) {

            if ( view.mRange.mBaseMipLevel == range.mBaseMipLevel && view.mRange.mNumMipLevels == range.mNumMipLevels &&
                 view.mRange.mBaseArraySlice == range.mBaseArraySlice && view.mRange.mNumArraySlices == range.mNumArraySlices &&
                 view.mDimension == dimension && view.mFormat == format ) {
                return view.mName;
            }
        }

        GLenum target{};

        switch ( dimension ) {
            case renderer::rhi::TextureDimension::eTexture2D:
                target = GL_TEXTURE_2D;
                break;
            case renderer::rhi::TextureDimension::eTexture2DArray:
                target = GL_TEXTURE_2D_ARRAY;
                break;
            case renderer::rhi::TextureDimension::eTextureCube:
                target = GL_TEXTURE_CUBE_MAP;
                break;
            case renderer::rhi::TextureDimension::eTextureCubeArray:
                target = GL_TEXTURE_CUBE_MAP_ARRAY;
                break;
            case renderer::rhi::TextureDimension::eTexture2DMS:
                target = GL_TEXTURE_2D_MULTISAMPLE;
                break;
            case renderer::rhi::TextureDimension::eTexture2DMSArray:
                target = GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
                break;
            default:
                Unsupported( "this texture view dimension" );
        }

        GLuint name{};
        glGenTextures( 1, &name );
        glTextureView( name, target, mName, opengl::GetFormat( format ).mInternal, range.mBaseMipLevel, range.mNumMipLevels,
            range.mBaseArraySlice, range.mNumArraySlices );

        try {
            CheckError( "texture view creation" );
        } catch ( ... ) {
            glDeleteTextures( 1, &name );
            throw;
        }

        if ( GetSampleCount() == renderer::rhi::Multisampling::eMsaaX1 ) {
            glTextureParameteri( name, GL_TEXTURE_MIN_FILTER, opengl::GetFormat( format ).mInteger ? GL_NEAREST : GL_LINEAR );
            glTextureParameteri( name, GL_TEXTURE_MAG_FILTER, opengl::GetFormat( format ).mInteger ? GL_NEAREST : GL_LINEAR );
        }

        mViews.push_back( { range, dimension, format, name } );
        return name;
    }

    auto Texture::Destroy() -> void {
        if ( !mViews.empty() ) {
            mState->Retire( [views = eastl::move( mViews )] {
                for ( const auto &view: views ) {
                    glDeleteTextures( 1, &view.mName );
                }
            } );
        }

        if ( mOwned && mName ) {
            const auto name{ eastl::exchange( mName, 0 ) };
            const auto bytes{ eastl::exchange( mAllocationBytes, 0 ) };
            const auto state{ mState.get() };
            mState->Retire( [name, bytes, state] {
                glDeleteTextures( 1, &name );
                state->mAllocatedBytes -= bytes;
            } );
        }

        mIsAllocated = false;
    }

    auto Texture::GetNativeHandle( renderer::rhi::ObjectType type ) -> renderer::rhi::Object {
        return type == renderer::rhi::ObjectType::GL_Texture ? renderer::rhi::Object{ core::u64{ mName } } : renderer::rhi::Object{};
    }

    auto Texture::GetStorageSizeBytes() const -> core::usize {
        const auto &format{ rhi::GetFormatInfo( mFormat ) };
        const core::usize block{ eastl::max<core::usize>( 1, format.mBlockSize ) };
        core::usize width{ mWidth };
        core::usize height{ mHeight };
        core::usize depth{ mDepth };
        core::usize result{};

        for ( core::u32 mip{}; mip < mMipCount; ++mip ) {
            result += ( ( width + block - 1 ) / block ) * ( ( height + block - 1 ) / block ) * depth * format.mBytesPerBlock;
            width = eastl::max<core::usize>( 1, width / 2 );
            height = eastl::max<core::usize>( 1, height / 2 );
            depth = eastl::max<core::usize>( 1, depth / 2 );
        }

        return result * ( mDimension == rhi::TextureDimension::eTextureCube ? 6 : 1 );
    }

    auto Texture::GetFormat() const -> rhi::Format {
        return mFormat;
    }

    auto Texture::GetWidth() const -> core::u32 {
        return mWidth;
    }

    auto Texture::GetHeight() const -> core::u32 {
        return mHeight;
    }

    auto Texture::GetDepth() const -> core::u32 {
        return mDepth;
    }

    auto Texture::GetTextureUsage() const -> rhi::TextureUsageFlags {
        return mTextureUsage;
    }

    auto Texture::GetImage() const -> asset::ImageHandle {
        return mImageData;
    }

    auto Texture::GetSizeBytes() const -> core::usize {
        if ( mImageData && mImageData->mBufferSpan ) {
            return mImageData->mBufferSpan->GetSize();
        }

        return mBufferSpan ? mBufferSpan->GetSize() : 0;
    }

    auto Texture::GetDimension() const -> rhi::TextureDimension {
        return mDimension;
    }

    auto Texture::GetSampleCount() const -> rhi::Multisampling {
        return mMultisampling;
    }

    auto Texture::GetMipLevelCount() const -> core::u32 {
        return mMipCount;
    }

    auto Texture::Name() const -> GLuint {
        return mName;
    }

    auto Texture::Target() const -> GLenum {
        return mTarget;
    }

    auto Texture::Layers() const -> core::u32 {
        return mLayers;
    }

    Sampler::Sampler( const renderer::rhi::SamplerCreateDescription &desc, eastl::shared_ptr<DeviceState> state )
        : mMipLevels{ desc.mMipLevels }, mMinFilter{ desc.mMinFilter }, mMagFilter{ desc.mMagFilter }, mWrapU{ desc.mWrapU }, mWrapV{ desc.mWrapV }, mWrapW{ desc.mWrapW }, mBorderColor{ desc.mBorderColor }, mDesc{ desc }, mState{ eastl::move( state ) } {
    }

    Sampler::~Sampler() {
        Destroy();
    }

    auto Sampler::Initialize() -> void {
        mState->RequireOwner();
        glCreateSamplers( 1, &mName );
        glSamplerParameteri( mName, GL_TEXTURE_MIN_FILTER, opengl::GetSamplerMinFilter( mDesc.mMinFilter, mDesc.mMipMapMode ) );
        glSamplerParameteri( mName, GL_TEXTURE_MAG_FILTER, opengl::GetSamplerFilter( mDesc.mMagFilter ) );
        glSamplerParameteri( mName, GL_TEXTURE_WRAP_S, opengl::GetSamplerWrap( mDesc.mWrapU ) );
        glSamplerParameteri( mName, GL_TEXTURE_WRAP_T, opengl::GetSamplerWrap( mDesc.mWrapV ) );
        glSamplerParameteri( mName, GL_TEXTURE_WRAP_R, opengl::GetSamplerWrap( mDesc.mWrapW ) );
        glSamplerParameterf( mName, GL_TEXTURE_MIN_LOD, 0 );
        glSamplerParameterf( mName, GL_TEXTURE_MAX_LOD, glm::max( 0.0f, mDesc.mMipLevels - 1.0f ) );
        if ( mDesc.mMaxAnisotropy > 1 && mDesc.mMinFilter == rhi::SamplerFilter::eLinear && mDesc.mMagFilter == rhi::SamplerFilter::eLinear ) {
            GLint extensions{};
            glGetIntegerv( GL_NUM_EXTENSIONS, &extensions );
            for ( GLint index{}; index < extensions; ++index ) {
                const auto *name{ reinterpret_cast<const char *>( glGetStringi( GL_EXTENSIONS, index ) ) };
                if ( std::strcmp( name, "GL_EXT_texture_filter_anisotropic" ) == 0 || std::strcmp( name, "GL_ARB_texture_filter_anisotropic" ) == 0 ) {
                    constexpr GLenum maxAnisotropy{ 0x84FF }, textureAnisotropy{ 0x84FE };
                    GLfloat limit{};
                    glGetFloatv( maxAnisotropy, &limit );
                    glSamplerParameterf( mName, textureAnisotropy, glm::min( limit, mDesc.mMaxAnisotropy ) );
                    break;
                }
            }
        }
        glSamplerParameterfv( mName, GL_TEXTURE_BORDER_COLOR, &mDesc.mBorderColor.mR );
        glSamplerParameteri( mName, GL_TEXTURE_COMPARE_MODE, mDesc.mCompareOp == renderer::rhi::CompareOp::eNever ? GL_NONE : GL_COMPARE_REF_TO_TEXTURE );
        glSamplerParameteri( mName, GL_TEXTURE_COMPARE_FUNC, opengl::GetCompareOp( mDesc.mCompareOp ) );
        CheckError( "sampler creation" );
        mIsAllocated = true;
    }

    auto Sampler::Destroy() -> void {
        if ( mName ) {
            const auto name{ eastl::exchange( mName, 0 ) };
            mState->Retire( [name] {
                glDeleteSamplers( 1, &name );
            } );
        }

        mIsAllocated = false;
    }

    auto Sampler::GetNativeHandle( renderer::rhi::ObjectType type ) -> renderer::rhi::Object {
        return type == renderer::rhi::ObjectType::GL_Sampler ? renderer::rhi::Object{ core::u64{ mName } } : renderer::rhi::Object{};
    }

    auto Sampler::GetMinFilter() const -> rhi::SamplerFilter {
        return mMinFilter;
    }

    auto Sampler::GetMagFilter() const -> rhi::SamplerFilter {
        return mMagFilter;
    }

    auto Sampler::GetWrapU() const -> rhi::SamplerWrapMode {
        return mWrapU;
    }

    auto Sampler::GetWrapV() const -> rhi::SamplerWrapMode {
        return mWrapV;
    }

    auto Sampler::GetWrapW() const -> rhi::SamplerWrapMode {
        return mWrapW;
    }

    auto Sampler::GetMipLevels() const -> float {
        return mMipLevels;
    }

    auto Sampler::Name() const -> GLuint {
        return mName;
    }
}// namespace mikoto::renderer::opengl
