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

#include <Memory/BufferSpan.hh>

namespace mikoto::memory {

    using namespace mikoto::core;

    BufferSpan::BufferSpan( core::usize byteSize )
        : mConsumedSize{ 0 }, mBuffer( byteSize, ubyte{} ) {
    }

    BufferSpan::BufferSpan( const void *data, core::usize byteSize )
        : mConsumedSize{ 0 }, mBuffer( byteSize, ubyte{} )
    {
        if (data) {
            Push( data, byteSize );
        }
    }

    auto BufferSpan::Push( const void* data, core::usize dataSize ) -> void {
        MKT_ASSERT( mConsumedSize != mBuffer.size(), "Buffer is full." );
        eastl::copy_n( as<ubyte*>( data ), dataSize, mBuffer.begin() + mConsumedSize );

        mConsumedSize += dataSize;
    }

    auto BufferSpan::GetSize() const -> core::usize {
        return mConsumedSize;
    }

    auto BufferSpan::GetData() const -> const void * {
        return mBuffer.data();
    }
}// namespace mikoto