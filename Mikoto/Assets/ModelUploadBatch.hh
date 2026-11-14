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

#ifndef MIKOTO_ASSETS_MODEL_UPLOAD_BATCH_HH
#define MIKOTO_ASSETS_MODEL_UPLOAD_BATCH_HH

#include <EASTL/deque.h>

#include <Renderer/Rhi/GpuDevice.hh>

namespace mikoto::asset {

    /** @brief Records model uploads on one owner thread with bounded in-flight staging.
     * Submitted command lists and destination handles outlive their completion fences,
     * including when import unwinds. Finish establishes visibility on the graphics queue
     * before the factory publishes its model. One oversized resource is allowed per batch.
     */
    class ModelUploadBatch final {
    public:
        explicit ModelUploadBatch( renderer::rhi::IGpuDevice& device );
        ~ModelUploadBatch();
        ModelUploadBatch( const ModelUploadBatch& ) = delete;
        auto operator=( const ModelUploadBatch& ) -> ModelUploadBatch& = delete;
        auto Write( renderer::rhi::BufferHandle buffer, const void* data, core::usize size,
                    renderer::rhi::ResourceStates state ) -> void;
        auto Write( renderer::rhi::TextureHandle texture, const void* data, core::usize size ) -> void;
        auto Finish() -> void;

    private:
        struct Batch {
            renderer::rhi::CommandListHandle mCommand{};
            eastl::vector<renderer::rhi::BufferHandle> mBuffers{};
            eastl::vector<renderer::rhi::TextureHandle> mTextures{};
            core::u64 mCompletion{};
        };
        auto Prepare( core::usize size ) -> void;
        auto Submit() -> void;
        auto RetireOldest() -> void;
        renderer::rhi::IGpuDevice& mDevice;
        renderer::rhi::FenceHandle mFence{};
        Batch mRecording{};
        eastl::deque<Batch> mPending{};
        core::usize mBytes{};
        core::u64 mSubmitted{};
    };
}// namespace mikoto::asset

#endif // MIKOTO_ASSETS_MODEL_UPLOAD_BATCH_HH
