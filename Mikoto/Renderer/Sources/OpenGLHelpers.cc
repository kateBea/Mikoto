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

#include <array>
#include <cstring>

#include <glm/glm.hpp>

#include <Core/Core.hh>
#include <Core/Exception.hh>

#include <Renderer/Rhi/OpenGL/OpenGLHelpers.hh>
#include <Renderer/Rhi/OpenGL/OpenGLTexture.hh>

namespace mikoto::renderer::opengl {

    auto Unsupported( const char *feature ) -> void {
        MKT_THROW_RUNTIME_ERROR( string::Format( "OpenGL 4.6 does not support {}.", feature ) );
    }

    auto CheckError( const char *operation ) -> void {
        const auto error{ glGetError() };

        if ( error != GL_NO_ERROR ) {
            MKT_THROW_RUNTIME_ERROR( string::Format( "OpenGL error {} during {}.", error, operation ) );
        }
    }

    auto GetFormat( renderer::rhi::Format format ) -> TextureFormat {
        switch ( format ) {
#define MKT_GL_FORMAT( name, internal, external, type, bytes ) \
    case renderer::rhi::Format::name:                          \
        return { internal, external, type, bytes };
            MKT_GL_FORMAT( eR8_UNORM, GL_R8, GL_RED, GL_UNSIGNED_BYTE, 1 )
            MKT_GL_FORMAT( eR8_SNORM, GL_R8_SNORM, GL_RED, GL_BYTE, 1 )
            MKT_GL_FORMAT( eRG8_UNORM, GL_RG8, GL_RG, GL_UNSIGNED_BYTE, 2 )
            MKT_GL_FORMAT( eRG8_SNORM, GL_RG8_SNORM, GL_RG, GL_BYTE, 2 )
            MKT_GL_FORMAT( eRGBA8_UNORM, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 4 )
            MKT_GL_FORMAT( eRGBA8_SNORM, GL_RGBA8_SNORM, GL_RGBA, GL_BYTE, 4 )
            MKT_GL_FORMAT( eBGRA8_UNORM, GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE, 4 )
            MKT_GL_FORMAT( eSRGBA8_UNORM, GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, 4 )
            MKT_GL_FORMAT( eSBGRA8_UNORM, GL_SRGB8_ALPHA8, GL_BGRA, GL_UNSIGNED_BYTE, 4 )
            MKT_GL_FORMAT( eR16_UNORM, GL_R16, GL_RED, GL_UNSIGNED_SHORT, 2 )
            MKT_GL_FORMAT( eR16_SNORM, GL_R16_SNORM, GL_RED, GL_SHORT, 2 )
            MKT_GL_FORMAT( eRG16_UNORM, GL_RG16, GL_RG, GL_UNSIGNED_SHORT, 4 )
            MKT_GL_FORMAT( eRG16_SNORM, GL_RG16_SNORM, GL_RG, GL_SHORT, 4 )
            MKT_GL_FORMAT( eRGBA16_UNORM, GL_RGBA16, GL_RGBA, GL_UNSIGNED_SHORT, 8 )
            MKT_GL_FORMAT( eRGBA16_SNORM, GL_RGBA16_SNORM, GL_RGBA, GL_SHORT, 8 )
            MKT_GL_FORMAT( eR16_FLOAT, GL_R16F, GL_RED, GL_HALF_FLOAT, 2 )
            MKT_GL_FORMAT( eRG16_FLOAT, GL_RG16F, GL_RG, GL_HALF_FLOAT, 4 )
            MKT_GL_FORMAT( eRGBA16_FLOAT, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, 8 )
            MKT_GL_FORMAT( eR32_FLOAT, GL_R32F, GL_RED, GL_FLOAT, 4 )
            MKT_GL_FORMAT( eRG32_FLOAT, GL_RG32F, GL_RG, GL_FLOAT, 8 )
            MKT_GL_FORMAT( eRGB32_FLOAT, GL_RGB32F, GL_RGB, GL_FLOAT, 12 )
            MKT_GL_FORMAT( eRGBA32_FLOAT, GL_RGBA32F, GL_RGBA, GL_FLOAT, 16 )
            MKT_GL_FORMAT( eR10G10B10A2_UNORM, GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, 4 )
            MKT_GL_FORMAT( eR11G11B10_FLOAT, GL_R11F_G11F_B10F, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV, 4 )
#undef MKT_GL_FORMAT
#define MKT_GL_INTEGER( name, internal, external, type, bytes, sign ) \
    case renderer::rhi::Format::name:                                 \
        return { internal, external, type, bytes, false, false, true, sign };
            MKT_GL_INTEGER( eR8_UINT, GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE, 1, false )
            MKT_GL_INTEGER( eR8_SINT, GL_R8I, GL_RED_INTEGER, GL_BYTE, 1, true )
            MKT_GL_INTEGER( eR16_UINT, GL_R16UI, GL_RED_INTEGER, GL_UNSIGNED_SHORT, 2, false )
            MKT_GL_INTEGER( eR16_SINT, GL_R16I, GL_RED_INTEGER, GL_SHORT, 2, true )
            MKT_GL_INTEGER( eR32_UINT, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, 4, false )
            MKT_GL_INTEGER( eR32_SINT, GL_R32I, GL_RED_INTEGER, GL_INT, 4, true )
            MKT_GL_INTEGER( eRG8_UINT, GL_RG8UI, GL_RG_INTEGER, GL_UNSIGNED_BYTE, 2, false )
            MKT_GL_INTEGER( eRG8_SINT, GL_RG8I, GL_RG_INTEGER, GL_BYTE, 2, true )
            MKT_GL_INTEGER( eRG16_UINT, GL_RG16UI, GL_RG_INTEGER, GL_UNSIGNED_SHORT, 4, false )
            MKT_GL_INTEGER( eRG16_SINT, GL_RG16I, GL_RG_INTEGER, GL_SHORT, 4, true )
            MKT_GL_INTEGER( eRG32_UINT, GL_RG32UI, GL_RG_INTEGER, GL_UNSIGNED_INT, 8, false )
            MKT_GL_INTEGER( eRG32_SINT, GL_RG32I, GL_RG_INTEGER, GL_INT, 8, true )
            MKT_GL_INTEGER( eRGBA8_UINT, GL_RGBA8UI, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, 4, false )
            MKT_GL_INTEGER( eRGBA8_SINT, GL_RGBA8I, GL_RGBA_INTEGER, GL_BYTE, 4, true )
            MKT_GL_INTEGER( eRGBA16_UINT, GL_RGBA16UI, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, 8, false )
            MKT_GL_INTEGER( eRGBA16_SINT, GL_RGBA16I, GL_RGBA_INTEGER, GL_SHORT, 8, true )
            MKT_GL_INTEGER( eRGBA32_UINT, GL_RGBA32UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT, 16, false )
            MKT_GL_INTEGER( eRGBA32_SINT, GL_RGBA32I, GL_RGBA_INTEGER, GL_INT, 16, true )

#undef MKT_GL_INTEGER
            case renderer::rhi::Format::eRGB32_UINT:
                return { GL_RGB32UI, GL_RGB_INTEGER, GL_UNSIGNED_INT, 12, false, false, true, false };
            case renderer::rhi::Format::eRGB32_SINT:
                return { GL_RGB32I, GL_RGB_INTEGER, GL_INT, 12, false, false, true, true };
            case renderer::rhi::Format::eD16:
                return { GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, 2, true };
            case renderer::rhi::Format::eD32:
                return { GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, 4, true };
            case renderer::rhi::Format::eD24S8:
                return { GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, 4, true, true };
            case renderer::rhi::Format::eD32S8:
                return { GL_DEPTH32F_STENCIL8, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, 8, true, true };
            default:
                Unsupported( "this texture/vertex format" );
        }
    }

    auto GetCompareOp( renderer::rhi::CompareOp op ) -> GLenum {
        static constexpr std::array<GLenum, 8> values{ GL_NEVER, GL_LESS, GL_EQUAL, GL_LEQUAL, GL_GREATER, GL_NOTEQUAL, GL_GEQUAL, GL_ALWAYS };
        const auto index{ static_cast<core::usize>( op ) };

        if ( index >= values.size() ) {
            MKT_THROW_INVALID_ARGUMENT( "Invalid comparison operation." );
        }

        return values[index];
    }

    auto GetTopology( renderer::rhi::PrimitiveTopology topology ) -> GLenum {
        switch ( topology ) {
            case renderer::rhi::PrimitiveTopology::ePointList:
                return GL_POINTS;
            case renderer::rhi::PrimitiveTopology::eLineList:
                return GL_LINES;
            case renderer::rhi::PrimitiveTopology::eLineStrip:
                return GL_LINE_STRIP;
            case renderer::rhi::PrimitiveTopology::eTriangleList:
                return GL_TRIANGLES;
            case renderer::rhi::PrimitiveTopology::eTriangleStrip:
                return GL_TRIANGLE_STRIP;
            case renderer::rhi::PrimitiveTopology::eTriangleFan:
                return GL_TRIANGLE_FAN;
            default:
                MKT_THROW_INVALID_ARGUMENT( "Invalid primitive topology." );
        }
    }

    auto GetShaderModuleStage( rhi::ShaderType stage ) -> GLenum {
        switch ( stage ) {
            case rhi::ShaderType::eVertex:
                return GL_VERTEX_SHADER;
            case rhi::ShaderType::ePixel:
                return GL_FRAGMENT_SHADER;
            case rhi::ShaderType::eCompute:
                return GL_COMPUTE_SHADER;
            case rhi::ShaderType::eGeometry:
                return GL_GEOMETRY_SHADER;
            case rhi::ShaderType::eHull:
                return GL_TESS_CONTROL_SHADER;
            case rhi::ShaderType::eDomain:
                return GL_TESS_EVALUATION_SHADER;
            default:
                Unsupported( "this shader stage" );
        }
    }

    auto GetCullMode( rhi::CullMode mode ) -> GLenum {
        switch ( mode ) {
            case rhi::CullMode::eNone:
            case rhi::CullMode::eCullBack:
                return GL_BACK;
            case rhi::CullMode::eCullFront:
                return GL_FRONT;
            default:
                MKT_THROW_INVALID_ARGUMENT( "Invalid cull mode." );
        }
    }

    auto GetWindingOrder( rhi::WindingOrder order ) -> GLenum {
        switch ( order ) {
            case rhi::WindingOrder::eClockwise:
                return GL_CW;
            case rhi::WindingOrder::eCounterClockwise:
                return GL_CCW;
            default:
                MKT_THROW_INVALID_ARGUMENT( "Invalid winding order." );
        }
    }

    auto GetPolygonMode( rhi::PolygonMode mode ) -> GLenum {
        switch ( mode ) {
            case rhi::PolygonMode::eLines:
                return GL_LINE;
            case rhi::PolygonMode::ePoint:
                return GL_POINT;
            case rhi::PolygonMode::eFill:
                return GL_FILL;
            default:
                MKT_THROW_INVALID_ARGUMENT( "Invalid polygon mode." );
        }
    }

    auto GetSamplerFilter( rhi::SamplerFilter filter ) -> GLenum {
        switch ( filter ) {
            case rhi::SamplerFilter::eNearest:
                return GL_NEAREST;
            case rhi::SamplerFilter::eLinear:
                return GL_LINEAR;
            default:
                MKT_THROW_INVALID_ARGUMENT( "Invalid sampler filter." );
        }
    }

    auto GetSamplerMinFilter( rhi::SamplerFilter filter, rhi::SamplerMipmapMode mipmapMode ) -> GLenum {
        if ( mipmapMode != rhi::SamplerMipmapMode::eNearest && mipmapMode != rhi::SamplerMipmapMode::eLinear ) {
            MKT_THROW_INVALID_ARGUMENT( "Invalid sampler mipmap mode." );
        }

        switch ( filter ) {
            case rhi::SamplerFilter::eNearest:
                return mipmapMode == rhi::SamplerMipmapMode::eLinear ? GL_NEAREST_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST;
            case rhi::SamplerFilter::eLinear:
                return mipmapMode == rhi::SamplerMipmapMode::eLinear ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_NEAREST;
            default:
                MKT_THROW_INVALID_ARGUMENT( "Invalid sampler filter." );
        }
    }

    auto GetSamplerWrap( rhi::SamplerWrapMode wrap ) -> GLenum {
        switch ( wrap ) {
            case rhi::SamplerWrapMode::eRepeat:
                return GL_REPEAT;
            case rhi::SamplerWrapMode::eMirroredRepeat:
                return GL_MIRRORED_REPEAT;
            case rhi::SamplerWrapMode::eClampToEdge:
                return GL_CLAMP_TO_EDGE;
            case rhi::SamplerWrapMode::eClampToBorder:
                return GL_CLAMP_TO_BORDER;
            default:
                MKT_THROW_INVALID_ARGUMENT( "Invalid sampler wrap mode." );
        }
    }

    auto GetBlendFactor( rhi::BlendFactor factor ) -> GLenum {
        switch ( factor ) {
            case rhi::BlendFactor::eZero:
                return GL_ZERO;
            case rhi::BlendFactor::eOne:
                return GL_ONE;
            case rhi::BlendFactor::eSrcColor:
                return GL_SRC_COLOR;
            case rhi::BlendFactor::eOneMinusSrcColor:
                return GL_ONE_MINUS_SRC_COLOR;
            case rhi::BlendFactor::eDstColor:
                return GL_DST_COLOR;
            case rhi::BlendFactor::eOneMinusDstColor:
                return GL_ONE_MINUS_DST_COLOR;
            case rhi::BlendFactor::eSrcAlpha:
                return GL_SRC_ALPHA;
            case rhi::BlendFactor::eOneMinusSrcAlpha:
                return GL_ONE_MINUS_SRC_ALPHA;
            case rhi::BlendFactor::eDstAlpha:
                return GL_DST_ALPHA;
            case rhi::BlendFactor::eOneMinusDstAlpha:
                return GL_ONE_MINUS_DST_ALPHA;
            case rhi::BlendFactor::eSrcAlphaSaturate:
                return GL_SRC_ALPHA_SATURATE;
            default:
                MKT_THROW_INVALID_ARGUMENT( "Invalid blend factor." );
        }
    }

    auto GetBlendOp( rhi::BlendOperation operation ) -> GLenum {
        switch ( operation ) {
            case rhi::BlendOperation::eAdd:
                return GL_FUNC_ADD;
            case rhi::BlendOperation::eSubtract:
                return GL_FUNC_SUBTRACT;
            case rhi::BlendOperation::eReverseSubtract:
                return GL_FUNC_REVERSE_SUBTRACT;
            case rhi::BlendOperation::eMin:
                return GL_MIN;
            case rhi::BlendOperation::eMax:
                return GL_MAX;
            default:
                MKT_THROW_INVALID_ARGUMENT( "Invalid blend operation." );
        }
    }

    auto GetColorWriteMask( rhi::ColorWriteMask writeMask ) -> std::array<GLboolean, 4> {
        const auto bits{ static_cast<core::u8>( writeMask ) };

        if ( bits > 0x0F ) {
            MKT_THROW_INVALID_ARGUMENT( "Invalid color write mask." );
        }

        return { static_cast<GLboolean>( ( bits & 0x01 ) != 0 ), static_cast<GLboolean>( ( bits & 0x02 ) != 0 ),
            static_cast<GLboolean>( ( bits & 0x04 ) != 0 ), static_cast<GLboolean>( ( bits & 0x08 ) != 0 ) };
    }

    auto SetCapability( GLenum capability, bool enabled ) -> void {
        if ( enabled ) {
            glEnable( capability );
        }
        else {
            glDisable( capability );
        }
    }

    auto Payload( const void *data, core::usize size ) -> eastl::vector<core::ubyte> {
        if ( !data && size ) {
            MKT_THROW_INVALID_ARGUMENT( "Null command payload." );
        }

        eastl::vector<core::ubyte> result( size );

        if ( size ) {
            std::memcpy( result.data(), data, size );
        }

        return result;
    }

    auto ValidateSlice( const Texture &texture, const renderer::rhi::TextureSlice &slice ) -> void {
        if ( slice.mMipLevel >= texture.GetMipLevelCount() || !slice.mWidth || !slice.mHeight || !slice.mDepth ) {
            MKT_THROW_OUT_OF_RANGE( "Invalid texture slice." );
        }

        const auto width{ glm::max( 1u, texture.GetWidth() >> slice.mMipLevel ) };
        const auto height{ glm::max( 1u, texture.GetHeight() >> slice.mMipLevel ) };

        if ( slice.x >= width || slice.mWidth > width - slice.x || slice.y >= height || slice.mHeight > height - slice.y ||
             slice.mArrayLayer >= texture.Layers() || slice.mDepth > texture.Layers() - slice.mArrayLayer || slice.z ) {
            MKT_THROW_OUT_OF_RANGE( "Texture slice exceeds its subresource." );
        }
    }

    auto CreateFullSlice( const renderer::rhi::ITexture &texture ) -> renderer::rhi::TextureSlice {
        renderer::rhi::TextureSlice slice{};
        slice.mWidth = texture.GetWidth();
        slice.mHeight = texture.GetHeight();
        slice.mDepth = dynamic_cast<const Texture &>( texture ).Layers();
        return slice;
    }

    auto ClearAttachment( GLuint framebuffer, GLenum buffer, GLint index, const renderer::rhi::Color &color, const TextureFormat &format ) -> void {
        if ( format.mDepth ) {

            if ( format.mStencil ) {
                glClearNamedFramebufferfi( framebuffer, GL_DEPTH_STENCIL, 0, color.mR, static_cast<GLint>( color.mG ) );
            }
            else {
                glClearNamedFramebufferfv( framebuffer, GL_DEPTH, 0, &color.mR );
            }
        }
        else if ( format.mInteger ) {

            if ( format.mSigned ) {
                const std::array<GLint, 4> values{ static_cast<GLint>( color.mR ), static_cast<GLint>( color.mG ), static_cast<GLint>( color.mB ),
                    static_cast<GLint>( color.mA ) };
                glClearNamedFramebufferiv( framebuffer, buffer, index, values.data() );
            }
            else {
                const std::array<GLuint, 4> values{ static_cast<GLuint>( color.mR ), static_cast<GLuint>( color.mG ),
                    static_cast<GLuint>( color.mB ), static_cast<GLuint>( color.mA ) };
                glClearNamedFramebufferuiv( framebuffer, buffer, index, values.data() );
            }
        }
        else {
            glClearNamedFramebufferfv( framebuffer, buffer, index, &color.mR );
        }
    }

}// namespace mikoto::renderer::opengl