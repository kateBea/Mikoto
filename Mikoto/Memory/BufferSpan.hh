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

#ifndef MIKOTO_BUFFER_SPAN_HH
#define MIKOTO_BUFFER_SPAN_HH

#include <cstdlib>

#include <EASTL/vector.h>
#include <EASTL/unique_ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Exception.hh>
#include <Core/ResourcePool.hh>

#include <Memory/Allocator.hh>

namespace mikoto::memory {

    using namespace mikoto::core;

    class BufferSpan : public ReferenceCounted {
    public:

        explicit BufferSpan( size_t byteSize );
        explicit BufferSpan( const void* data, size_t byteSize );

        template<typename T >
        auto Push(const T& data) -> void {
            Push( MKT_ADDRESSOF( data ), MKT_SIZEOF( data ) );
        }

        auto Push(const void* data, size_t dataSize) -> void;

        MKT_NODISCARD auto GetSize() const -> size_t;
        MKT_NODISCARD auto GetData() const -> const void*;

        ~BufferSpan() override = default;

    private:
        size_t mConsumedSize{ 0 };
        eastl::vector<byte_t> mBuffer{};
    };

    using BufferSpanHandle = Ref<BufferSpan>;
}// namespace mikoto

#endif//MIKOTO_BUFFER_SPAN_HH
