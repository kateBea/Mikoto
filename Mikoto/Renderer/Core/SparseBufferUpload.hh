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

#ifndef MIKOTO_RENDERER_CORE_SPARSE_BUFFER_UPLOAD_HH
#define MIKOTO_RENDERER_CORE_SPARSE_BUFFER_UPLOAD_HH

#include <cstring>
#include <algorithm>
#include <type_traits>

#include <EASTL/span.h>
#include <EASTL/sort.h>
#include <EASTL/vector.h>
#include <EASTL/algorithm.h>

#include <Core/Types.hh>

namespace mikoto::renderer {

    struct SceneUploadStatistics {
        core::usize mBytes{};
        core::usize mChangedRecords{};
        core::u32 mCopyCalls{};
        core::u32 mScatterDispatches{};
    };

    // One persistent destination per frame graph. Its resource history orders
    // updates after previous readers, including readers from earlier frames.
    template<typename T>
    class SparseBufferUpload {
        static_assert( std::is_trivially_copyable_v<T> && sizeof( T ) % 4 == 0 );

    public:
        static constexpr core::u32 kMaxScatterRecords{ 65536 };
        static constexpr core::usize kScratchBytes{ core::usize{ kMaxScatterRecords } * ( sizeof( T ) + 4 ) };

        // The module owns the values. Uploads retain only indices until Stage().
        auto MarkDirty( core::u32 slot ) -> void {
            mDirty.push_back( slot );
        }

        auto Clear() -> void {
            mDirty.clear();
            mPayload.clear();
            mScatterCount = 0;
            mStatistics = {};
        }

        template<typename Context, typename BufferHandle>
        auto Stage( Context& ctx, BufferHandle target, BufferHandle scratch, eastl::span<const T> values ) -> void {
            mStatistics = {};
            mScatterCount = 0;

            if ( mDirty.empty() ) {
                return;
            }
            
            eastl::sort( mDirty.begin(), mDirty.end() );
            mDirty.erase( eastl::unique( mDirty.begin(), mDirty.end() ), mDirty.end() );
            mStatistics.mChangedRecords = mDirty.size();

            core::usize runs{ 1 };
            for ( core::usize i{ 1 }; i < mDirty.size(); ++i ) {
                runs += mDirty[i] != mDirty[i - 1] + 1;
            }

            // Dense changes favor sequential transfer. A few contiguous ranges
            // need no compute dispatch; scattered edits use a compact payload.
            if ( mDirty.size() * 2 >= values.size() || mDirty.size() > kMaxScatterRecords ) {
                ctx.CopyBuffer( target, 0, values.data(), values.size() * sizeof( T ) );
                mStatistics.mBytes = values.size() * sizeof( T );
                mStatistics.mCopyCalls = 1;
            } else if ( runs <= 8 || mDirty.size() < 64 ) {
                for ( core::usize i{}; i < mDirty.size(); ) {
                    const auto first{ mDirty[i] };
                    core::usize count{ 1 };
                    while ( i + count < mDirty.size() && mDirty[i + count] == first + count ) {
                        ++count;
                    }

                    ctx.CopyBuffer( target, core::usize{ first } * sizeof( T ), values.data() + first, count * sizeof( T ) );
                    mStatistics.mBytes += count * sizeof( T );
                    ++mStatistics.mCopyCalls;
                    i += count;
                }
            } else {
                mScatterCount = static_cast<core::u32>( mDirty.size() );
                constexpr core::usize words{ sizeof( T ) / 4 };

                mPayload.resize( mDirty.size() * ( words + 1 ) );
                for ( core::usize i{}; i < mDirty.size(); ++i ) {
                    mPayload[i] = mDirty[i];
                    std::memcpy( mPayload.data() + mDirty.size() + i * words, &values[mDirty[i]], sizeof( T ) );
                }

                ctx.CopyBuffer( scratch, 0, mPayload.data(), mPayload.size() * 4 );
                
                mStatistics.mBytes = mPayload.size() * 4;
                mStatistics.mCopyCalls = 1;
                mStatistics.mScatterDispatches = 1;
            }

            mDirty.clear();
        }

        template<typename Context, typename BufferHandle, typename PipelineHandle>
        auto Scatter( Context& ctx, BufferHandle target, BufferHandle scratch, PipelineHandle pipeline ) -> void {
            if ( mScatterCount == 0 ) {
                return;
            }

            struct Parameters {
                core::u64 mSource;
                core::u64 mDestination;
                core::u32 mCount;
                core::u32 mWords;
            } params{
                .mSource = ctx.GetDeviceBufferAddress( scratch ),
                .mDestination = ctx.GetDeviceBufferAddress( target ),
                .mCount = mScatterCount,
                .mWords = sizeof( T ) / 4
            };
            
            ctx.PushConstants( params );
            ctx.BindPipeline( pipeline );
            ctx.Dispatch( ( mScatterCount * params.mWords + 63 ) / 64, 1, 1 );
        }

        auto GetStatistics() const -> const SceneUploadStatistics& {
            return mStatistics;
        }

    private:
        core::u32 mScatterCount{};
        eastl::vector<core::u32> mDirty{};
        eastl::vector<core::u32> mPayload{};
        SceneUploadStatistics mStatistics{};
    };
}// namespace mikoto::renderer
#endif
