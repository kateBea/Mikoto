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
#include <Core/String.hh>

#include <Renderer/Rhi/Types.hh>

namespace mikoto::renderer::rhi {

    using namespace mikoto::core;

    auto BufferRange::SetByteOffset( u64 value ) -> BufferRange & {
        mByteOffset = value;
        return *this;
    }

    auto BufferRange::SetByteSize( u64 value ) -> BufferRange & {
        mByteSize = value;
        return *this;
    }

    auto BufferRange::Validate( size_t bufferByteSize ) -> BufferRange & {
        // Check specified offset is not out of bounds
        mByteOffset = eastl::min(mByteOffset, bufferByteSize);

        // If offset is 0 it means whole range
        // otherwise pick a slice
        if (mByteOffset != 0) {
            mByteSize = eastl::min(mByteSize, bufferByteSize - mByteOffset);
        } else {
            mByteSize = bufferByteSize;
        }

        return *this;
    }

    Color::operator float4() const {
        return float4{ mR, mG, mB, mA };
    }
}