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

#include <Assets/ModelUploadBatch.hh>

namespace mikoto::asset {
    using namespace core;
    using namespace renderer::rhi;

    ModelUploadBatch::ModelUploadBatch( renderer::rhi::IGpuDevice& device )
        : mDevice{ device }, mFence{ device.CreateFence( 0 ) } {
    }

    ModelUploadBatch::~ModelUploadBatch() {
        // Unsubmitted work can be discarded. Submitted work must stop using destinations first.
        try {
            while ( !mPending.empty() ) {
                RetireOldest();
            }
        } catch ( ... ) {
            // Device loss can prevent fence completion. Drain the device before
            // unwinding retained resources; destructors must not mask the import error.
            try {
                mDevice.WaitIdle();
            } catch ( ... ) {
            }
        }
    }

    auto ModelUploadBatch::Prepare( core::usize size ) -> void {
        constexpr core::usize budget{ 64 * 1024 * 1024 };

        if ( mBytes && ( size > budget || mBytes > budget - size ) ) {
            Submit();
        }

        if ( !mRecording.mCommand ) {
            mRecording.mCommand = mDevice.CreateCommandList( renderer::rhi::QueueType::eTransfer );
            mRecording.mCommand->Open( {} );
        }

        mBytes += size;
    }

    auto ModelUploadBatch::Write( renderer::rhi::BufferHandle buffer, const void* data, core::usize size, renderer::rhi::ResourceStates state ) -> void {
        Prepare( size );
        mRecording.mBuffers.push_back( buffer );
        mRecording.mCommand->Write( buffer.GetPtr(), data, size );
        mRecording.mCommand->SetTransition( { buffer.GetPtr(), state } );
    }

    auto ModelUploadBatch::Write( renderer::rhi::TextureHandle texture, const void* data, core::usize size ) -> void {
        Prepare( size );
        mRecording.mTextures.push_back( texture );
        mRecording.mCommand->Write( texture.GetPtr(), data, size );
        mRecording.mCommand->SetTransition( renderer::rhi::TransitionDescription{}
            .AddTexture( texture.GetPtr(), renderer::rhi::ResourceStates::eShaderResource ) );
    }

    auto ModelUploadBatch::Submit() -> void {
        if ( !mBytes ) {
            return;
        }
        // Backpressure bounds staging memory while CPU preparation overlaps prior uploads.
        if ( mPending.size() >= 2 ) {
            RetireOldest();
        }

        mRecording.mCommand->Close();
        mRecording.mCompletion = ++mSubmitted;
        mPending.push_back( eastl::move( mRecording ) );
        auto& batch{ mPending.back() };

        try {
            mDevice.GetQueue( renderer::rhi::QueueType::eTransfer )->ExecuteCommandLists( renderer::rhi::SubmitInfo{}.AddCommandList( batch.mCommand ).AddSignal( mFence, batch.mCompletion ) );
        } catch ( ... ) {
            // No signal will arrive for a rejected submission. Keep only batches
            // that were accepted, otherwise cleanup would wait indefinitely.
            mPending.pop_back();
            --mSubmitted;
            throw;
        }

        mRecording = {};
        mBytes = 0;
    }

    auto ModelUploadBatch::RetireOldest() -> void {
        while ( !mFence->Wait( mPending.front().mCompletion, 1000 ) ) {
        }

        mPending.pop_front();
    }

    auto ModelUploadBatch::Finish() -> void {
        Submit();

        if ( mSubmitted && mDevice.GetQueue( renderer::rhi::QueueType::eTransfer ) != mDevice.GetQueue( renderer::rhi::QueueType::eGraphics ) ) {
            // A host wait alone is not a cross-queue GPU memory dependency. Publish through
            // the render queue before any later draw can reference this completed model.
            auto visible{ mDevice.CreateFence( 0 ) };
            mDevice.GetQueue( renderer::rhi::QueueType::eGraphics )->ExecuteCommandLists( renderer::rhi::SubmitInfo{}.AddWait( mFence, mSubmitted ).AddSignal( visible, 1 ) );

            while ( !visible->Wait( 1, 1000 ) ) {
            }
        }

        while ( !mPending.empty() ) {
            RetireOldest();
        }
    }
}// namespace mikoto::asset
