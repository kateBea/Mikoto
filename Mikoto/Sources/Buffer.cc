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

#include <Renderer/Rhi/Buffer.hh>

namespace mikoto::renderer::rhi {

    auto IBuffer::GetUsage() const -> BufferUsageFlags {
        return mUsage;
    }

    auto IBuffer::GetDataType() const -> BufferDataType {
        return mDataType;
    }

    auto IBuffer::GetData() const -> memory::BufferSpanHandle {
        return mUploadContents;
    }

    auto IBuffer::GetSizeBytes() const -> core::usize {
        return mElementCount == 0 ? mElementSize : mElementCount * mElementSize;
    }

    auto IBuffer::GetFormat() const -> Format {
        return mFormat;
    }

    auto IBuffer::GetCount() const -> core::usize {
        return mElementCount == 0 ? InferElementCount( mFormat, mElementSize ) : mElementCount;
    }
}
