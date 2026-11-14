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

#include <EASTL/algorithm.h>

#include <Core/Profiler.hh>

#include <Logging/Assert.hh>

#include <Memory/BufferSpan.hh>

namespace mikoto::memory {

    using namespace mikoto::core;

    BufferSpan::BufferSpan( usize byteSize )
        : mConsumedSize{ 0 }, mBuffer( byteSize, ubyte{} ) {
    }

    BufferSpan::BufferSpan( const void *data, usize byteSize )
        : mConsumedSize{ 0 }, mBuffer( byteSize, ubyte{} ) {
        MKT_BEGIN_PROFILER();

        if ( data ) {
            Push( data, byteSize );
        }
    }

    auto BufferSpan::Push( const void *data, usize dataSize ) -> void {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( mConsumedSize != mBuffer.size(), "Buffer is full." );
        std::memcpy( mBuffer.begin() + mConsumedSize, core::as<ubyte *>( data ), dataSize );

        mConsumedSize += dataSize;
    }

    auto BufferSpan::GetSize() const -> usize {
        MKT_BEGIN_PROFILER();
        return mConsumedSize;
    }

    auto BufferSpan::GetData() const -> const void * {
        MKT_BEGIN_PROFILER();
        return mBuffer.data();
    }
}// namespace mikoto