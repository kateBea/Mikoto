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

#ifndef MIKOTO_RENDERER_RHI_OPENGL_OPENGL_HELPERS_HH
#define MIKOTO_RENDERER_RHI_OPENGL_OPENGL_HELPERS_HH

#include <array>

#include <EASTL/vector.h>

#include <glad/gl.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Exception.hh>
#include <Core/ReferenceCounted.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/CommandList.hh>

namespace mikoto::renderer::opengl {

    // TODO: https://registry.khronos.org/OpenGL-Refpages/gl4/html/glDebugMessageCallback.xhtml
    // or do a GLCheck macro one cause glDebugMessageCallback is 4.3 onewards
    // GLCheck(glCreateuffer()) or some shit

    class Texture;

    struct TextureFormat {
        GLenum mInternal{};
        GLenum mExternal{};
        GLenum mType{};
        core::u32 mBytes{};
        bool mDepth{};
        bool mStencil{};
        bool mInteger{};
        bool mSigned{};
    };

    /** Throws an explicit error for features this OpenGL backend cannot represent. */
    auto Unsupported( const char* feature ) -> void;

    /** Checks the context's GL error flag after the named operation. */
    auto CheckError( const char* operation ) -> void;

    MKT_NODISCARD auto GetFormat( rhi::Format format ) -> TextureFormat;
    MKT_NODISCARD auto GetCompareOp( rhi::CompareOp operation ) -> GLenum;
    MKT_NODISCARD auto GetTopology( rhi::PrimitiveTopology topology ) -> GLenum;
    MKT_NODISCARD auto GetShaderModuleStage( rhi::ShaderType stage ) -> GLenum;
    MKT_NODISCARD auto GetCullMode( rhi::CullMode mode ) -> GLenum;
    MKT_NODISCARD auto GetWindingOrder( rhi::WindingOrder order ) -> GLenum;
    MKT_NODISCARD auto GetPolygonMode( rhi::PolygonMode mode ) -> GLenum;
    MKT_NODISCARD auto GetSamplerFilter( rhi::SamplerFilter filter ) -> GLenum;
    MKT_NODISCARD auto GetSamplerMinFilter( rhi::SamplerFilter filter, rhi::SamplerMipmapMode mipmapMode ) -> GLenum;
    MKT_NODISCARD auto GetSamplerWrap( rhi::SamplerWrapMode wrap ) -> GLenum;

    /** Converts an RHI source/destination blend factor to its GL enum. */
    MKT_NODISCARD auto GetBlendFactor( rhi::BlendFactor factor ) -> GLenum;

    /** Converts an RHI color or alpha blend operation to its GL enum. */
    MKT_NODISCARD auto GetBlendOp( rhi::BlendOperation operation ) -> GLenum;

    /** Converts writable channels to the four GLboolean arguments of glColorMask. */
    MKT_NODISCARD auto GetColorWriteMask( rhi::ColorWriteMask writeMask ) -> std::array<GLboolean, 4>;

    MKT_NODISCARD auto CreateFullSlice( const rhi::ITexture& texture ) -> rhi::TextureSlice;
    MKT_NODISCARD auto Payload( const void* data, core::usize size ) -> eastl::vector<core::ubyte>;

    auto SetCapability( GLenum capability, bool enabled ) -> void;
    auto ValidateSlice( const Texture& texture, const rhi::TextureSlice& slice ) -> void;

    auto ClearAttachment( GLuint framebuffer, GLenum buffer, GLint index, const rhi::Color& color, const TextureFormat& format ) -> void;

    template<typename T, typename U>
    auto Retain( U *resource ) -> core::Ref<T> {
        auto *result{ dynamic_cast<T *>( resource ) };

        if ( !result ) {
            MKT_THROW_INVALID_ARGUMENT( "Resource is null or belongs to another graphics backend." );
        }

        return core::Ref<T>{ result };
    }

}// namespace mikoto::renderer::opengl

#endif// MIKOTO_RENDERER_RHI_OPENGL_OPENGL_HELPERS_HH
