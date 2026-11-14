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

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Renderer/Rhi/Types.hh>

namespace mikoto::renderer::rhi {

    using namespace mikoto::core;

    auto BufferRange::SetByteOffset( core::u64 value ) -> BufferRange& {
        MKT_BEGIN_PROFILER();
        mByteOffset = value;
        return *this;
    }

    auto BufferRange::SetByteSize( core::u64 value ) -> BufferRange& {
        MKT_BEGIN_PROFILER();
        mByteSize = value;
        return *this;
    }

    BufferRange::BufferRange( core::u64 byteOffset, core::u64 byteSize )
        : mByteOffset{ byteOffset }, mByteSize{ byteSize } {
    }

    auto BufferRange::IsEntireBuffer( core::usize bufferByteSize ) const -> bool {
        MKT_BEGIN_PROFILER();
        return mByteOffset == 0 && ( mByteSize == ~0ull || mByteSize == bufferByteSize );
    }

    auto BufferRange::operator==( const BufferRange& other ) const -> bool {
        MKT_BEGIN_PROFILER();
        return mByteOffset == other.mByteOffset && mByteSize == other.mByteSize;
    }

    auto BufferRange::Validate( core::usize bufferByteSize ) -> BufferRange& {
        MKT_BEGIN_PROFILER();
        // Check specified offset is not out of bounds
        mByteOffset = eastl::min( mByteOffset, bufferByteSize );

        // If offset is 0 it means whole range
        // otherwise pick a slice
        if ( mByteOffset != 0 ) {
            mByteSize = eastl::min( mByteSize, bufferByteSize - mByteOffset );
        }
        else {
            mByteSize = bufferByteSize;
        }

        return *this;
    }

    Color::operator core::float4() const {
        MKT_BEGIN_PROFILER();
        return core::float4{ mR, mG, mB, mA };
    }


    TextureSubresourceSet::TextureSubresourceSet( MipLevel baseMipLevel, MipLevel numMipLevels, ArraySlice baseArraySlice, ArraySlice numArraySlices )
        : mBaseMipLevel( baseMipLevel ), mNumMipLevels( numMipLevels ), mBaseArraySlice( baseArraySlice ), mNumArraySlices( numArraySlices ) {
    }


    Color::Color( core::f32 c )
        : mR{ c }, mG{ c }, mB{ c }, mA{ c } {
    }


    Color::Color( const core::float4& c )
        : mR{ c.r }, mG{ c.g }, mB{ c.b }, mA{ c.a } {
    }


    Color::Color( core::f32 r, core::f32 g, core::f32 b, core::f32 a )
        : mR{ r }, mG{ g }, mB{ b }, mA{ a } {
    }


    Viewport::Viewport( core::f32 width, core::f32 height )
        : mMinX{ 0.f }, mMaxX{ width }, mMinY{ 0.f }, mMaxY{ height }, mMinZ{ 0.f }, mMaxZ{ 1.f } {
    }


    Viewport::Viewport( core::f32 minX, core::f32 maxX, core::f32 minY, core::f32 maxY, core::f32 minZ, core::f32 maxZ )
        : mMinX{ minX }, mMaxX{ maxX }, mMinY{ minY }, mMaxY{ maxY }, mMinZ{ minZ }, mMaxZ{ maxZ } {
    }


    auto Viewport::GetWidth() const -> float {
        MKT_BEGIN_PROFILER();
        return mMaxX - mMinX;
    }


    auto Viewport::GetHeight() const -> float {
        MKT_BEGIN_PROFILER();
        return mMaxY - mMinY;
    }


    Rect::Rect( core::i32 width, core::i32 height )
        : mMinX{ 0 }, mMaxX{ width }, mMinY{ 0 }, mMaxY{ height } {
    }


    Rect::Rect( core::i32 minX, core::i32 maxX, core::i32 minY, core::i32 maxY )
        : mMinX{ minX }, mMaxX{ maxX }, mMinY{ minY }, mMaxY{ maxY } {
    }


    Rect::Rect( const Viewport& viewport )
        : mMinX{ core::as<core::i32>( glm::floor( viewport.mMinX ) ) },
          mMaxX{ core::as<core::i32>( glm::ceil( viewport.mMaxX ) ) },
          mMinY{ core::as<core::i32>( glm::floor( viewport.mMinY ) ) },
          mMaxY{ core::as<core::i32>( glm::ceil( viewport.mMaxY ) ) } {
    }


    auto Rect::ComputeWidth() const -> core::i32 {
        MKT_BEGIN_PROFILER();
        return mMaxX - mMinX;
    }


    auto Rect::ComputeHeight() const -> core::i32 {
        MKT_BEGIN_PROFILER();
        return mMaxY - mMinY;
    }


    auto Color::operator==( const Color& other ) const -> bool {
        MKT_BEGIN_PROFILER();
        return mR == other.mR && mG == other.mG && mB == other.mB && mA == other.mA;
    }

    auto Color::operator!=( const Color& other ) const -> bool {
        MKT_BEGIN_PROFILER();
        return !( *this == other );
    }


    auto Viewport::operator==( const Viewport& other ) const -> bool {
        MKT_BEGIN_PROFILER();
        return mMinX == other.mMinX && mMinY == other.mMinY && mMinZ == other.mMinZ && mMaxX == other.mMaxX && mMaxY == other.mMaxY && mMaxZ == other.mMaxZ;
    }

    auto Viewport::operator!=( const Viewport& other ) const -> bool {
        MKT_BEGIN_PROFILER();
        return !( *this == other );
    }


    auto Rect::operator==( const Rect& other ) const -> bool {
        MKT_BEGIN_PROFILER();
        return mMinX == other.mMinX && mMinY == other.mMinY && mMaxX == other.mMaxX && mMaxY == other.mMaxY;
    }

    auto Rect::operator!=( const Rect& other ) const -> bool {
        MKT_BEGIN_PROFILER();
        return !( *this == other );
    }
}
