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


#include <bit>
#include <chrono>
#include <ranges>
#include <cstring>

#include <EASTL/sort.h>
#include <EASTL/array.h>
#include <EASTL/algorithm.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

#include <Math/Math.hh>
#include <Math/Random.hh>

#include <Memory/Allocator.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Utility.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Rhi/GpuAllocator.hh>
#include <Renderer/Core/RenderSystem.hh>


#if defined( MIKOTO_PLATFORM_WINDOWS )

#include <Platform/Windows/Windows.hh>

#include <Renderer/Rhi/D3D12/D3D12Buffer.hh>
#include <Renderer/Rhi/D3D12/D3D12Device.hh>
#include <Renderer/Rhi/D3D12/D3D12Shader.hh>
#include <Renderer/Rhi/D3D12/D3D12Context.hh>
#include <Renderer/Rhi/D3D12/D3D12Texture.hh>
#include <Renderer/Rhi/D3D12/D3D12Pipeline.hh>
#include <Renderer/Rhi/D3D12/D3D12QueryPool.hh>
#include <Renderer/Rhi/D3D12/D3D12VideoDecoder.hh>
#include <Renderer/Rhi/D3D12/Direct3D12Helpers.hh>

// D3D12 extension library
#include <d3dcompiler.h>
#include <directx/d3d12.h>
#include <directx/d3dx12.h>
#include <directx/d3dx12_resource_helpers.h>
#include <DirectXMath.h>
#include <dxgi1_6.h>
#include <dxva.h>

#include <Renderer/Rhi/D3D12/D3D12AccelerationStructure.hh>

#if defined( MIKOTO_ENABLE_TRACY )
#include <TracyD3D12Capture.hpp>
#endif

namespace mikoto::renderer::d3d12 {

    using namespace mikoto::core;
    using namespace mikoto::memory;
    using namespace mikoto::renderer::rhi;

#if defined( MIKOTO_ENABLE_TRACY )
    struct CommandList::GpuProfileZone {
        eastl::optional<tracy::D3D12ZoneScope> mScope{};
    };
#endif

    static auto SupportsH264Decode( ID3D12VideoDevice& device ) -> bool {
        // Query advertised profiles and output formats; resolution/tier are checked per stream.
        // https://learn.microsoft.com/en-us/windows/win32/medfound/direct3d-12-video-overview
        D3D12_FEATURE_DATA_VIDEO_DECODE_PROFILE_COUNT count{};

        if ( FAILED( device.CheckFeatureSupport( D3D12_FEATURE_VIDEO_DECODE_PROFILE_COUNT, &count, sizeof( count ) ) ) || !count.ProfileCount ) {
            return false;
        }

        eastl::vector<GUID> profiles( count.ProfileCount );
        D3D12_FEATURE_DATA_VIDEO_DECODE_PROFILES query{};
        query.ProfileCount = count.ProfileCount;
        query.pProfiles = profiles.data();

        if ( FAILED( device.CheckFeatureSupport( D3D12_FEATURE_VIDEO_DECODE_PROFILES, &query, sizeof( query ) ) ) ||
            eastl::none_of( profiles.begin(), profiles.end(), []( const GUID& profile ) {
                return IsEqualGUID( profile, D3D12_VIDEO_DECODE_PROFILE_H264 );
            } ) ) {
            return false;
        }

        D3D12_FEATURE_DATA_VIDEO_DECODE_FORMAT_COUNT formatCount{};
        formatCount.Configuration.DecodeProfile = D3D12_VIDEO_DECODE_PROFILE_H264;
        formatCount.Configuration.InterlaceType = D3D12_VIDEO_FRAME_CODED_INTERLACE_TYPE_NONE;

        if ( FAILED( device.CheckFeatureSupport( D3D12_FEATURE_VIDEO_DECODE_FORMAT_COUNT, &formatCount, sizeof( formatCount ) ) ) || !formatCount.FormatCount ) {
            return false;
        }

        eastl::vector<DXGI_FORMAT> formats( formatCount.FormatCount );
        D3D12_FEATURE_DATA_VIDEO_DECODE_FORMATS formatQuery{};
        formatQuery.Configuration = formatCount.Configuration;
        formatQuery.FormatCount = formatCount.FormatCount;
        formatQuery.pOutputFormats = formats.data();
        return SUCCEEDED( device.CheckFeatureSupport( D3D12_FEATURE_VIDEO_DECODE_FORMATS, &formatQuery, sizeof( formatQuery ) ) ) &&
            eastl::find( formats.begin(), formats.end(), DXGI_FORMAT_NV12 ) != formats.end();
    }

    static auto MakeVideoPictureParameters( const VideoDecodeDescription& description, u32 feedback ) -> DXVA_PicParams_H264 {
        // DXVA H.264 picture arguments, adapted to Mikoto's existing reference-picture bookkeeping.
        // https://www.microsoft.com/en-us/download/details.aspx?id=11323
        // https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/wiGraphicsDevice_DX12.cpp (VideoDecode)
        const auto& picture{ *description.mPicture };
        const auto& sps{ picture.GetSps() };
        const auto& pps{ picture.GetPps() };
        const auto& slice{ picture.GetSlices().front() };
        DXVA_PicParams_H264 parameters{};
        parameters.wFrameWidthInMbsMinus1 = static_cast<USHORT>( sps.pic_width_in_mbs_minus1 );
        parameters.wFrameHeightInMbsMinus1 = static_cast<USHORT>( sps.pic_height_in_map_units_minus1 );
        parameters.CurrPic.Index7Bits = static_cast<UCHAR>( description.mReferences.size() );
        parameters.CurrFieldOrderCnt[0] = parameters.CurrFieldOrderCnt[1] = description.mReconstruction.mPictureOrderCount;
        parameters.num_ref_frames = static_cast<UCHAR>( sps.num_ref_frames );
        parameters.chroma_format_idc = 1;
        parameters.RefPicFlag = picture.IsReference();
        parameters.IntraPicFlag = eastl::all_of( picture.GetSlices().begin(), picture.GetSlices().end(), []( const auto& entry ) {
            return h264::is_slice_type( entry.slice_type, h264::SH_SLICE_TYPE_I );
        } );
        parameters.MbsConsecutiveFlag = 1;
        parameters.frame_mbs_only_flag = 1;
        parameters.MinLumaBipredSize8x8Flag = sps.level_idc >= 31;
        parameters.Reserved16Bits = 3;
        parameters.StatusReportFeedbackNumber = feedback;
        parameters.ContinuationFlag = 1;

        for ( auto& reference: parameters.RefFrameList ) {
            reference.bPicEntry = 255;
        }

        for ( usize index{}; index < description.mReferences.size(); ++index ) {
            const auto& reference{ description.mReferences[index] };
            parameters.RefFrameList[index].Index7Bits = static_cast<UCHAR>( index );
            parameters.RefFrameList[index].AssociatedFlag = reference.mLongTerm;
            parameters.FrameNumList[index] = static_cast<USHORT>( reference.mFrameNumber );
            parameters.FieldOrderCntList[index][0] = parameters.FieldOrderCntList[index][1] = reference.mPictureOrderCount;
            parameters.UsedForReferenceFlags |= 3u << ( index * 2 );
        }

        parameters.frame_num = static_cast<USHORT>( slice.frame_num );
        parameters.pic_init_qp_minus26 = static_cast<CHAR>( pps.pic_init_qp_minus26 );
        parameters.pic_init_qs_minus26 = static_cast<CHAR>( pps.pic_init_qs_minus26 );
        parameters.chroma_qp_index_offset = static_cast<CHAR>( pps.chroma_qp_index_offset );
        parameters.second_chroma_qp_index_offset = static_cast<CHAR>( pps.second_chroma_qp_index_offset );
        parameters.weighted_pred_flag = pps.weighted_pred_flag;
        parameters.weighted_bipred_idc = pps.weighted_bipred_idc;
        parameters.transform_8x8_mode_flag = pps.transform_8x8_mode_flag;
        parameters.constrained_intra_pred_flag = pps.constrained_intra_pred_flag;
        parameters.log2_max_frame_num_minus4 = static_cast<UCHAR>( sps.log2_max_frame_num_minus4 );
        parameters.pic_order_cnt_type = static_cast<UCHAR>( sps.pic_order_cnt_type );
        parameters.log2_max_pic_order_cnt_lsb_minus4 = static_cast<UCHAR>( sps.log2_max_pic_order_cnt_lsb_minus4 );
        parameters.delta_pic_order_always_zero_flag = sps.delta_pic_order_always_zero_flag;
        parameters.direct_8x8_inference_flag = sps.direct_8x8_inference_flag;
        parameters.entropy_coding_mode_flag = pps.entropy_coding_mode_flag;
        parameters.pic_order_present_flag = pps.pic_order_present_flag;
        parameters.deblocking_filter_control_present_flag = pps.deblocking_filter_control_present_flag;
        parameters.redundant_pic_cnt_present_flag = pps.redundant_pic_cnt_present_flag;
        // Picture arguments carry PPS defaults; the short-slice decoder reads per-slice overrides.
        parameters.num_ref_idx_l0_active_minus1 = static_cast<UCHAR>( pps.num_ref_idx_l0_active_minus1 );
        parameters.num_ref_idx_l1_active_minus1 = static_cast<UCHAR>( pps.num_ref_idx_l1_active_minus1 );
        return parameters;
    }

    Fence::operator HANDLE() const {
        MKT_BEGIN_PROFILER();
        return mFenceEvent;
    }


    auto Fence::GetCompletionValue() const -> core::u64 {
        const auto value{ mFence->GetCompletedValue() };

        if ( value == UINT64_MAX ) {
            MKT_THROW_RUNTIME_ERROR( "D3D12 device removed while querying fence completion." );
        }

        return value;
    }

    auto Fence::SetDebugName( eastl::string_view name ) -> void {
        MKT_BEGIN_PROFILER();
        mFence->SetName( string::ToWide( name ).c_str() );
    }

    auto Fence::Signal( core::u64 fenceValue ) -> bool {
        MKT_BEGIN_PROFILER();
        return SUCCEEDED( mFence->Signal( fenceValue ) );
    }

    auto Fence::Wait( core::u64 fenceValue, core::u64 timeoutMs ) -> bool {
        MKT_BEGIN_PROFILER();

        if ( IsCompleted( fenceValue ) ) {
            return true;
        }

        const auto started{ GetTickCount64() };
        const DWORD timeout{ timeoutMs == UINT64_MAX ? INFINITE : static_cast<DWORD>( glm::min( timeoutMs, static_cast<core::u64>( INFINITE - 1 ) ) ) };
        std::unique_lock<std::timed_mutex> lock{ mWaitMutex, std::defer_lock };

        if ( timeout == INFINITE ) {
            lock.lock();
        }
        else if ( !lock.try_lock_for( std::chrono::milliseconds{ timeout } ) ) {
            return IsCompleted( fenceValue );
        }

        if ( IsCompleted( fenceValue ) ) {
            return true;
        }

        if ( !ResetEvent( mFenceEvent ) || FAILED( mFence->SetEventOnCompletion( fenceValue, mFenceEvent ) ) ) {
            return false;
        }
        while ( !IsCompleted( fenceValue ) ) {
            const auto elapsed{ GetTickCount64() - started };

            if ( timeout != INFINITE && elapsed >= timeout ) {
                return false;
            }

            const DWORD remaining{ timeout == INFINITE ? INFINITE : timeout - static_cast<DWORD>( elapsed ) };
            const auto result{ WaitForSingleObject( mFenceEvent, remaining ) };

            if ( result != WAIT_OBJECT_0 ) {
                return result == WAIT_TIMEOUT && IsCompleted( fenceValue );
            }
            // A previous timed-out registration can wake this shared event.
            // Only the requested fence value establishes GPU completion.
        }

        return true;
    }

    auto Fence::IsCompleted( core::u64 fenceValue ) const -> bool {
        return GetCompletionValue() >= fenceValue;
    }

    auto Fence::GetNativeHandle( renderer::rhi::ObjectType type ) -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        switch ( type ) {
            case renderer::rhi::ObjectType::D3D12_Fence:
                return renderer::rhi::Object( mFence.Get() );
            default:;
        }

        return renderer::rhi::Object( nullptr );
    }

    auto Fence::GetNativeHandle( renderer::rhi::ObjectType type ) const -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        switch ( type ) {
            case renderer::rhi::ObjectType::D3D12_Fence:
                return renderer::rhi::Object( mFence.Get() );
            default:;
        }

        return renderer::rhi::Object( nullptr );
    }


    Fence::operator ID3D12Fence*() const {
        MKT_BEGIN_PROFILER();
        return mFence.Get();
    }

    Fence::~Fence() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto Fence::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        Device* device{ core::checked_cast<Device*>( mDevice ) };
        ThrowIfFailed( device->GetDevice()->CreateFence(
            mFenceInitialValue, D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS( &mFence ) ) );

        mFenceEvent = CreateEventW( nullptr, FALSE, FALSE, nullptr );

        if ( !mFenceEvent ) {
            ThrowIfFailed( HRESULT_FROM_WIN32( GetLastError() ) );
        }

        mIsAllocated = true;
    }

    auto Fence::Destroy() -> void {
        if ( mFenceEvent ) {
            CloseHandle( mFenceEvent );
            mFenceEvent = nullptr;
        }

        mIsAllocated = false;
    }

    StaticDescriptorHeap::StaticDescriptorHeap( renderer::rhi::IGpuDevice* device, eastl::string_view debugName )
        : mDebugName{ debugName } {
        MKT_BEGIN_PROFILER();
        Device* pDev{ core::checked_cast<Device*>( device ) };
        mDevice = pDev->GetDevice();
    }

    auto StaticDescriptorHeap::CopyToShaderVisibleHeap( DescriptorIndex index, core::u32 count ) -> void {
        MKT_BEGIN_PROFILER();
        // https://stackoverflow.com/questions/45303008/dx12-descriptor-heaps-management
        mDevice->CopyDescriptorsSimple( count,
            GetCpuHandleShaderVisible( index ),
            GetCpuHandle( index ), mHeapType );
    }

    auto StaticDescriptorHeap::AllocateDescriptor() -> DescriptorIndex {
        MKT_BEGIN_PROFILER();
        return AllocateDescriptors( 1 );
    }

    auto StaticDescriptorHeap::AllocateDescriptors( core::u32 count ) -> DescriptorIndex {
        MKT_BEGIN_PROFILER();

        if ( count == 0 ) {
            return d3d12::kInvalidDescriptorIndex;
        }

        std::lock_guard lockGuard{ mMutex };

        core::u32 freeCount{ 0 };
        bool found{ false };
        DescriptorIndex foundIndex{ 0 };

        // Find a contiguous range of 'count' indices for which m_AllocatedDescriptors[index] is false
        for ( DescriptorIndex index{ mSearchStart }; index < mNumDescriptors; index++ ) {
            for ( const auto& page: mCpuPages ) {
                if ( page.first == index ) {
                    freeCount = 0;
                    break;
                }
            }

            if ( mAllocatedDescriptors[index] ) {
                freeCount = 0;
            }
            else {
                freeCount += 1;
            }

            if ( freeCount >= count ) {
                foundIndex = index - count + 1;
                found = true;
                break;
            }
        }

        if ( !found ) {
            foundIndex = mNumDescriptors;

            if ( FAILED( Grow( mNumDescriptors + count ) ) ) {
                MKT_CORE_LOGGER_ERROR( "Failed to grow a descriptor heap!" );
                return kInvalidDescriptorIndex;
            }
        }

        for ( DescriptorIndex index{ foundIndex }; index < foundIndex + count; index++ ) {
            mAllocatedDescriptors[index] = true;
        }

        mNumAllocatedDescriptors += count;
        mSearchStart = foundIndex + count;

        return foundIndex;
    }

    auto StaticDescriptorHeap::ReleaseDescriptor( DescriptorIndex index ) -> void {
        MKT_BEGIN_PROFILER();
        ReleaseDescriptors( index, 1 );
    }

    auto StaticDescriptorHeap::ReleaseDescriptors( DescriptorIndex baseIndex, core::u32 count ) -> void {
        MKT_BEGIN_PROFILER();

        if ( count == 0 || baseIndex == kInvalidDescriptorIndex ) {
            return;
        }

        std::lock_guard lockGuard{ mMutex };

        for ( DescriptorIndex index{ baseIndex }; index < baseIndex + count; index++ ) {
#if !defined( NDEBUG )
            if ( !mAllocatedDescriptors[index] ) {
                MKT_CORE_LOGGER_ERROR( "Attempted to release an un-allocated descriptor" );
            }
#endif

            mAllocatedDescriptors[index] = false;
        }

        mNumAllocatedDescriptors -= count;

        if ( mSearchStart > baseIndex ) {
            mSearchStart = baseIndex;
        }
    }

    auto StaticDescriptorHeap::AllocateResources( D3D12_DESCRIPTOR_HEAP_TYPE heapType, core::u32 numDescriptors, bool shaderVisible ) -> HRESULT {
        MKT_BEGIN_PROFILER();
        mHeap = nullptr;
        mShaderVisibleHeap = nullptr;

        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.Type = heapType;
        heapDesc.NumDescriptors = numDescriptors;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        HRESULT hr{ mDevice->CreateDescriptorHeap( &heapDesc, IID_PPV_ARGS( &mHeap ) ) };

        if ( FAILED( hr ) ) {
            return hr;
        }

        if ( !mDebugName.empty() ) {
            const eastl::string debugName{ string::Format( "{}_Heap_Type({})",
                mDebugName.empty() ? "Unnamed" : mDebugName.c_str(), GetHeapTypeName( heapType ) ) };
            mHeap->SetName( string::ToWide( debugName ).c_str() );
        }

        if ( shaderVisible ) {
            heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            hr = mDevice->CreateDescriptorHeap( &heapDesc, IID_PPV_ARGS( &mShaderVisibleHeap ) );

            if ( FAILED( hr ) ) {
                return hr;
            }

            if ( !mDebugName.empty() ) {
                const eastl::string debugName{ string::Format( "{}_ShaderVisibleHeap_Type({})",
                    mDebugName.empty() ? "Unnamed" : mDebugName.c_str(), GetHeapTypeName( heapType ) ) };
                mShaderVisibleHeap->SetName( string::ToWide( debugName ).c_str() );
            }

            mStartCpuHandleShaderVisible = mShaderVisibleHeap->GetCPUDescriptorHandleForHeapStart();
            mStartGpuHandleShaderVisible = mShaderVisibleHeap->GetGPUDescriptorHandleForHeapStart();
        }

        mCpuPages.clear();
        mCpuPages.emplace_back( 0, mHeap );

        mHeapType = heapDesc.Type;
        mNumDescriptors = heapDesc.NumDescriptors;

        mStartCpuHandle = mHeap->GetCPUDescriptorHandleForHeapStart();
        mStride = mDevice->GetDescriptorHandleIncrementSize( heapDesc.Type );
        mAllocatedDescriptors.resize( mNumDescriptors );

        return S_OK;
    }

    auto StaticDescriptorHeap::GetHeap() const -> ID3D12DescriptorHeap* {
        MKT_BEGIN_PROFILER();
        return mHeap.Get();
    }

    auto StaticDescriptorHeap::GetHeapType() const -> D3D12_DESCRIPTOR_HEAP_TYPE {
        MKT_BEGIN_PROFILER();
        return mHeapType;
    }

    auto StaticDescriptorHeap::GetCpuHandle( DescriptorIndex index ) const -> D3D12_CPU_DESCRIPTOR_HANDLE {
        std::scoped_lock lock{ mMutex };

        if ( index >= mNumDescriptors ) {
            MKT_THROW_RUNTIME_ERROR( "D3D12 attachment descriptor index is out of bounds." );
        }

        for ( auto it{ mCpuPages.rbegin() }; it != mCpuPages.rend(); ++it ) {
            if ( index >= it->first ) {
                auto result{ it->second->GetCPUDescriptorHandleForHeapStart() };
                result.ptr += ( index - it->first ) * mStride;
                return result;
            }
        }

        MKT_THROW_RUNTIME_ERROR( "D3D12 attachment descriptor has no heap page." );
    }

    auto StaticDescriptorHeap::GetCpuHandleShaderVisible( DescriptorIndex index ) const -> D3D12_CPU_DESCRIPTOR_HANDLE {
        MKT_BEGIN_PROFILER();
        D3D12_CPU_DESCRIPTOR_HANDLE result{ mStartCpuHandleShaderVisible };
        result.ptr += index * mStride;
        return result;
    }

    auto StaticDescriptorHeap::GetGpuHandle( DescriptorIndex index ) const -> D3D12_GPU_DESCRIPTOR_HANDLE {
        MKT_BEGIN_PROFILER();
        D3D12_GPU_DESCRIPTOR_HANDLE result{ mStartGpuHandleShaderVisible };
        result.ptr += index * mStride;
        return result;
    }

    auto StaticDescriptorHeap::GetShaderVisibleHeap() const -> ID3D12DescriptorHeap* {
        MKT_BEGIN_PROFILER();
        return mShaderVisibleHeap.Get();
    }

    auto StaticDescriptorHeap::Grow( core::u32 minRequiredSize ) -> HRESULT {
        MKT_BEGIN_PROFILER();

        if ( !mShaderVisibleHeap ) {
            // CPU attachment handles remain valid while other threads record.
            // Growing must never relocate descriptors another recording has fetched.
            D3D12_DESCRIPTOR_HEAP_DESC desc{};
            desc.Type = mHeapType;
            desc.NumDescriptors = math::NextPowerOf2( glm::max( mNumDescriptors, minRequiredSize - mNumDescriptors ) );
            Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> page{};
            const auto result{ mDevice->CreateDescriptorHeap( &desc, IID_PPV_ARGS( &page ) ) };

            if ( FAILED( result ) ) {
                return result;
            }

            mCpuPages.emplace_back( mNumDescriptors, eastl::move( page ) );
            mNumDescriptors += desc.NumDescriptors;
            mAllocatedDescriptors.resize( mNumDescriptors );
            return S_OK;
        }

        core::u32 oldSize{ mNumDescriptors };
        core::u32 newSize{ math::NextPowerOf2( minRequiredSize ) };

        bool const isShaderVisible{ mShaderVisibleHeap != nullptr };

        if ( isShaderVisible ) {
            const core::u32 maxSize( ( mHeapType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV )
                                         ? D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1// Not a power of 2!
                                         : D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE );

            newSize = eastl::min( newSize, maxSize );

            if ( newSize < minRequiredSize ) {
                return E_OUTOFMEMORY;
            }
        }

        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> oldHeap{ mHeap };

        HRESULT hr{ AllocateResources( mHeapType, newSize, isShaderVisible ) };

        if ( FAILED( hr ) ) {
            return hr;
        }

        mDevice->CopyDescriptorsSimple( oldSize, mStartCpuHandle, oldHeap->GetCPUDescriptorHandleForHeapStart(), mHeapType );

        if ( mShaderVisibleHeap != nullptr ) {
            mDevice->CopyDescriptorsSimple( oldSize, mStartCpuHandleShaderVisible, oldHeap->GetCPUDescriptorHandleForHeapStart(), mHeapType );
        }

        return S_OK;
    }

    DynamicDescriptorHeap::DynamicDescriptorHeap( renderer::rhi::IGpuDevice* device, eastl::string_view debugName )
        : mDebugName{ debugName } {
        MKT_BEGIN_PROFILER();
        Device* pDev{ core::checked_cast<Device*>( device ) };
        mDevice = pDev->GetDevice();
    }

    auto DynamicDescriptorHeap::AllocateDescriptor() -> DescriptorIndex {
        return AllocateDescriptors( 1 );
    }

    auto DynamicDescriptorHeap::ReleaseDescriptor( DescriptorIndex index ) -> void {
        ReleaseDescriptors( index, 1 );
    }

    auto DynamicDescriptorHeap::AllocateDescriptors( core::u32 count ) -> DescriptorIndex {
        if ( count == 0 ) {
            return kInvalidDescriptorIndex;
        }

        std::lock_guard lock{ mIndicesMutex };

        for ( auto it{ mFreeRanges.begin() }; it != mFreeRanges.end(); ++it ) {
            if ( it->second >= count ) {
                const auto first{ it->first };
                it->first += count;
                it->second -= count;

                if ( it->second == 0 ) {
                    mFreeRanges.erase( it );
                }

                return first;
            }
        }

        MKT_THROW_RUNTIME_ERROR( "D3D12 shader descriptor heap has no contiguous range of the requested size." );
    }

    auto DynamicDescriptorHeap::ReleaseDescriptors( DescriptorIndex baseIndex, core::u32 count ) -> void {
        if ( count == 0 || baseIndex == kInvalidDescriptorIndex ) {
            return;
        }

        std::lock_guard lock{ mIndicesMutex };

        if ( baseIndex >= mCapacity || count > mCapacity - baseIndex ) {
            MKT_THROW_RUNTIME_ERROR( "D3D12 descriptor release is outside the heap." );
        }

        auto it{ eastl::lower_bound( mFreeRanges.begin(), mFreeRanges.end(), baseIndex,
            []( const auto& range, DescriptorIndex index ) {
            return range.first < index;
        } ) };

        if ( ( it != mFreeRanges.end() && baseIndex + count > it->first ) ||
             ( it != mFreeRanges.begin() && ( it - 1 )->first + ( it - 1 )->second > baseIndex ) ) {
            MKT_THROW_RUNTIME_ERROR( "D3D12 descriptor range was released twice." );
        }

        it = mFreeRanges.insert( it, { baseIndex, count } );

        if ( it + 1 != mFreeRanges.end() && it->first + it->second == ( it + 1 )->first ) {
            it->second += ( it + 1 )->second;
            mFreeRanges.erase( it + 1 );
        }

        if ( it != mFreeRanges.begin() && ( it - 1 )->first + ( it - 1 )->second == it->first ) {
            ( it - 1 )->second += it->second;
            mFreeRanges.erase( it );
        }
    }

    auto DynamicDescriptorHeap::AllocateResources( D3D12_DESCRIPTOR_HEAP_TYPE heapType, core::usize capacity ) -> HRESULT {
        MKT_BEGIN_PROFILER();
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.Type = heapType;
        heapDesc.NumDescriptors = capacity;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        mCapacity = capacity;

        HRESULT hr{ mDevice->CreateDescriptorHeap( &heapDesc, IID_PPV_ARGS( &mShaderHeap ) ) };

        if ( FAILED( hr ) ) {
            return hr;
        }

        if ( !mDebugName.empty() ) {
            const eastl::string debugName{ string::Format( "{}_ShaderGlobalHeap_Type({})",
                mDebugName.empty() ? "Unnamed" : mDebugName.c_str(), GetHeapTypeName( heapType ) ) };
            mShaderHeap->SetName( string::ToWide( debugName ).c_str() );
        }

        mStartCpuHandle = mShaderHeap->GetCPUDescriptorHandleForHeapStart();
        mStartGpuHandle = mShaderHeap->GetGPUDescriptorHandleForHeapStart();

        mHeapType = heapDesc.Type;

        mStartCpuHandle = mShaderHeap->GetCPUDescriptorHandleForHeapStart();
        mDescriptorStride = mDevice->GetDescriptorHandleIncrementSize( heapDesc.Type );

        mFreeRanges.clear();
        mFreeRanges.emplace_back( 0, static_cast<core::u32>( capacity ) );

        return S_OK;
    }

    auto DynamicDescriptorHeap::GetShaderVisibleHeap() const -> ID3D12DescriptorHeap* {
        MKT_BEGIN_PROFILER();
        return mShaderHeap.Get();
    }

    auto DynamicDescriptorHeap::GetCpuHandleShaderVisible( DescriptorIndex index ) const -> D3D12_CPU_DESCRIPTOR_HANDLE {
        return D3D12_CPU_DESCRIPTOR_HANDLE{};
    }

    auto DynamicDescriptorHeap::GetHeap() const -> ID3D12DescriptorHeap* {
        MKT_BEGIN_PROFILER();
        return mShaderHeap.Get();
    }

    auto DynamicDescriptorHeap::GetHeapType() const -> D3D12_DESCRIPTOR_HEAP_TYPE {
        MKT_BEGIN_PROFILER();
        return mHeapType;
    }

    auto DynamicDescriptorHeap::GetCpuHandle( DescriptorIndex index ) const -> D3D12_CPU_DESCRIPTOR_HANDLE {
        MKT_BEGIN_PROFILER();
        D3D12_CPU_DESCRIPTOR_HANDLE result{ mStartCpuHandle };
        result.ptr += index * mDescriptorStride;
        return result;
    }

    auto DynamicDescriptorHeap::GetGpuHandle( DescriptorIndex index ) const -> D3D12_GPU_DESCRIPTOR_HANDLE {
        MKT_BEGIN_PROFILER();
        D3D12_GPU_DESCRIPTOR_HANDLE result{ mStartGpuHandle };
        result.ptr += index * mDescriptorStride;
        return result;
    }

    auto BindingLayout::IsBindless() const -> bool {
        MKT_BEGIN_PROFILER();
        return mIsBindless;
    }

    auto BindingLayout::GetBindingLayoutDesc() const -> const renderer::rhi::BindingLayoutDescription& {
        MKT_BEGIN_PROFILER();
        return mBindingLayoutDesc;
    }

    BindingLayout::BindingLayout( const renderer::rhi::BindingLayoutDescription& desc )
        : mRegisterSpace{ desc.mRegisterSpace }, mIsBindless{ false }, mBindingLayoutDesc{ desc } {
    }

    BindingLayout::BindingLayout( const renderer::rhi::DescriptorTableLayoutDescription& desc )
        : mRegisterSpace{ desc.mRegisterSpace }, mIsBindless{ true }, mBindlessLayoutDesc{ desc } {
    }

    auto BindingLayout::GetBindlessLayoutDesc() const -> const renderer::rhi::DescriptorTableLayoutDescription& {
        MKT_BEGIN_PROFILER();
        return mBindlessLayoutDesc;
    }

    auto BindingLayout::GetDescriptorRanges() const -> const eastl::vector<D3D12_DESCRIPTOR_RANGE1>& {
        MKT_BEGIN_PROFILER();
        return mDescriptorRanges;
    }

    auto BindingLayout::GetDescriptorSamplerRanges() const -> const eastl::vector<D3D12_DESCRIPTOR_RANGE1>& {
        MKT_BEGIN_PROFILER();
        return mDescriptorRangesSamplers;
    }

    auto BindingLayout::GetNextIndexForDescriptor( D3D12_DESCRIPTOR_RANGE_TYPE descriptor ) const -> core::u32 {
        MKT_BEGIN_PROFILER();
        const auto it{ mNextRegisterForType.find( descriptor ) };
        return it == mNextRegisterForType.end() ? 0 : it->second;
    }

    auto BindingLayout::SetDebugName( eastl::string_view name ) -> void {
        MKT_BEGIN_PROFILER();
        renderer::rhi::IBindingLayout::SetDebugName( name );
    }

    auto BindingLayout::GetNativeHandle( renderer::rhi::ObjectType type ) -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();
        return renderer::rhi::IBindingLayout::GetNativeHandle( type );
    }

    auto BindingLayout::GetNativeHandle( renderer::rhi::ObjectType type ) const -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();
        return renderer::rhi::IBindingLayout::GetNativeHandle( type );
    }

    auto BindingLayout::GetRegisterSpace() const -> core::u32 {
        MKT_BEGIN_PROFILER();
        return mRegisterSpace;
    }

    BindingLayout::~BindingLayout() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto BindingLayout::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        Device* device{ core::checked_cast<Device*>( mDevice ) };

        if ( !mIsBindless ) {
            for ( const auto& descriptor: mBindingLayoutDesc.mBindings ) {
                D3D12_DESCRIPTOR_RANGE1 range{};

                // Convert the RHI type to DX12 range type (SRV, UAV, CBV, or SAMPLER)
                D3D12_DESCRIPTOR_RANGE_TYPE dx12Type{ d3d12::GetDescriptorRangeType( descriptor.mType ) };

                range.RangeType = dx12Type;
                range.NumDescriptors = 1;
                range.RegisterSpace = mRegisterSpace;

                // Let DX12 calculate the offset automatically
                range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

                range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;

                // Automatically assign the next free register for this specific resource type
                range.BaseShaderRegister = mNextRegisterForType[dx12Type]++;

                // Separate Samplers because DX12 requires them in a dedicated Sampler Descriptor Heap
                if ( descriptor.mType == renderer::rhi::ResourceType::eSampler ) {
                    mDescriptorRangesSamplers.emplace_back( range );
                }
                else {
                    mDescriptorRanges.emplace_back( range );
                }
            }
        }
        else {
            // TODO: Implement bindless for shader model 5.1
            if ( !device->IsSupported( DeviceFeatureSupport::eShaderModel6_6 ) ) {
                // Each bindless array gets its own space starting
                // from the one we specified as base
                core::u32 registerSpaceOffsetIndex{ mRegisterSpace };

                for ( const auto& descriptor: mBindlessLayoutDesc.mSlots ) {
                    D3D12_DESCRIPTOR_RANGE1 range{};

                    // Convert the RHI type to DX12 range type (SRV, UAV, CBV, or SAMPLER)
                    D3D12_DESCRIPTOR_RANGE_TYPE dx12Type{ d3d12::GetDescriptorRangeType( descriptor.mResourceType ) };

                    range.RangeType = dx12Type;
                    range.BaseShaderRegister = 0;
                    range.NumDescriptors = UINT_MAX;
                    range.RegisterSpace = registerSpaceOffsetIndex + descriptor.mSlot;

                    // Let DX12 calculate the offset automatically
                    range.OffsetInDescriptorsFromTableStart = 0;

                    range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;

                    // Separate Samplers because DX12 requires them in a dedicated Sampler Descriptor Heap
                    if ( descriptor.mResourceType == renderer::rhi::ResourceType::eSampler ) {
                        mDescriptorRangesSamplers.emplace_back( range );
                    }
                    else {
                        mDescriptorRanges.emplace_back( range );
                    }
                }
            }
        }

        mIsAllocated = true;
    }

    auto BindingLayout::Destroy() -> void {
        MKT_BEGIN_PROFILER();
        mIsAllocated = false;
    }

    BindingTable::BindingTable( const renderer::rhi::BindingTableDescription& desc, renderer::rhi::BindingLayoutHandle layout, DeviceResources& resources )
        : mBindingLayout{ layout }, mBindingDescription{ desc }, mDeviceResources{ MKT_ADDRESSOF( resources ) } {
        for ( const auto& binding: desc.mBindings ) {
            if ( binding.mResource ) {
                mResources.emplace_back( core::checked_cast<renderer::rhi::DeviceObject*>( binding.mResource ) );
            }
        }
    }

    auto BindingTable::SetDebugName( eastl::string_view name ) -> void {
    }

    auto BindingTable::GetNativeHandle( renderer::rhi::ObjectType type ) -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();
        return renderer::rhi::IBindingTable::GetNativeHandle( type );
    }

    auto BindingTable::GetNativeHandle( renderer::rhi::ObjectType type ) const -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();
        return renderer::rhi::IBindingTable::GetNativeHandle( type );
    }

    auto BindingTable::GetSrvRange() const -> const DescriptorRange& {
        MKT_BEGIN_PROFILER();
        return mSrvRange;
    }

    auto BindingTable::GetSamplerRange() const -> const DescriptorRange& {
        MKT_BEGIN_PROFILER();
        return mSamplerRange;
    }

    auto BindingTable::Snapshot() -> renderer::rhi::BindingTableHandle {
        std::scoped_lock lock{ mVersionMutex };
        return mCurrentVersion ? mCurrentVersion : renderer::rhi::BindingTableHandle{ this };
    }

    auto BindingTable::Update( const renderer::rhi::BindingTableDescription& desc ) -> void {
        // Publish a fresh allocation; recorded commands retain their previous
        // snapshot through the submission fence rather than seeing overwritten descriptors.
        auto replacement{ mDevice->CreateBindingTable( desc, mBindingLayout ) };
        std::scoped_lock lock{ mVersionMutex };
        mCurrentVersion = eastl::move( replacement );
    }

    BindingTable::~BindingTable() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto BindingTable::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        Device* device{ core::checked_cast<Device*>( mDevice ) };

        // Handle samplers first
        core::u32 totalSamplerCount{ ( core::u32 )std::ranges::count_if( mBindingDescription.mBindings,
            []( const renderer::rhi::BindingTableItem& binding ) {
            return binding.mResourceType == renderer::rhi::ResourceType::eSampler;
        } ) };

        mSamplerRange.mBaseIndex = mDeviceResources->mSamplerDescriptorHeap->AllocateDescriptors( totalSamplerCount );
        mSamplerRange.mCount = totalSamplerCount;

        for ( core::u32 samplerIndex{}, bindingIndex{}; samplerIndex < totalSamplerCount && bindingIndex < mBindingDescription.mBindings.size(); ++bindingIndex ) {
            const renderer::rhi::BindingTableItem& binding{ mBindingDescription.mBindings[bindingIndex] };

            if ( rhi::IsSampler( binding.mResourceType ) ) {
                DescriptorIndex index{ mSamplerRange.mBaseIndex + samplerIndex };
                D3D12_CPU_DESCRIPTOR_HANDLE cpu{ mDeviceResources->mSamplerDescriptorHeap->GetCpuHandle( index ) };

                Sampler* sampler{ core::checked_cast<Sampler*>( binding.mResource ) };
                sampler->AllocateSampler( cpu );

                // This was for pre 6.6. When I implement it again I'll need to look into this
                // This is line is here because handle comes from heap without D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                //mDeviceResources->mSamplerHeap->CopyToShaderVisibleHeap(index);
                ++samplerIndex;
            }
        }

        if ( totalSamplerCount != 0 ) {
            mSamplerRange.mGpuHandle = mDeviceResources->mSamplerDescriptorHeap->GetGpuHandle( mSamplerRange.mBaseIndex );
        }

        // Handle rest of descriptor types
        // Allocate one contiguous range for all SRV/etc descriptors
        core::u32 totalDescriptorCount{ core::as<core::u32>( mBindingDescription.mBindings.size() ) - totalSamplerCount };

        mSrvRange.mBaseIndex = mDeviceResources->mResourceDescriptorHeap->AllocateDescriptors( totalDescriptorCount );
        mSrvRange.mCount = totalDescriptorCount;

        for ( core::u32 descriptorIndex{}, bindingIndex{}; descriptorIndex < totalDescriptorCount && bindingIndex < mBindingDescription.mBindings.size(); ++bindingIndex ) {
            const renderer::rhi::BindingTableItem& binding{ mBindingDescription.mBindings[bindingIndex] };

            if ( !rhi::IsSampler( binding.mResourceType ) ) {
                DescriptorIndex index{ mSrvRange.mBaseIndex + descriptorIndex };
                D3D12_CPU_DESCRIPTOR_HANDLE cpu{ mDeviceResources->mResourceDescriptorHeap->GetCpuHandle( index ) };

                if ( rhi::IsBuffer( binding.mResourceType ) ) {
                    Buffer* buffer{ core::checked_cast<Buffer*>( binding.mResource ) };

                    if ( binding.mResource ) {
                        switch ( binding.mResourceType ) {
                            case renderer::rhi::ResourceType::eRawBuffer_SRV:
                            case renderer::rhi::ResourceType::eTypedBuffer_SRV:
                            case renderer::rhi::ResourceType::eStructuredBuffer_SRV:
                                buffer->CreateSRV( cpu.ptr, binding.mRange, binding.mResourceType, binding.mFormat );
                                break;
                            case renderer::rhi::ResourceType::eRawBuffer_UAV:
                            case renderer::rhi::ResourceType::eTypedBuffer_UAV:
                            case renderer::rhi::ResourceType::eStructuredBuffer_UAV:
                                buffer->CreateUAV( cpu.ptr, binding.mRange, binding.mResourceType, binding.mFormat );
                                break;
                            case renderer::rhi::ResourceType::eConstantBuffer:
                                buffer->CreateCBV( cpu.ptr, binding.mRange, binding.mFormat );
                                break;
                            default:;
                        }
                    }
                    else {
                        Buffer::CreateNullSRV( cpu.ptr, binding.mFormat, device->GetDevice() );
                    }
                }
                else if ( rhi::IsTexture( binding.mResourceType ) ) {
                    Texture* texture{ core::checked_cast<Texture*>( binding.mResource ) };

                    if ( binding.mResource ) {
                        switch ( binding.mResourceType ) {
                            case renderer::rhi::ResourceType::eTexture_SRV:
                                texture->CreateSrv( cpu.ptr, binding.mSubResourceSet, binding.mFormat, binding.mDimension );
                                break;
                            case renderer::rhi::ResourceType::eTexture_UAV:
                                texture->CreateUav( cpu.ptr, binding.mSubResourceSet, binding.mFormat, binding.mDimension );
                                break;
                            default:;
                        }
                    }
                }

                // See comment above for samplers
                // mDeviceResources->mShaderResourceViewHeap->CopyToShaderVisibleHeap(index);

                ++descriptorIndex;
            }
        }

        if ( totalDescriptorCount != 0 ) {
            mSrvRange.mGpuHandle = mDeviceResources->mResourceDescriptorHeap->GetGpuHandle( mSrvRange.mBaseIndex );
        }

        mIsAllocated = true;
    }

    auto BindingTable::Destroy() -> void {
        MKT_BEGIN_PROFILER();
        // Release descriptor indices
        //mDeviceResources->mShaderResourceViewHeap->ReleaseDescriptors(mSrvRange.mBaseIndex, descriptorCount);
        //mDeviceResources->mResourceDescriptorHeap->ReleaseDescriptors(mSrvRange.mBaseIndex, descriptorCount);

        core::u32 totalSamplerCount{ ( core::u32 )std::ranges::count_if( mBindingDescription.mBindings,
            []( const renderer::rhi::BindingTableItem& binding ) {
            return binding.mResourceType == renderer::rhi::ResourceType::eSampler;
        } ) };
        mDeviceResources->mSamplerDescriptorHeap->ReleaseDescriptors( mSamplerRange.mBaseIndex, totalSamplerCount );

        core::u32 totalDescriptorCount{ core::as<core::u32>( mBindingDescription.mBindings.size() ) - totalSamplerCount };
        mDeviceResources->mResourceDescriptorHeap->ReleaseDescriptors( mSrvRange.mBaseIndex, totalDescriptorCount );

        mIsAllocated = false;
    }

    DescriptorTable::DescriptorTable( renderer::rhi::BindingLayoutHandle setLayout, DeviceResources& resources )
        : mBindingLayout{ eastl::move( setLayout ) }, mDeviceResources{ MKT_ADDRESSOF( resources ) } {
    }

    auto DescriptorTable::SetDebugName( eastl::string_view name ) -> void {
        MKT_BEGIN_PROFILER();

        if ( name.empty() ) {
            return;
        }

        mDebugName = name;
    }

    auto DescriptorTable::GetCapacity( core::u32 slot ) const -> core::u32 {
        MKT_BEGIN_PROFILER();
        const BindingLayout* layout{ core::checked_cast<const BindingLayout*>( mBindingLayout.GetPtr() ) };
        const auto& blDesc{ layout->GetBindlessLayoutDesc() };

        const auto it{ eastl::find_if( blDesc.mSlots.begin(), blDesc.mSlots.end(),
            [slot]( const renderer::rhi::BindlessLayoutItem& item ) {
            return item.mSlot == slot;
        } ) };

        return it != blDesc.mSlots.end() ? it->mMaxCapacity : 0;
    }

    auto DescriptorTable::AllocateIndex( renderer::rhi::ResourceType type, core::u32 slot ) -> DescriptorIndex {
        std::scoped_lock lock{ mDescriptorMutex };
        const bool sampler{ rhi::IsSampler( type ) };
        auto& live{ sampler ? mSamplerIndices[slot] : mResourceIndices[slot] };

        if ( live.size() >= GetCapacity( slot ) ) {
            MKT_THROW_RUNTIME_ERROR( "D3D12 descriptor table slot capacity exhausted." );
        }

        auto* heap{ sampler ? mDeviceResources->mSamplerDescriptorHeap.get() : mDeviceResources->mResourceDescriptorHeap.get() };
        const auto index{ heap->AllocateDescriptor() };
        live.emplace( index );
        return index;
    }

    auto DescriptorTable::UnRegisterIndex( DescriptorIndex index, renderer::rhi::ResourceType type, core::u32 slot ) -> void {
        std::scoped_lock lock{ mDescriptorMutex };
        const bool sampler{ rhi::IsSampler( type ) };
        auto& live{ sampler ? mSamplerIndices[slot] : mResourceIndices[slot] };

        if ( live.erase( index ) ) {
            mResources.erase( ( core::u64{ slot } << 32 ) | index );
            auto* heap{ sampler ? mDeviceResources->mSamplerDescriptorHeap.get() : mDeviceResources->mResourceDescriptorHeap.get() };
            heap->ReleaseDescriptor( index );
        }
    }

    auto DescriptorTable::GetLayout() const -> const BindingLayout* {
        MKT_BEGIN_PROFILER();
        return core::checked_cast<const BindingLayout*>( mBindingLayout.GetPtr() );
    }

    auto DescriptorTable::Initialize() -> void {
        // Directly indexed heaps need stable global indices, not a reserved
        // contiguous range for every frame graph and every declared slot.
        mIsAllocated = true;
    }

    auto DescriptorTable::Destroy() -> void {
        for ( const auto& [slot, indices]: mResourceIndices ) {
            for ( const auto index: indices ) {
                mDeviceResources->mResourceDescriptorHeap->ReleaseDescriptor( index );
            }
        }

        for ( const auto& [slot, indices]: mSamplerIndices ) {
            for ( const auto index: indices ) {
                mDeviceResources->mSamplerDescriptorHeap->ReleaseDescriptor( index );
            }
        }

        mResourceIndices.clear();
        mSamplerIndices.clear();
        mIsAllocated = false;
    }

    DescriptorTable::~DescriptorTable() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto DescriptorTable::GetNativeHandle( renderer::rhi::ObjectType type ) -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        if ( type != renderer::rhi::ObjectType::Vk_DescriptorSet ) {
            return renderer::rhi::Object( nullptr );
        }

        return renderer::rhi::Object( nullptr );
    }

    auto DescriptorTable::GetNativeHandle( renderer::rhi::ObjectType type ) const -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        if ( type != renderer::rhi::ObjectType::Vk_DescriptorSet ) {
            return renderer::rhi::Object( nullptr );
        }

        return renderer::rhi::Object( nullptr );
    }

    InputLayout::InputLayout( const renderer::rhi::InputLayoutCreateDescription& desc )
        : mDescription{ desc } {
        MKT_BEGIN_PROFILER();
        // D3D12 keeps semantic-name pointers until pipeline creation. Point into
        // our owned description, since the caller's description may be temporary.
        for ( const auto& bindingDesc: mDescription.mVertexAttributeDescriptions ) {
            const auto bindingIt{ std::ranges::find_if( mDescription.mVertexBindingDescriptions,
                [index = bindingDesc.mBinding]( const renderer::rhi::VertexBindingDescription& item ) {
                return item.mBinding == index;
            } ) };

            MKT_ASSERT( bindingIt != mDescription.mVertexBindingDescriptions.end(), "No binding found for the given index" );
            const auto& binding{ *bindingIt };

            D3D12_INPUT_ELEMENT_DESC description{
                .SemanticName = bindingDesc.mName.c_str(),
                .SemanticIndex = mSemanticIndexPerName[bindingDesc.mName]++,
                .Format = d3d12::GetFormat( bindingDesc.mFormat ),
                .InputSlot = bindingDesc.mBinding,
                .AlignedByteOffset = bindingDesc.mOffset,
                .InputSlotClass = ( binding.mRate == renderer::rhi::InputRate::ePerVertex )
                                      ? D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA
                                      : D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,
                .InstanceDataStepRate = core::as<UINT>( binding.mRate == renderer::rhi::InputRate::ePerVertex ? 0 : 1 )
            };
            mInputElems.emplace_back( description );
            mAttributes.emplace( bindingDesc.mLocation, bindingDesc );
        }
    }

    auto InputLayout::GetNumAttributes() const -> core::u32 {
        MKT_BEGIN_PROFILER();
        return mAttributes.size();
    }

    auto InputLayout::GetAttributeDescription( core::u32 index ) const -> const renderer::rhi::VertexAttributeDescription& {
        MKT_BEGIN_PROFILER();
        return mAttributes.at( index );
    }

    auto InputLayout::GetInputElements() -> const D3D12_INPUT_ELEMENT_DESC* {
        MKT_BEGIN_PROFILER();
        return mInputElems.data();
    }

    auto InputLayout::GetInputElementsCount() -> UINT {
        MKT_BEGIN_PROFILER();
        return core::as<UINT>( mInputElems.size() );
    }

    InputLayout::~InputLayout() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto InputLayout::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        mIsAllocated = true;
    }

    auto InputLayout::Destroy() -> void {
        MKT_BEGIN_PROFILER();
        mIsAllocated = false;
    }

    PipelineLayout::PipelineLayout( const renderer::rhi::PipelineLayoutCreateDescription& description )
        : mDescription{ description } {
    }

    auto PipelineLayout::SetDebugName( const eastl::string_view name ) -> void {
        MKT_BEGIN_PROFILER();

        if ( name.empty() ) {
            return;
        }

        mDebugName = name;
        mRootSignature->SetName( string::ToWide( name ).c_str() );
    }

    auto PipelineLayout::GetNativeHandle( renderer::rhi::ObjectType type ) -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();
        return renderer::rhi::IPipelineLayout::GetNativeHandle( type );
    }

    auto PipelineLayout::GetNativeHandle( renderer::rhi::ObjectType type ) const -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();
        return renderer::rhi::IPipelineLayout::GetNativeHandle( type );
    }

    auto PipelineLayout::GetRootConstantIndex() const -> core::u32 {
        MKT_BEGIN_PROFILER();
        return mRootConstantIndex;
    }

    auto PipelineLayout::GetMax32BitValuesCount() const -> core::u32 {
        MKT_BEGIN_PROFILER();
        return m32BitValueCount;
    }

    auto PipelineLayout::GetBindingRootIndex( core::u32 layoutIndex, bool sampler ) const -> core::u32 {
        const auto& indices{ sampler ? mSamplerRootIndices : mResourceRootIndices };

        if ( layoutIndex >= indices.size() || indices[layoutIndex] == UINT_MAX ) {
            MKT_THROW_RUNTIME_ERROR( "D3D12 binding set has no corresponding root descriptor table." );
        }

        return indices[layoutIndex];
    }

    auto PipelineLayout::GetDescription() const -> const renderer::rhi::PipelineLayoutCreateDescription& {
        MKT_BEGIN_PROFILER();
        return mDescription;
    }

    PipelineLayout::operator ID3D12RootSignature*() const {
        MKT_BEGIN_PROFILER();
        return mRootSignature.Get();
    }

    PipelineLayout::~PipelineLayout() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto PipelineLayout::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        Device* device{ core::checked_cast<Device*>( mDevice ) };
        ID3D12Device2* d3d12Device{ device->GetDevice() };

        // Check root signature availability
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData{};
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

        if ( FAILED( d3d12Device->CheckFeatureSupport( D3D12_FEATURE_ROOT_SIGNATURE,
                 MKT_ADDRESSOF( featureData ), MKT_SIZEOF( featureData ) ) ) ) {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }


        if ( featureData.HighestVersion < D3D_ROOT_SIGNATURE_VERSION_1_1 ) {
            MKT_THROW_RUNTIME_ERROR( "D3D12 bindless pipelines require root signature version 1.1." );
        }

        if ( mDescription.mPushConstantsSize % sizeof( core::u32 ) || mDescription.mPushConstantsSize > 256 ) {
            MKT_THROW_RUNTIME_ERROR( "D3D12 root constants must contain at most 64 aligned 32-bit values." );
        }

        m32BitValueCount = static_cast<core::u32>( mDescription.mPushConstantsSize / sizeof( core::u32 ) );
        mResourceRootIndices.assign( mDescription.mBindingLayouts.size(), UINT_MAX );
        mSamplerRootIndices.assign( mDescription.mBindingLayouts.size(), UINT_MAX );
        core::u32 constantRegister{};
        eastl::vector<D3D12_ROOT_PARAMETER1> rootParameters{};
        // Keep RHI layout order. Shader registers and root parameter indices
        // are separate namespaces; one layout may produce two root tables.
        for ( core::u32 index{}; index < mDescription.mBindingLayouts.size(); ++index ) {
            auto* layout{ core::checked_cast<BindingLayout*>( mDescription.mBindingLayouts[index].GetPtr() ) };
            const auto visibility{ layout->IsBindless() ? layout->GetBindlessLayoutDesc().mStageVisibility : layout->GetBindingLayoutDesc().mStageVisibility };

            if ( layout->GetRegisterSpace() == 0 ) {
                constantRegister = eastl::max( constantRegister, layout->GetNextIndexForDescriptor( D3D12_DESCRIPTOR_RANGE_TYPE_CBV ) );
            }

            const auto addTable{ [&]( const auto& ranges, core::u32& rootIndex ) {
                if ( !ranges.empty() ) {
                    rootIndex = static_cast<core::u32>( rootParameters.size() );
                    auto& parameter{ rootParameters.emplace_back() };
                    parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                    parameter.ShaderVisibility = d3d12::GetShaderVisibility( visibility );
                    parameter.DescriptorTable.NumDescriptorRanges = static_cast<UINT>( ranges.size() );
                    parameter.DescriptorTable.pDescriptorRanges = ranges.data();
                }
            } };
            addTable( layout->GetDescriptorRanges(), mResourceRootIndices[index] );
            addTable( layout->GetDescriptorSamplerRanges(), mSamplerRootIndices[index] );
        }

        if ( rootParameters.size() + m32BitValueCount > 64 ) {
            MKT_THROW_RUNTIME_ERROR( "D3D12 root signature exceeds its 64 DWORD budget." );
        }

        mRootConstantIndex = static_cast<core::u32>( rootParameters.size() );

        if ( m32BitValueCount ) {
            auto& constants{ rootParameters.emplace_back() };
            constants.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
            constants.ShaderVisibility = d3d12::GetShaderVisibility( mDescription.mPushConstantsVisibility );
            constants.Constants.ShaderRegister = constantRegister;
            constants.Constants.RegisterSpace = 0;
            constants.Constants.Num32BitValues = m32BitValueCount;
        }

        D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc{};
        rootSignatureDesc.Version = featureData.HighestVersion;
        rootSignatureDesc.Desc_1_1.Flags =
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED |
            D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED;
        rootSignatureDesc.Desc_1_1.NumParameters = core::as<UINT>( rootParameters.size() );
        rootSignatureDesc.Desc_1_1.pParameters = rootParameters.data();
        rootSignatureDesc.Desc_1_1.NumStaticSamplers = 0;
        rootSignatureDesc.Desc_1_1.pStaticSamplers = nullptr;

        HRESULT hr{ D3D12SerializeVersionedRootSignature( &rootSignatureDesc,
            &mSignatureBlob, &mErrorMessages ) };

        if ( mErrorMessages && mErrorMessages->GetBufferSize() != 0 ) {
            eastl::string msg{ ( const char* )mErrorMessages->GetBufferPointer() };
            MKT_CORE_LOGGER_ERROR( "D3D12SerializeVersionedRootSignature Errors: {}", msg.c_str() );
        }

        ThrowIfFailed( hr );

        ThrowIfFailed( d3d12Device->CreateRootSignature( 0,
            mSignatureBlob->GetBufferPointer(),
            mSignatureBlob->GetBufferSize(),
            IID_PPV_ARGS( &mRootSignature ) ) );

        mIsAllocated = true;
    }

    auto PipelineLayout::Destroy() -> void {
        MKT_BEGIN_PROFILER();
        mIsAllocated = false;
    }

    Queue::Queue( renderer::rhi::QueueType type, renderer::rhi::QueueOpSupportFlags flags )
        : mType{ type }, mOpSupportFlags{ flags } {
    }

    auto Queue::ExecuteCommandLists( const renderer::rhi::SubmitInfo& submitInfo ) -> void {
        MKT_BEGIN_PROFILER();

        std::lock_guard lock{ mSubmissionMutex };
        eastl::fixed_vector<ID3D12CommandList*, 25> commands{};

        for ( auto handle: submitInfo.mCommands ) {
            auto* command{ core::checked_cast<CommandList*>( handle.GetPtr() ) };

            auto* native{ command->GetCommandList() };

            if ( eastl::find( commands.begin(), commands.end(), native ) != commands.end() ) {
                MKT_THROW_RUNTIME_ERROR( "D3D12 submission contains the same recording twice." );
            }

            commands.push_back( native );
        }

        mPendingCommands.reserve( mPendingCommands.size() + submitInfo.mCommands.size() );
        mPendingFences.reserve( mPendingFences.size() + submitInfo.mWaits.size() + submitInfo.mSignals.size() );

        for ( auto [value, handle]: submitInfo.mWaits ) {
            ID3D12Fence* fence{ *core::checked_cast<Fence*>( handle.GetPtr() ) };
            ThrowIfFailed( mQueue->Wait( fence, value ) );
        }

        if ( !commands.empty() ) {
            mQueue->ExecuteCommandLists( static_cast<UINT>( commands.size() ), commands.data() );
        }

        for ( auto [value, handle]: submitInfo.mSignals ) {
            ID3D12Fence* fence{ *core::checked_cast<Fence*>( handle.GetPtr() ) };
            ThrowIfFailed( mQueue->Signal( fence, value ) );
        }

        const auto submission{ ++mFenceValue };
        ID3D12Fence* timeline{ *core::checked_cast<Fence*>( mFence.GetPtr() ) };
        ThrowIfFailed( mQueue->Signal( timeline, submission ) );

        for ( auto handle: submitInfo.mCommands ) {
            core::checked_cast<CommandList*>( handle.GetPtr() )->MarkExecuted( this, submission );
            mPendingCommands.emplace_back( submission, handle );
        }

        for ( const auto& [value, handle]: submitInfo.mWaits ) {
            mPendingFences.emplace_back( submission, handle );
        }

        for ( const auto& [value, handle]: submitInfo.mSignals ) {
            mPendingFences.emplace_back( submission, handle );
        }
    }

    auto Queue::AllocateCmdList() -> renderer::rhi::CommandListHandle {
        MKT_BEGIN_PROFILER_NAMED();

        renderer::rhi::CommandListHandle handle{ core::Ref<CommandList>::New( this ) };
        handle->Initialize( mDevice );

        return handle;
    }

    auto Queue::WaitIdle() -> void {
        MKT_BEGIN_PROFILER();
        core::u64 value{};
        {
            std::lock_guard lock{ mSubmissionMutex };
            value = ++mFenceValue;
            ID3D12Fence* fence{ *core::checked_cast<Fence*>( mFence.GetPtr() ) };
            ThrowIfFailed( mQueue->Signal( fence, value ) );
        }

        WaitForSubmission( value );
        CollectGarbage();
    }

    auto Queue::WaitForSubmission( core::u64 value ) -> void {
        if ( !mFence->Wait( value, UINT64_MAX ) ) {
            MKT_THROW_RUNTIME_ERROR( "Failed waiting for the D3D12 queue submission." );
        }
    }

    auto Queue::CollectGarbage() -> void {
        std::lock_guard lock{ mSubmissionMutex };
        const auto completed{ GetCurrentTimeline() };
        const auto firstPending{ eastl::find_if( mPendingCommands.begin(), mPendingCommands.end(),
            [completed]( const auto& entry ) {
            return entry.first > completed;
        } ) };

        for ( auto it{ mPendingCommands.begin() }; it != firstPending; ++it ) {
            core::checked_cast<CommandList*>( it->second.GetPtr() )->ReleaseCompletedResources();
        }

        mPendingCommands.erase( mPendingCommands.begin(), firstPending );
        eastl::erase_if( mPendingFences, [completed]( const auto& entry ) {
            return entry.first <= completed;
        } );
    }

    auto Queue::Signal( Fence* fence, core::u64 value ) -> void {
        ExecuteCommandLists( renderer::rhi::SubmitInfo{}.AddSignal( renderer::rhi::FenceHandle{ fence }, value ) );
    }

    auto Queue::GetCurrentTimeline() -> core::u64 {
        MKT_BEGIN_PROFILER();
        return mFence->GetCompletionValue();
    }

    Queue::operator ID3D12CommandQueue*() const {
        MKT_BEGIN_PROFILER();
        return mQueue.Get();
    }

    Queue::~Queue() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto Queue::Destroy() -> void {
        MKT_BEGIN_PROFILER();
#if defined( MIKOTO_ENABLE_TRACY )
        mGpuProfiler.reset();
#endif
        mIsAllocated = false;
    }

#if defined( MIKOTO_ENABLE_TRACY )
    auto Queue::InitializeGpuProfiler( core::u64 connection ) -> void {
        std::lock_guard lock{ mSubmissionMutex };
        // The previous capture's query IDs have no matching zones in the new capture.
        // Keep its buffers alive in the recording slots until the GPU is finished.
        mGpuProfiler.reset();
        mGpuProfilerConnection = connection;

        if ( tracy::GetGpuCtxCounter().load() > UINT8_MAX ) {
            MKT_CORE_LOGGER_WARN( "Tracy has exhausted its GPU context IDs. Restart Mikoto to capture more GPU sessions." );
            return;
        }

        core::u64 frequency{}, gpuTimestamp{}, cpuTimestamp{};
        if ( FAILED( mQueue->GetTimestampFrequency( &frequency ) ) || !frequency ||
             FAILED( mQueue->GetClockCalibration( &gpuTimestamp, &cpuTimestamp ) ) ) {
            return;
        }

        auto* device{ core::checked_cast<Device*>( mDevice ) };
        constexpr char kName[]{ "D3D12 Graphics" };
        auto* profiler{ tracy::CreateD3D12Context( device->GetDevice(), mQueue.Get() ) };
        mGpuProfiler.reset( profiler, []( tracy::D3D12QueueCtx* context ) { TracyD3D12Destroy( context ); } );
        TracyD3D12ContextName( profiler, kName, sizeof( kName ) - 1 );
    }
#endif

    auto Queue::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        Device* device{ core::checked_cast<Device*>( mDevice ) };

        const D3D12_COMMAND_LIST_TYPE cmdQueueType{ d3d12::GetQueueType( mType ) };

        D3D12_COMMAND_QUEUE_DESC desc{};
        desc.Type = cmdQueueType;
        desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.NodeMask = 0;

        // Command queue
        ThrowIfFailed( device->GetDevice()->CreateCommandQueue( &desc, IID_PPV_ARGS( &mQueue ) ) );

        mFence = device->CreateFence( 0 );
        mFence->SetDebugName( string::Format( "Queue [{}] Timeline", rc_cast<core::u64>( mQueue.Get() ) ) );

        mIsAllocated = true;
    }

    CommandList::CommandList( renderer::rhi::IQueue* queue )
        : mQueueType{ queue->GetType() }, mQueue{ queue } {
        MKT_BEGIN_PROFILER();
        mLabelColor = renderer::rhi::Color(
            as<core::f32>( math::random::GetRandomReal( 0.0f, 1.0f ) ),
            as<core::f32>( math::random::GetRandomReal( 0.0f, 1.0f ) ),
            as<core::f32>( math::random::GetRandomReal( 0.0f, 1.0f ) ),
            0.1f );
    }

    auto CommandList::Open( const renderer::rhi::CommandListBeginDescription& desc ) -> void {
        MKT_BEGIN_PROFILER();

        std::lock_guard lock{ mRecordingMutex };

        if ( mRecording || mReadyForSubmission ) {
            MKT_THROW_RUNTIME_ERROR( "D3D12 recording must be ended and submitted before beginning another recording." );
        }

        mBoundRayTracingPipeline = nullptr;
        mBoundShaderTable = nullptr;

        auto* queue{ core::checked_cast<Queue*>( mQueue ) };
        const auto completed{ queue->GetCurrentTimeline() };
        core::u32 selected{ mRecordingContextIndex };
        bool available{};

        for ( core::u32 offset{}; offset < mRecordingContext.size(); ++offset ) {
            const core::u32 index{ ( mRecordingContextIndex + offset ) % as<core::u32>( mRecordingContext.size() ) };

            if ( mRecordingContext[index].mSubmissionID <= completed ) {
                selected = index;
                available = true;
                break;
            }

            if ( mRecordingContext[index].mSubmissionID < mRecordingContext[selected].mSubmissionID ) {
                selected = index;
            }
        }

        if ( !available ) {
            MKT_PROFILE_NAMED( "D3D12 allocator fence wait" );
            queue->WaitForSubmission( mRecordingContext[selected].mSubmissionID );
        }

        mRecordingContextIndex = selected;
        mCurrentRecordingContext = &mRecordingContext[selected];

        // Release only the context whose submission has actually completed.
        ClearState();

        ThrowIfFailed( mCurrentRecordingContext->mCommandAllocator->Reset() );
        if ( mQueueType == QueueType::eVideoDecode ) {
            ThrowIfFailed( mCurrentRecordingContext->mVideoCommandList->Reset( mCurrentRecordingContext->mCommandAllocator.Get() ) );
        }
        else {
            ThrowIfFailed( mCurrentRecordingContext->mCommandList->Reset( mCurrentRecordingContext->mCommandAllocator.Get(), nullptr ) );
        }

        mRecording = true;
        mRecordingScopeName = desc.mScopeName.empty() ? "UnnamedScope" : desc.mScopeName.c_str();
    }

    auto CommandList::Close() -> void {
        std::lock_guard lock{ mRecordingMutex };

        if ( !mRecording ) {
            MKT_THROW_RUNTIME_ERROR( "D3D12 command list is not recording." );
        }

        if ( mIsRenderScopeActive ) {
            EndRenderPass();
        }

#if defined( MIKOTO_ENABLE_TRACY )
        EndGpuProfileZone();
#endif
        CommitBarriers();

        if ( mQueueType == QueueType::eVideoDecode ) {
            ThrowIfFailed( mCurrentRecordingContext->mVideoCommandList->Close() );
        }
        else {
            ThrowIfFailed( mCurrentRecordingContext->mCommandList->Close() );
        }

        mRecording = false;
        mReadyForSubmission = true;
        mBoundShaderTable = nullptr;
        mBoundRayTracingPipeline = nullptr;
    }

    auto CommandList::AcquireResource( const rhi::BufferBarrierDescription& description, rhi::IQueue* sourceQueue ) -> void {
        // D3D12 has queue types rather than queue-family ownership.
        const auto type{ core::checked_cast<Queue*>( sourceQueue )->GetType() };

        if ( type == mQueueType ) {
            return;
        }

        if ( !mRecording || mIsRenderScopeActive || !description.mBuffer ) {
            MKT_THROW_RUNTIME_ERROR( "Queue handoffs require a resource and a recording command list outside rendering." );
        }

        const auto access{ d3d12::GetBarrierAccess( description.mAccessBefore | description.mAccessAfter ) };

        if ( access & ( D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ |
                          D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE ) ) {
            // Acceleration-structure storage must remain in its immutable native state.
            // Queue signal/wait supplies ordering; it cannot be handed to a copy queue.
            if ( type == rhi::QueueType::eTransfer || mQueueType == rhi::QueueType::eTransfer ) {
                MKT_THROW_INVALID_ARGUMENT( "Acceleration structures cannot be handed to a copy queue." );
            }

            RetainResource( description.mBuffer );
            return;
        }

        auto barrier{ description };
        // Use COMMON between queue types, with the fence wait supplied at submission.
        barrier.mStageBefore = rhi::PipelineStageFlagsBits::None;
        barrier.mAccessBefore = rhi::BarrierAccessFlagsBits::None;
        SetBarrier( barrier );
    }

    auto CommandList::ReleaseResource( const rhi::BufferBarrierDescription& description, rhi::IQueue* destQueue ) -> void {
        // D3D12 has queue types rather than queue-family ownership.
        const auto type{ core::checked_cast<Queue*>( destQueue )->GetType() };

        if ( type == mQueueType ) {
            return;
        }

        if ( !mRecording || mIsRenderScopeActive || !description.mBuffer ) {
            MKT_THROW_RUNTIME_ERROR( "Queue handoffs require a resource and a recording command list outside rendering." );
        }

        const auto access{ d3d12::GetBarrierAccess( description.mAccessBefore | description.mAccessAfter ) };

        if ( access & ( D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ |
                          D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE ) ) {
            // Acceleration-structure storage must remain in its immutable native state.
            // Queue signal/wait supplies ordering; it cannot be handed to a copy queue.
            if ( type == rhi::QueueType::eTransfer || mQueueType == rhi::QueueType::eTransfer ) {
                MKT_THROW_INVALID_ARGUMENT( "Acceleration structures cannot be handed to a copy queue." );
            }

            RetainResource( description.mBuffer );
            return;
        }

        auto barrier{ description };
        // Use COMMON between queue types, with the fence wait supplied at submission.
        barrier.mStageAfter = rhi::PipelineStageFlagsBits::None;
        barrier.mAccessAfter = rhi::BarrierAccessFlagsBits::None;
        SetBarrier( barrier );
    }

    auto CommandList::AcquireResource( const rhi::TextureBarrierDescription& description, rhi::IQueue* sourceQueue ) -> void {
        // D3D12 has queue types rather than queue-family ownership.
        const auto type{ core::checked_cast<Queue*>( sourceQueue )->GetType() };

        if ( type == mQueueType ) {
            return;
        }

        if ( !mRecording || mIsRenderScopeActive || !description.mTexture ) {
            MKT_THROW_RUNTIME_ERROR( "Queue handoffs require a resource and a recording command list outside rendering." );
        }

        if ( description.mLayoutBefore == rhi::TextureLayoutBits::Unknown ||
             description.mLayoutAfter == rhi::TextureLayoutBits::Unknown ) {
            MKT_THROW_INVALID_ARGUMENT( "Texture queue handoffs require explicit before and after layouts." );
        }

        auto barrier{ description };
        // Use COMMON between queue types, with the fence wait supplied at submission.
        barrier.mStageBefore = rhi::PipelineStageFlagsBits::None;
        barrier.mAccessBefore = rhi::BarrierAccessFlagsBits::None;
        barrier.mLayoutBefore = rhi::TextureLayoutBits::General;
        SetBarrier( barrier );
    }

    auto CommandList::ReleaseResource( const rhi::TextureBarrierDescription& description, rhi::IQueue* destQueue ) -> void {
        // D3D12 has queue types rather than queue-family ownership.
        const auto type{ core::checked_cast<Queue*>( destQueue )->GetType() };

        if ( type == mQueueType ) {
            return;
        }

        if ( !mRecording || mIsRenderScopeActive || !description.mTexture ) {
            MKT_THROW_RUNTIME_ERROR( "Queue handoffs require a resource and a recording command list outside rendering." );
        }

        if ( description.mLayoutBefore == rhi::TextureLayoutBits::Unknown ||
             description.mLayoutAfter == rhi::TextureLayoutBits::Unknown ) {
            MKT_THROW_INVALID_ARGUMENT( "Texture queue handoffs require explicit before and after layouts." );
        }

        auto barrier{ description };
        // Use COMMON between queue types, with the fence wait supplied at submission.
        barrier.mStageAfter = rhi::PipelineStageFlagsBits::None;
        barrier.mAccessAfter = rhi::BarrierAccessFlagsBits::None;
        barrier.mLayoutAfter = rhi::TextureLayoutBits::General;
        SetBarrier( barrier );
    }


    auto CommandList::SetDebugName( eastl::string_view name ) -> void {
        MKT_BEGIN_PROFILER();

        if ( name.empty() || !mDevice || !mCurrentRecordingContext ) {
            return;
        }

        mDebugName = name;

        for ( auto& context: mRecordingContext ) {
            if ( context.mVideoCommandList ) {
                context.mVideoCommandList->SetName( string::ToWide( mDebugName ).c_str() );
            }
            else {
                context.mCommandList->SetName( string::ToWide( mDebugName ).c_str() );
            }
            context.mCommandAllocator->SetName( string::ToWide( mDebugName + " allocator" ).c_str() );
        }
    }

    auto CommandList::RecordBarrier( const renderer::rhi::BufferBarrierDescription& desc ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !desc.mBuffer ) {
            return;
        }

        RetainResource( desc.mBuffer );

        auto* buffer{ core::checked_cast<Buffer*>( desc.mBuffer ) };

        if ( buffer->GetHeapType() == renderer::rhi::HeapType::eUpload || buffer->GetHeapType() == renderer::rhi::HeapType::eReadback ) {
            return;
        }

        const auto state{ [this]( D3D12_BARRIER_ACCESS access ) {
            D3D12_RESOURCE_STATES result{ D3D12_RESOURCE_STATE_COMMON };

            if ( access & D3D12_BARRIER_ACCESS_VERTEX_BUFFER ) {
                result |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
            }

            if ( access & D3D12_BARRIER_ACCESS_CONSTANT_BUFFER ) {
                result |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
            }

            if ( access & D3D12_BARRIER_ACCESS_INDEX_BUFFER ) {
                result |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
            }

            if ( access & D3D12_BARRIER_ACCESS_SHADER_RESOURCE ) {
                result |= mQueueType == rhi::QueueType::eCompute ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
            }

            if ( access & D3D12_BARRIER_ACCESS_UNORDERED_ACCESS ) {
                result |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            }

            if ( access & D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT ) {
                result |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
            }

            if ( access & D3D12_BARRIER_ACCESS_COPY_SOURCE ) {
                result |= D3D12_RESOURCE_STATE_COPY_SOURCE;
            }

            if ( access & D3D12_BARRIER_ACCESS_COPY_DEST ) {
                result |= D3D12_RESOURCE_STATE_COPY_DEST;
            }

            if ( access & ( D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ |
                              D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE ) ) {
                result |= D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
            }

            return result;
        } };

        const auto before{ state( d3d12::GetBarrierAccess( desc.mAccessBefore ) ) };
        const auto after{ state( d3d12::GetBarrierAccess( desc.mAccessAfter ) ) };
        D3D12_RESOURCE_BARRIER barrier{};

        if ( before == after ) {
            if ( !( after & ( D3D12_RESOURCE_STATE_UNORDERED_ACCESS | D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE ) ) ) {
                return;
            }

            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            barrier.UAV.pResource = *buffer;
        }
        else {
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = *buffer;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = before;
            barrier.Transition.StateAfter = after;
        }

        mResourceBarriers.push_back( barrier );
    }

    auto CommandList::Build( rhi::IAccelerationStructure *resource ) -> void {
        MKT_BEGIN_PROFILER();

        AccelerationStructure *pAccelStructure{ checked_cast<AccelerationStructure *>( resource ) };

        if ( !pAccelStructure ) {
            MKT_THROW_RUNTIME_ERROR( "Acceleration structure belongs to a different backend or is null." );
        }

        auto *command{ PrepareRayTracing( *pAccelStructure ) };

        if ( pAccelStructure->GetType() == renderer::rhi::AccelStructureType::eTopLevel ) {
            for ( auto &instance: pAccelStructure->mDescription.mInstances ) {
                auto &bottom{ static_cast<AccelerationStructure &>( *instance.mBottomLevel ) };

                if ( !bottom.IsBuilt() ) {
                    Build( &bottom );
                }
            }
        }
        else {
            SetTransition( { pAccelStructure->mDescription.mVertices.GetPtr(), renderer::rhi::ResourceStates::eAccelStructBuildInput } );
            SetTransition( { pAccelStructure->mDescription.mIndices.GetPtr(), renderer::rhi::ResourceStates::eAccelStructBuildInput } );
            PrepareRayTracing( *pAccelStructure );
        }

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        command->ResourceBarrier( 1, &barrier );
        command->BuildRaytracingAccelerationStructure( &pAccelStructure->mBuild, 0, nullptr );
        barrier.UAV.pResource = *core::checked_cast<Buffer *>( pAccelStructure->mResult.GetPtr() );

        command->ResourceBarrier( 1, &barrier );
        pAccelStructure->mBuilt = true;
    }

    auto CommandList::RecordBarrier( const renderer::rhi::TextureBarrierDescription& desc ) -> void {
        MKT_BEGIN_PROFILER();

        RetainResource( desc.mTexture );

        auto* texture{ core::checked_cast<Texture*>( desc.mTexture ) };
        ID3D12Resource* resource{ *texture };
        const auto native{ resource->GetDesc() };
        const auto state{ [this]( renderer::rhi::TextureLayoutFlags layout ) -> D3D12_RESOURCE_STATES {
            if ( layout == TextureLayoutBits::RenderTarget ) {
                return D3D12_RESOURCE_STATE_RENDER_TARGET;
            }

            if ( layout == TextureLayoutBits::DepthStencil ) {
                return D3D12_RESOURCE_STATE_DEPTH_WRITE;
            }

            if ( layout == TextureLayoutBits::DepthStencilReadOnly ) {
                return D3D12_RESOURCE_STATE_DEPTH_READ;
            }

            if ( layout == TextureLayoutBits::ShaderResource ) {
                return mQueueType == rhi::QueueType::eCompute ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
            }

            if ( layout == TextureLayoutBits::UnorderedAccess ) {
                return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            }

            if ( layout == TextureLayoutBits::CopySource ) {
                return D3D12_RESOURCE_STATE_COPY_SOURCE;
            }

            if ( layout == TextureLayoutBits::CopyDest ) {
                return D3D12_RESOURCE_STATE_COPY_DEST;
            }

            if ( layout == TextureLayoutBits::ResolveSource ) {
                return D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
            }

            if ( layout == TextureLayoutBits::ResolveDest ) {
                return D3D12_RESOURCE_STATE_RESOLVE_DEST;
            }

            if ( layout == TextureLayoutBits::VideoDecodeOutput ) {
                return D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE;
            }

            if ( layout == TextureLayoutBits::VideoDecodeReference ) {
                return D3D12_RESOURCE_STATE_VIDEO_DECODE_READ;
            }

            return D3D12_RESOURCE_STATE_COMMON;
        } };
        // Texture transitions use one barrier model throughout this backend.
        // Uploads, automatic transitions and DXR already use ResourceBarrier;
        // mixing enhanced layouts into that stream requires COMMON handoffs.
        const auto before{ state( desc.mLayoutBefore ) };
        const auto after{ state( desc.mLayoutAfter ) };

        if ( before == after ) {
            if ( after == D3D12_RESOURCE_STATE_UNORDERED_ACCESS ) {
                D3D12_RESOURCE_BARRIER barrier{};
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                barrier.UAV.pResource = resource;
                mResourceBarriers.push_back( barrier );
            }

            return;
        }

        const auto& range{ desc.mSubresourceSet };
        const UINT layers{ native.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? 1u : native.DepthOrArraySize };

        if ( range.mBaseMipLevel >= native.MipLevels || range.mBaseArraySlice >= layers ) {
            MKT_THROW_RUNTIME_ERROR( "D3D12 texture barrier starts outside the texture." );
        }

        const UINT mips{ glm::min( range.mNumMipLevels, UINT( native.MipLevels - range.mBaseMipLevel ) ) };
        const UINT slices{ glm::min( range.mNumArraySlices, layers - range.mBaseArraySlice ) };
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = resource;
        barrier.Transition.StateBefore = before;
        barrier.Transition.StateAfter = after;

        if ( range.mBaseMipLevel == 0 && range.mBaseArraySlice == 0 && mips == native.MipLevels && slices == layers ) {
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            mResourceBarriers.push_back( barrier );
        }
        else {
            // The RHI range covers every aspect, including the separate stencil plane.
            const UINT planes{ rhi::GetFormatInfo( texture->GetFormat() ).mHasStencil ? 2u : 1u };

            for ( UINT plane{}; plane < planes; ++plane ) {
                for ( UINT slice{}; slice < slices; ++slice ) {
                    for ( UINT mip{}; mip < mips; ++mip ) {
                        barrier.Transition.Subresource = range.mBaseMipLevel + mip +
                                                         ( range.mBaseArraySlice + slice + plane * layers ) * native.MipLevels;
                        mResourceBarriers.push_back( barrier );
                    }
                }
            }
        }
    }

    auto CommandList::SetTransition( const renderer::rhi::TransitionDescription& description ) -> void {
        MKT_BEGIN_PROFILER();

        for ( const auto& transition: description.mBuffers ) {
            SetTransition( transition.mBuffer, transition.mState );
        }

        for ( const auto& transition: description.mTextures ) {
            SetTransition( transition.mTexture, transition.mState );
        }
    }

    auto CommandList::SetBarrier( const renderer::rhi::BarrierDescription& description ) -> void {
        MKT_BEGIN_PROFILER();

        for ( const renderer::rhi::BufferBarrierDescription& barrier: description.mBuffers ) {
            RecordBarrier( barrier );
        }

        for ( const renderer::rhi::TextureBarrierDescription& barrier: description.mTextures ) {
            RecordBarrier( barrier );
        }

        CommitBarriers();
    }

    Fence::Fence( core::u64 initialValue ) {
        MKT_BEGIN_PROFILER();
        mFenceInitialValue = initialValue;
    }


    auto CommandList::SetBarrier( const renderer::rhi::BufferBarrierDescription& desc ) -> void {
        if ( desc.mBuffer ) {
            RecordBarrier( desc );
            CommitBarriers();
        }
    }

    auto CommandList::SetBarrier( const renderer::rhi::TextureBarrierDescription& desc ) -> void {
        RecordBarrier( desc );
        CommitBarriers();
    }

    auto CommandList::RecordTransition( renderer::rhi::IBuffer* buffer, renderer::rhi::ResourceStates stateBits ) -> void {
        MKT_BEGIN_PROFILER();

        RetainResource( buffer );

        const auto heap{ core::checked_cast<Buffer*>( buffer )->GetHeapType() };

        if ( heap == renderer::rhi::HeapType::eUpload || heap == renderer::rhi::HeapType::eReadback ) {
            return;
        }
        // ID3D12CommandList::ResourceBarrier: Before and after states must be different.
        if ( d3d12::GetResourceState( buffer->GetResourceState() ) == d3d12::GetResourceState( stateBits ) ) {
            return;
        }

        Buffer* d3d12Buffer{ core::checked_cast<Buffer*>( buffer ) };
        ID3D12Resource* resource{ *d3d12Buffer };

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = resource;
        barrier.Transition.StateBefore = d3d12::GetResourceState( buffer->GetResourceState() );
        barrier.Transition.StateAfter = d3d12::GetResourceState( stateBits );
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        mResourceBarriers.emplace_back( barrier );

        if ( stateBits != renderer::rhi::ResourceStates::eUnknown ) {
            buffer->SetResourceState( stateBits );
        }
    }

    auto CommandList::RecordTransition( renderer::rhi::ITexture* texture, renderer::rhi::ResourceStates stateBits ) -> void {
        RetainResource( texture );
        MKT_BEGIN_PROFILER();
        // ID3D12CommandList::ResourceBarrier: Before and after states must be different.
        if ( d3d12::GetResourceState( texture->GetResourceState() ) == d3d12::GetResourceState( stateBits ) ) {
            return;
        }

        Texture* d3d12Texture{ core::checked_cast<Texture*>( texture ) };
        ID3D12Resource* resource{ *d3d12Texture };

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = resource;
        barrier.Transition.StateBefore = d3d12::GetResourceState( texture->GetResourceState() );
        barrier.Transition.StateAfter = d3d12::GetResourceState( stateBits );
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        mResourceBarriers.emplace_back( barrier );

        if ( stateBits != renderer::rhi::ResourceStates::eUnknown ) {
            texture->SetResourceState( stateBits );
        }
    }

    auto CommandList::SetTransition( renderer::rhi::IBuffer* buffer, renderer::rhi::ResourceStates stateBits ) -> void {
        RecordTransition( buffer, stateBits );
        CommitBarriers();
    }

    auto CommandList::SetTransition( renderer::rhi::ITexture* texture, renderer::rhi::ResourceStates stateBits ) -> void {
        RecordTransition( texture, stateBits );
        CommitBarriers();
    }

    auto CommandList::DecodeVideo( const VideoDecodeDescription& description ) -> void {
        if ( !mRecording || mIsRenderScopeActive || mQueueType != QueueType::eVideoDecode ) {
            MKT_THROW_RUNTIME_ERROR( "Video decoding requires a recording video command list outside rendering." );
        }

        auto decoderOwner{ description.mDecoder };
        auto* decoder{ core::checked_cast<VideoDecoder*>( decoderOwner.GetPtr() ) };

        if ( !decoder || !description.mPicture || description.mPicture->GetSlices().empty() ||
            !description.mOutput || !description.mReconstruction.mSurface ||
            !decoder->MatchesConfiguration( *description.mPicture ) ) {
            MKT_THROW_RUNTIME_ERROR( "Video decode operation has missing resources or changed H.264 parameters." );
        }

        auto bitstream{ decoder->CreateBitstream( *description.mPicture ) };
        auto outputImage{ description.mOutput->GetImage() };
        auto reconstructionImage{ description.mReconstruction.mSurface->GetImage() };
        RetainResource( decoder );
        RetainResource( bitstream.GetPtr() );
        RetainResource( description.mOutput.GetPtr() );
        RetainResource( description.mReconstruction.mSurface.GetPtr() );

        // Picture indices address this native table, with the current reconstruction last.
        // The scheduler owns queue handoffs; these transitions only assign decode read/write roles.
        eastl::array<ID3D12Resource*, 17> references{};
        eastl::array<UINT, 17> subresources{};
        eastl::array<ID3D12VideoDecoderHeap*, 17> heaps{};
        const auto count{ description.mReferences.size() };

        for ( usize index{}; index < count; ++index ) {
            const auto& reference{ description.mReferences[index] };
            auto image{ reference.mSurface->GetImage() };
            RetainResource( reference.mSurface.GetPtr() );
            RecordTransition( image.GetPtr(), ResourceStates::eVideoDecodeReference );
            references[index] = *core::checked_cast<Texture*>( image.GetPtr() );
            heaps[index] = decoder->GetHeap();
        }

        references[count] = *core::checked_cast<Texture*>( reconstructionImage.GetPtr() );
        heaps[count] = decoder->GetHeap();
        RecordTransition( reconstructionImage.GetPtr(), ResourceStates::eVideoDecodeOutput );

        if ( !decoder->UsesCoincidentOutput() ) {
            RecordTransition( outputImage.GetPtr(), ResourceStates::eVideoDecodeOutput );
        }

        // D3D12 consumes DXVA picture, quantization and short-slice arguments when recording.
        // https://learn.microsoft.com/en-us/windows/win32/api/d3d12video/ns-d3d12video-d3d12_video_decode_input_stream_arguments
        // https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/wiGraphicsDevice_DX12.cpp (VideoDecode)
        auto parameters{ MakeVideoPictureParameters( description, decoder->NextFeedbackNumber() ) };
        DXVA_Qmatrix_H264 quantization{};
        h264::resolve_scaling_lists( &description.mPicture->GetSps(), &description.mPicture->GetPps(),
            quantization.bScalingLists4x4, quantization.bScalingLists8x8 );
        const auto offsets{ description.mPicture->GetSliceOffsets() };
        eastl::vector<DXVA_Slice_H264_Short> slices( offsets.size() );

        for ( usize index{}; index < offsets.size(); ++index ) {
            const auto start{ offsets[index] - static_cast<u32>( index ) };
            const auto end{ index + 1 < offsets.size() ? offsets[index + 1] - static_cast<u32>( index + 1 ) : static_cast<u32>( bitstream->GetSizeBytes() ) };
            slices[index].BSNALunitDataLocation = start;
            slices[index].SliceBytesInBuffer = end - start;
        }

        D3D12_VIDEO_DECODE_INPUT_STREAM_ARGUMENTS input{};
        input.pHeap = decoder->GetHeap();
        input.CompressedBitstream.pBuffer = *core::checked_cast<Buffer*>( bitstream.GetPtr() );
        input.CompressedBitstream.Size = bitstream->GetSizeBytes();
        input.ReferenceFrames.NumTexture2Ds = static_cast<UINT>( count + 1 );
        input.ReferenceFrames.ppTexture2Ds = references.data();
        input.ReferenceFrames.pSubresources = subresources.data();
        input.ReferenceFrames.ppHeaps = heaps.data();
        input.NumFrameArguments = 3;
        input.FrameArguments[0] = { D3D12_VIDEO_DECODE_ARGUMENT_TYPE_PICTURE_PARAMETERS, sizeof( parameters ), &parameters };
        input.FrameArguments[1] = { D3D12_VIDEO_DECODE_ARGUMENT_TYPE_INVERSE_QUANTIZATION_MATRIX, sizeof( quantization ), &quantization };
        input.FrameArguments[2] = { D3D12_VIDEO_DECODE_ARGUMENT_TYPE_SLICE_CONTROL,
            static_cast<UINT>( slices.size() * sizeof( DXVA_Slice_H264_Short ) ), slices.data() };

        D3D12_VIDEO_DECODE_OUTPUT_STREAM_ARGUMENTS output{};
        output.pOutputTexture2D = *core::checked_cast<Texture*>( outputImage.GetPtr() );

        if ( !decoder->UsesCoincidentOutput() ) {
            // Reference-only allocations require a separate NV12 display output, without color conversion.
            output.ConversionArguments.Enable = TRUE;
            output.ConversionArguments.pReferenceTexture2D = references[count];
            output.ConversionArguments.OutputColorSpace = decoder->GetColorSpace();
            output.ConversionArguments.DecodeColorSpace = decoder->GetColorSpace();
        }

        CommitBarriers();
        mCurrentRecordingContext->mVideoCommandList->DecodeFrame( decoder->GetDecoder(), &output, &input );
    }

    auto CommandList::CommitBarriers() -> void {
        MKT_BEGIN_PROFILER();
        eastl::array<D3D12_BARRIER_GROUP, 2> groups{};
        UINT numGroups{ 0 };

        if ( !mBufferBarriers.empty() ) {
            auto& group{ groups[numGroups++] };
            group.Type = D3D12_BARRIER_TYPE_BUFFER;
            group.NumBarriers = core::as<UINT>( mBufferBarriers.size() );
            group.pBufferBarriers = mBufferBarriers.data();
        }

        if ( !mTextureBarriers.empty() ) {
            auto& group{ groups[numGroups++] };
            group.Type = D3D12_BARRIER_TYPE_TEXTURE;
            group.NumBarriers = core::as<UINT>( mTextureBarriers.size() );
            group.pTextureBarriers = mTextureBarriers.data();
        }

        if ( numGroups > 0 ) {
            mCurrentRecordingContext->mCommandList->Barrier( numGroups, groups.data() );
        }

        if ( !mResourceBarriers.empty() ) {
            if ( mQueueType == QueueType::eVideoDecode ) {
                mCurrentRecordingContext->mVideoCommandList->ResourceBarrier( core::as<UINT>( mResourceBarriers.size() ), mResourceBarriers.data() );
            }
            else {
                mCurrentRecordingContext->mCommandList->ResourceBarrier( core::as<UINT>( mResourceBarriers.size() ), mResourceBarriers.data() );
            }
        }

        mBufferBarriers.clear();
        mTextureBarriers.clear();

        mResourceBarriers.clear();
    }

    auto CommandList::SetEnableAutomaticBarriers( bool enable ) -> void {
        MKT_BEGIN_PROFILER();
        mEnableAutomaticBarriers = enable;
    }

    auto CommandList::SetClearColor( renderer::rhi::ITexture* renderTarget, renderer::rhi::Color color ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !renderTarget ) {
            return;
        }

        RetainResource( renderTarget );

        if ( mEnableAutomaticBarriers ) {
            SetTransition( renderTarget, renderer::rhi::ResourceStates::eRenderTarget );
        }

        Device* device{ core::checked_cast<Device*>( mDevice ) };
        Texture* texture{ core::checked_cast<Texture*>( renderTarget ) };

        const DeviceResources* deviceResources{ device->GetHeapResources() };
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandler{
            deviceResources->mRenderTargetViewHeap->GetCpuHandle( texture->GetRtvDescriptorIndex() )
        };

        const float clearColor[]{ color.mR, color.mG, color.mB, color.mA };
        mCurrentRecordingContext->mCommandList->ClearRenderTargetView( rtvHandler, clearColor, 0, nullptr );
    }

    auto CommandList::Write( renderer::rhi::IBuffer* src, renderer::rhi::ITexture* dest ) -> void {
    }

    auto CommandList::Write( renderer::rhi::ITexture* texture, const void* data, core::usize byteSize ) -> void {
        if ( !texture || !data || texture->GetSampleCount() != renderer::rhi::Multisampling::eMsaaX1 ) {
            MKT_THROW_RUNTIME_ERROR( "D3D12 texture upload requires data and a single-sample destination." );
        }

        RetainResource( texture );
        MKT_BEGIN_PROFILER();

        if ( mIsRenderScopeActive ) {
            EndRenderPass();
        }

        if ( mEnableAutomaticBarriers ) {
            SetTransition( texture, renderer::rhi::ResourceStates::eCopyDest );
        }

        Texture* pTexture{ core::checked_cast<Texture*>( texture ) };
        ID3D12Resource* pDestResource{ *pTexture };
        const auto description{ pDestResource->GetDesc() };
        const UINT layers{ description.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? 1u : description.DepthOrArraySize };
        core::usize sourceOffset{};

        for ( UINT layer{}; layer < layers; ++layer ) {
            const UINT subresource{ layer * description.MipLevels };
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
            UINT rows{};
            UINT64 rowSize{}, uploadSize{};
            core::checked_cast<Device*>( mDevice )->GetDevice()->GetCopyableFootprints(
                &description, subresource, 1, 0, &footprint, &rows, &rowSize, &uploadSize );
            const UINT64 packedSize{ rowSize * rows * footprint.Footprint.Depth };

            if ( uploadSize == UINT64_MAX || !uploadSize || sourceOffset > byteSize || packedSize > byteSize - sourceOffset ) {
                MKT_THROW_RUNTIME_ERROR( "D3D12 texture upload data is smaller than its base-level slices." );
            }

            auto* allocation{ mUploadManager->SubAllocate( uploadSize, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT ) };
            mCurrentRecordingContext->mInFlightSubAllocations.emplace_back( allocation );

            for ( UINT z{}; z < footprint.Footprint.Depth; ++z ) {
                for ( UINT y{}; y < rows; ++y ) {
                    const core::usize row{ static_cast<core::usize>( z ) * rows + y };
                    std::memcpy( core::as<core::u8*>( allocation->mMappedMemory ) + row * footprint.Footprint.RowPitch,
                        core::as<const core::u8*>( data ) + sourceOffset + row * rowSize, rowSize );
                }
            }

            D3D12_TEXTURE_COPY_LOCATION source{};
            source.pResource = *core::checked_cast<Buffer*>( allocation->mBuffer );
            source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            source.PlacedFootprint = footprint;
            source.PlacedFootprint.Offset += allocation->mOffset;
            D3D12_TEXTURE_COPY_LOCATION destination{};
            destination.pResource = pDestResource;
            destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            destination.SubresourceIndex = subresource;

            // A full-subresource copy initializes the destination. Discard is invalid
            // in COPY_DEST and would also invalidate previously uploaded array slices.
            mCurrentRecordingContext->mCommandList->CopyTextureRegion( &destination, 0, 0, 0, &source, nullptr );
            sourceOffset += packedSize;
        }
    }

    auto CommandList::Copy( renderer::rhi::ITexture* src, const renderer::rhi::TextureSlice& srcSlice, renderer::rhi::ITexture* dest, const renderer::rhi::TextureSlice& destSlice ) -> void {
        RetainResource( src );
        RetainResource( dest );
        MKT_BEGIN_PROFILER();
        const auto srcTexture{ core::checked_cast<Texture*>( src ) };
        const auto dstTexture{ core::checked_cast<Texture*>( dest ) };

        MKT_ASSERT( srcTexture != nullptr, "Source D3D12 texture cannot be null" );
        MKT_ASSERT( dstTexture != nullptr, "Destination D3D12 texture cannot be null" );

        ID3D12Resource* srcResource{ *srcTexture };
        ID3D12Resource* dstResource{ *dstTexture };

        MKT_ASSERT( srcResource != nullptr, "Source D3D12 resource is null" );
        MKT_ASSERT( dstResource != nullptr, "Destination D3D12 resource is null" );

        if ( mIsRenderScopeActive ) {
            EndRenderPass();
        }

        if ( mEnableAutomaticBarriers ) {
            SetTransition( srcTexture, renderer::rhi::ResourceStates::eCopySource );
            SetTransition( dstTexture, renderer::rhi::ResourceStates::eCopyDest );
        }

        // Source
        renderer::rhi::TextureSubresourceSet srcSubresourceSet{ srcSlice.mMipLevel, 1, srcSlice.mArrayLayer, 1 };
        D3D12_TEXTURE_COPY_LOCATION srcLocation{};
        srcLocation.pResource = srcResource;
        srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        srcLocation.SubresourceIndex =
            D3D12CalcSubresource(
                srcSubresourceSet.mBaseMipLevel,
                srcSubresourceSet.mBaseArraySlice,
                0,
                srcTexture->GetMipLevelCount(),
                srcSubresourceSet.mNumArraySlices );

        // Destination
        renderer::rhi::TextureSubresourceSet destSubresourceSet{ destSlice.mMipLevel, 1, destSlice.mArrayLayer, 1 };
        D3D12_TEXTURE_COPY_LOCATION dstLocation{};
        dstLocation.pResource = dstResource;
        dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLocation.SubresourceIndex =
            D3D12CalcSubresource(
                destSubresourceSet.mBaseMipLevel,
                destSubresourceSet.mBaseArraySlice,
                0,
                dstTexture->GetMipLevelCount(),
                destSubresourceSet.mNumArraySlices );

        const D3D12_BOX sourceBox{
            srcSlice.x,
            srcSlice.y,
            srcSlice.z,
            srcSlice.x + srcSlice.mWidth,
            srcSlice.y + srcSlice.mHeight,
            srcSlice.z + srcSlice.mDepth
        };

        const bool fullSource{
            srcSlice.x == 0 && srcSlice.y == 0 && srcSlice.z == 0 &&
            srcSlice.mWidth == glm::max( 1u, src->GetWidth() >> srcSlice.mMipLevel ) &&
            srcSlice.mHeight == glm::max( 1u, src->GetHeight() >> srcSlice.mMipLevel ) &&
            srcSlice.mDepth == ( srcResource->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D
                                       ? glm::max( 1u, core::u32( srcResource->GetDesc().DepthOrArraySize ) >> srcSlice.mMipLevel )
                                       : 1u )
        };

        mCurrentRecordingContext->mCommandList->CopyTextureRegion(
            &dstLocation,
            destSlice.x,
            destSlice.y,
            destSlice.z,
            &srcLocation,
            fullSource ? nullptr : &sourceBox );
    }

    auto CommandList::Resolve( renderer::rhi::ITexture* src, const renderer::rhi::TextureSlice& srcSlice, renderer::rhi::ITexture* dest, const renderer::rhi::TextureSlice& destSlice ) -> void {
        if ( !src || !dest || src->GetSampleCount() == renderer::rhi::Multisampling::eMsaaX1 ||
             dest->GetSampleCount() != renderer::rhi::Multisampling::eMsaaX1 || src->GetFormat() != dest->GetFormat() ||
             src->GetWidth() != dest->GetWidth() || src->GetHeight() != dest->GetHeight() ||
             srcSlice.x || srcSlice.y || srcSlice.z || destSlice.x || destSlice.y || destSlice.z ||
             srcSlice.mMipLevel || destSlice.mMipLevel || srcSlice.mWidth != src->GetWidth() ||
             srcSlice.mHeight != src->GetHeight() || destSlice.mWidth != dest->GetWidth() ||
             destSlice.mHeight != dest->GetHeight() ||
             rhi::GetFormatInfo( src->GetFormat() ).mHasDepth || rhi::GetFormatInfo( src->GetFormat() ).mHasStencil ) {
            MKT_THROW_RUNTIME_ERROR( "D3D12 resolve requires matching full-size color subresources and a single-sample destination." );
        }

        RetainResource( src );
        RetainResource( dest );

        if ( mIsRenderScopeActive ) {
            EndRenderPass();
        }

        if ( mEnableAutomaticBarriers ) {
            SetTransition( src, renderer::rhi::ResourceStates::eResolveSource );
            SetTransition( dest, renderer::rhi::ResourceStates::eResolveDest );
        }

        ID3D12Resource* source{ *core::checked_cast<Texture*>( src ) };
        ID3D12Resource* target{ *core::checked_cast<Texture*>( dest ) };

        if ( srcSlice.mArrayLayer >= source->GetDesc().DepthOrArraySize || destSlice.mArrayLayer >= target->GetDesc().DepthOrArraySize ) {
            MKT_THROW_RUNTIME_ERROR( "D3D12 resolve array slice is outside the texture." );
        }

        const core::u32 destSubresource{ destSlice.mArrayLayer * dest->GetMipLevelCount() };
        const core::u32 srcSubresource{ srcSlice.mArrayLayer * src->GetMipLevelCount() };

        if ( target->GetDesc().Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET ) {
            // Placed render targets need clear/copy/discard initialization before a resolve.
            // The full resolve replaces these contents, so discard is safe on every recording.
            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = target;
            barrier.Transition.Subresource = destSubresource;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RESOLVE_DEST;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            mCurrentRecordingContext->mCommandList->ResourceBarrier( 1, &barrier );

            const D3D12_DISCARD_REGION region{ 0, nullptr, destSubresource, 1 };
            mCurrentRecordingContext->mCommandList->DiscardResource( target, &region );

            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RESOLVE_DEST;
            mCurrentRecordingContext->mCommandList->ResourceBarrier( 1, &barrier );
        }

        mCurrentRecordingContext->mCommandList->ResolveSubresource(
            target, destSubresource, source, srcSubresource, d3d12::GetFormat( src->GetFormat() ) );
    }

    auto CommandList::Write( renderer::rhi::IBuffer* buffer, core::usize destOffset, const void* data, core::usize byteSize ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !buffer || buffer->GetOwningDevice() != mDevice || destOffset > buffer->GetSizeBytes() ||
             byteSize > buffer->GetSizeBytes() - destOffset || ( byteSize && !data ) ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid D3D12 buffer write destination or range." );
        }

        if ( byteSize == 0 ) {
            return;
        }

        auto* destination{ core::checked_cast<Buffer*>( buffer ) };

        // Upload heaps are CPU-writable. This path also works before Open().
        if ( destination->GetHeapType() == renderer::rhi::HeapType::eUpload ) {
            const bool mapHere{ !destination->IsMapped() };
            auto* mapped{ static_cast<core::u8*>( mapHere ? destination->PersistentMap() : destination->GetMappedAddress() ) };
            std::memcpy( mapped + destOffset, data, byteSize );

            if ( mapHere ) {
                destination->PersistentUnmap();
            }
        }
        else if ( destination->GetHeapType() == renderer::rhi::HeapType::eDeviceLocal ) {
            RetainResource( buffer );

            if ( mEnableAutomaticBarriers ) {
                SetTransition( buffer, renderer::rhi::ResourceStates::eCopyDest );
            }

            auto* allocation{ mUploadManager->SubAllocate( byteSize ) };
            mCurrentRecordingContext->mInFlightSubAllocations.emplace_back( allocation );
            std::memcpy( allocation->mMappedMemory, data, byteSize );
            ID3D12Resource* source{ *core::checked_cast<Buffer*>( allocation->mBuffer ) };
            ID3D12Resource* target{ *destination };
            mCurrentRecordingContext->mCommandList->CopyBufferRegion( target, destOffset, source, allocation->mOffset, byteSize );
        }
        else {
            MKT_THROW_RUNTIME_ERROR( "D3D12 readback buffers cannot receive CPU uploads." );
        }
    }

    auto CommandList::Write( renderer::rhi::IBuffer* buffer, const void* data, core::usize byteSize ) -> void {
        Write( buffer, 0, data, byteSize );
    }

    auto CommandList::Copy( renderer::rhi::IBuffer* src, renderer::rhi::IBuffer* dest ) -> void {
        Copy( src, dest, 0 );
    }

    auto CommandList::Copy( renderer::rhi::IBuffer* src, renderer::rhi::IBuffer* dest, core::usize destOffset ) -> void {
        if ( !src || !dest || src == dest || destOffset > dest->GetSizeBytes() || src->GetSizeBytes() > dest->GetSizeBytes() - destOffset ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid D3D12 buffer copy range." );
        }

        RetainResource( src );
        RetainResource( dest );

        if ( mEnableAutomaticBarriers ) {
            SetTransition( src, renderer::rhi::ResourceStates::eCopySource );
            SetTransition( dest, renderer::rhi::ResourceStates::eCopyDest );
        }

        ID3D12Resource* source{ *core::checked_cast<Buffer*>( src ) };
        ID3D12Resource* target{ *core::checked_cast<Buffer*>( dest ) };
        mCurrentRecordingContext->mCommandList->CopyBufferRegion( target, destOffset, source, 0, src->GetSizeBytes() );
    }

    auto CommandList::Copy( renderer::rhi::IBuffer* dest, renderer::rhi::ITexture* src ) -> void {
        if ( !src ) {
            MKT_THROW_RUNTIME_ERROR( "D3D12 texture readback requires a source texture." );
        }

        Copy( dest, src, renderer::rhi::TextureSlice{ .mWidth = src->GetWidth(), .mHeight = src->GetHeight() } );
    }

    auto CommandList::Copy( renderer::rhi::IBuffer* dest, renderer::rhi::ITexture* src, const renderer::rhi::TextureSlice& srcSlice ) -> void {
        if ( !src || !dest || src->GetSampleCount() != renderer::rhi::Multisampling::eMsaaX1 ||
             core::checked_cast<Buffer*>( dest )->GetHeapType() == renderer::rhi::HeapType::eUpload ) {
            MKT_THROW_RUNTIME_ERROR( "D3D12 texture readback requires a single-sample source and a GPU-writeable buffer." );
        }

        ID3D12Resource* source{ *core::checked_cast<Texture*>( src ) };
        auto description{ source->GetDesc() };
        const auto& format{ rhi::GetFormatInfo( src->GetFormat() ) };

        if ( description.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || format.mBlockSize != 1 ||
             format.mHasDepth || format.mHasStencil || srcSlice.mMipLevel >= description.MipLevels ||
             srcSlice.mArrayLayer >= description.DepthOrArraySize || srcSlice.z || srcSlice.mDepth != 1 ) {
            MKT_THROW_RUNTIME_ERROR( "D3D12 packed texture readback requires an uncompressed 2D color subresource." );
        }

        const core::u64 width{ eastl::max<core::u64>( 1, description.Width >> srcSlice.mMipLevel ) };
        const core::u32 height{ eastl::max<core::u32>( 1, description.Height >> srcSlice.mMipLevel ) };

        if ( !srcSlice.mWidth || !srcSlice.mHeight || srcSlice.x > width || srcSlice.mWidth > width - srcSlice.x ||
             srcSlice.y > height || srcSlice.mHeight > height - srcSlice.y ) {
            MKT_THROW_RUNTIME_ERROR( "D3D12 texture readback region is outside the source subresource." );
        }

        const UINT subresource{ D3D12CalcSubresource( srcSlice.mMipLevel, srcSlice.mArrayLayer, 0,
            description.MipLevels, description.DepthOrArraySize ) };
        description.Width = srcSlice.mWidth;
        description.Height = srcSlice.mHeight;
        description.MipLevels = 1;
        description.DepthOrArraySize = 1;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        UINT rows{};
        UINT64 rowBytes{}, totalBytes{};
        core::checked_cast<Device*>( mDevice )->GetDevice()->GetCopyableFootprints(
            &description, 0, 1, 0, &footprint, &rows, &rowBytes, &totalBytes );

        if ( totalBytes == UINT64_MAX || !totalBytes || rowBytes * rows > dest->GetSizeBytes() ) {
            MKT_THROW_RUNTIME_ERROR( "D3D12 texture readback destination is too small." );
        }

        RetainResource( src );
        RetainResource( dest );

        if ( mIsRenderScopeActive ) {
            EndRenderPass();
        }

        if ( mEnableAutomaticBarriers ) {
            SetTransition( src, renderer::rhi::ResourceStates::eCopySource );
            SetTransition( dest, renderer::rhi::ResourceStates::eCopyDest );
        }

        // The RHI exposes tightly packed rows. D3D12 texture copies require
        // aligned rows, so remove padding on the GPU before the readback fence.
        renderer::rhi::BufferHandle staging{};
        ID3D12Resource* destination{ *core::checked_cast<Buffer*>( dest ) };
        ID3D12Resource* copyTarget{ destination };

        if ( rowBytes != footprint.Footprint.RowPitch ) {
            staging = mDevice->CreateBuffer( renderer::rhi::BufferCreateDescription{}
                .SetName( "D3D12_TextureReadbackPacking" )
                .SetByteSize( totalBytes )
                .SetResourceType( renderer::rhi::ResourceType::eRawBuffer_SRV )
                .SetBufferUsage( renderer::rhi::BufferUsageFlagsBits::CopySource | renderer::rhi::BufferUsageFlagsBits::CopyDest )
                .SetInitialState( renderer::rhi::ResourceStates::eCopyDest ) );
            RetainResource( staging.GetPtr() );
            copyTarget = *core::checked_cast<Buffer*>( staging.GetPtr() );
        }

        D3D12_TEXTURE_COPY_LOCATION from{};
        from.pResource = source;
        from.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        from.SubresourceIndex = subresource;
        D3D12_TEXTURE_COPY_LOCATION to{};
        to.pResource = copyTarget;
        to.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        to.PlacedFootprint = footprint;
        const D3D12_BOX box{ srcSlice.x, srcSlice.y, 0,
            srcSlice.x + srcSlice.mWidth, srcSlice.y + srcSlice.mHeight, 1 };
        mCurrentRecordingContext->mCommandList->CopyTextureRegion( &to, 0, 0, 0, &from, &box );

        if ( staging ) {
            SetTransition( staging.GetPtr(), renderer::rhi::ResourceStates::eCopySource );

            for ( UINT row{}; row < rows; ++row ) {
                mCurrentRecordingContext->mCommandList->CopyBufferRegion( destination, row * rowBytes,
                    copyTarget, static_cast<core::u64>( row ) * footprint.Footprint.RowPitch, rowBytes );
            }
        }
    }

    auto CommandList::BeginRenderPass( renderer::rhi::RenderPassDescription& state ) -> void {
        MKT_BEGIN_PROFILER();

        // Keep ref to render targets until this cmd is done in the GPU
        for ( const auto& target: state.mCurrentRenderTargets ) {
            RetainResource( target.mRenderTarget.GetPtr() );
        }

        // Keep ref to depth targets
        RetainResource( state.mDepthTarget.mRenderTarget.GetPtr() );

        mIsRenderScopeActive = true;

        Device* device{ core::checked_cast<Device*>( mDevice ) };

        const DeviceResources* deviceResources{ device->GetHeapResources() };
        eastl::fixed_vector<D3D12_CPU_DESCRIPTOR_HANDLE, kMaxRenderTargets> rtvHandles{};

        // Match Vulkan render-area clears; atlas tiles must preserve their neighbours.
        const D3D12_RECT clearRect{ state.mRenderArea.mMinX, state.mRenderArea.mMinY,
            state.mRenderArea.mMaxX, state.mRenderArea.mMaxY };
        const bool hasClearRect{ clearRect.right > clearRect.left && clearRect.bottom > clearRect.top };
        const auto isPartialClear = [&]( const Texture* texture, core::u32 mip ) {
            const auto width{ glm::max( 1u, texture->GetWidth() >> mip ) };
            const auto height{ glm::max( 1u, texture->GetHeight() >> mip ) };
            return hasClearRect && ( clearRect.left > 0 || clearRect.top > 0 ||
                                       clearRect.right < core::as<LONG>( width ) || clearRect.bottom < core::as<LONG>( height ) );
        };

        for ( auto& renderTarget: state.mCurrentRenderTargets ) {
            if ( mEnableAutomaticBarriers ) {
                SetTransition( renderTarget.mRenderTarget.GetPtr(), renderer::rhi::ResourceStates::eRenderTarget );
            }

            Texture* pColorTexture{ core::checked_cast<Texture*>( renderTarget.mRenderTarget.GetPtr() ) };
            D3D12_CPU_DESCRIPTOR_HANDLE rtvHandler{
                deviceResources->mRenderTargetViewHeap->GetCpuHandle( pColorTexture->GetRtvDescriptorIndex( renderTarget.mSubresourceSet ) )
            };

            if ( renderTarget.mLoadOp == renderer::rhi::LoadOp::eClear || renderTarget.mLoadOp == renderer::rhi::LoadOp::eDontCare ) {
                const float clearColor[]{
                    renderTarget.mClearColor.mR,
                    renderTarget.mClearColor.mG,
                    renderTarget.mClearColor.mB,
                    renderTarget.mClearColor.mA
                };

                // Rectangular clears do not count as first-use initialization, even
                // when the rectangle covers the view. Keep rects only for partial clears.
                const bool partial{ isPartialClear( pColorTexture, renderTarget.mSubresourceSet.mBaseMipLevel ) };
                mCurrentRecordingContext->mCommandList->ClearRenderTargetView( rtvHandler, clearColor,
                    partial ? 1u : 0u, partial ? &clearRect : nullptr );
            }

            rtvHandles.emplace_back( rtvHandler );
        }

        // Depth stencil. Provide it if available
        Texture* pDepthTexture{ core::checked_cast<Texture*>( state.mDepthTarget.mRenderTarget.GetPtr() ) };
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandler{ pDepthTexture ? deviceResources->mDepthStencilViewHeap->GetCpuHandle( pDepthTexture->GetDsvDescriptorIndex( state.mDepthTarget.mSubresourceSet ) ) : D3D12_CPU_DESCRIPTOR_HANDLE{} };

        if ( pDepthTexture ) {
            if ( mEnableAutomaticBarriers ) {
                // Loading existing depth does not make the attachment read-only.
                SetTransition( pDepthTexture, renderer::rhi::ResourceStates::eDepthWrite );
            }

            if ( state.mDepthTarget.mLoadOp == renderer::rhi::LoadOp::eClear || state.mDepthTarget.mLoadOp == renderer::rhi::LoadOp::eDontCare ) {
                const bool partial{ isPartialClear( pDepthTexture, state.mDepthTarget.mSubresourceSet.mBaseMipLevel ) };
                const auto flags{ rhi::GetFormatInfo( pDepthTexture->GetFormat() ).mHasStencil
                                      ? D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL
                                      : D3D12_CLEAR_FLAG_DEPTH };
                mCurrentRecordingContext->mCommandList->ClearDepthStencilView( dsvHandler, flags, 1.0f, 0,
                    partial ? 1u : 0u, partial ? &clearRect : nullptr );
            }
        }

        mCurrentRecordingContext->mCommandList->OMSetRenderTargets(
            core::as<UINT>( rtvHandles.size() ),
            rtvHandles.data(), FALSE,
            pDepthTexture ? MKT_ADDRESSOF( dsvHandler ) : nullptr );
    }

    auto CommandList::EndRenderPass() -> void {
        MKT_BEGIN_PROFILER();
        mIsRenderScopeActive = false;
    }

    auto CommandList::BindPipeline( renderer::rhi::IPipeline* pipeline ) -> void {
        if ( !pipeline || pipeline->GetOwningDevice() != mDevice ) {
            MKT_THROW_RUNTIME_ERROR( "Bind a pipeline from this command list's device." );
        }

        RetainResource( pipeline );
        mBoundRayTracingPipeline = nullptr;
        MKT_BEGIN_PROFILER();

        bool requiresEmptySignature{ false };

        const renderer::rhi::PipelineType pipelineType{ pipeline->GetPipelineType() };
        mBoundPipelineType = pipelineType;

        if ( pipelineType == renderer::rhi::PipelineType::eGraphics ) {
            auto* gfxPipeline{ core::checked_cast<GraphicsPipeline*>( pipeline ) };
            const auto& desc{ gfxPipeline->GetDescription() };
            ID3D12PipelineState* gfxPipelineState{ *gfxPipeline };
            D3D_PRIMITIVE_TOPOLOGY gfxPipelineTopology{ d3d12::GetCmdTopologyType( desc.mPrimitiveTopology ) };

            mCurrentRecordingContext->mCommandList->SetPipelineState( gfxPipelineState );
            mCurrentRecordingContext->mCommandList->IASetPrimitiveTopology( gfxPipelineTopology );

            requiresEmptySignature = gfxPipeline->HasEmptyRootSignature();
        }
        else if ( pipelineType == renderer::rhi::PipelineType::eCompute ) {
            auto* computePipeline{ core::checked_cast<ComputePipeline*>( pipeline ) };
            ID3D12PipelineState* computePipelineState{ *computePipeline };

            mCurrentRecordingContext->mCommandList->SetPipelineState( computePipelineState );
            requiresEmptySignature = computePipeline->HasEmptyRootSignature();
        }
        else if ( pipelineType == rhi::PipelineType::eRayTracing ) {
            auto* rayPipeline{ core::checked_cast<RayTracingPipeline*>( pipeline ) };
            auto command{ PrepareRayTracing( *rayPipeline ) };
            command->SetPipelineState1( rayPipeline->mPipeline.Get() );
            mBoundRayTracingPipeline = rayPipeline;
        }

        // I should probably bind the previous resources only if empty root signature.
        // This is more of a D3D12 limitation
        // In vulkan the resources stay bound for the duration of the command buffer
        // or until being invalidated by different pipeline layout
        if ( requiresEmptySignature ) {
            // If pipeline receives no resources we bind an empty root signature.
            // We do this here because you need a pipeline bound before you issue draw/dispatch calls.
            Device* device{ core::checked_cast<Device*>( mDevice ) };
            ID3D12RootSignature* rootSignature{ device->GetEmptyRootSignature() };

            if ( pipelineType == renderer::rhi::PipelineType::eCompute ) {
                mCurrentRecordingContext->mCommandList->SetComputeRootSignature( rootSignature );
            }
            else {
                mCurrentRecordingContext->mCommandList->SetGraphicsRootSignature( rootSignature );
            }
        }
    }

    auto CommandList::SetViewport( eastl::span<const renderer::rhi::Viewport> viewports ) -> void {
        MKT_BEGIN_PROFILER();
        eastl::fixed_vector<D3D12_VIEWPORT, kMaxViewports> d3dViewports{};

        for ( const auto& viewport: viewports ) {
            D3D12_VIEWPORT value{
                .TopLeftX = viewport.mMinX,
                .TopLeftY = viewport.mMinY,
                .Width = viewport.GetWidth(),
                .Height = viewport.GetHeight(),
                .MinDepth = viewport.mMinZ,
                .MaxDepth = viewport.mMaxZ
            };
            d3dViewports.emplace_back( value );
        }

        mCurrentRecordingContext->mCommandList->RSSetViewports( core::as<UINT>( d3dViewports.size() ), d3dViewports.data() );
    }


    auto CommandList::SetScissors( eastl::span<const renderer::rhi::Rect> scissorRects ) -> void {
        MKT_BEGIN_PROFILER();
        eastl::fixed_vector<D3D12_RECT, kMaxScissors> scissors{};

        for ( const auto& scissor: scissorRects ) {
            scissors.emplace_back( D3D12_RECT{
                .left = scissor.mMinX,
                .top = scissor.mMinY,
                .right = scissor.mMinX + core::as<LONG>( scissor.ComputeWidth() ),
                .bottom = scissor.mMinY + core::as<LONG>( scissor.ComputeHeight() ) } );
        }

        mCurrentRecordingContext->mCommandList->RSSetScissorRects( core::as<UINT>( scissors.size() ), scissors.data() );
    }

    auto CommandList::SetViewportState( const renderer::rhi::ViewportState& vs ) -> void {
        MKT_BEGIN_PROFILER();
        SetViewport( vs.mViewports );
        SetScissors( vs.mScissorRects );
    }

    auto CommandList::SetPolygonLineWidth( MKT_UNUSED_VAR core::f32 width ) -> void {
        // Not supported
    }

    auto CommandList::BindIndexBuffer( renderer::rhi::IBuffer* buffer ) -> void {
        MKT_BEGIN_PROFILER();
        RetainResource( buffer );
        Buffer* indexBuffer{ core::checked_cast<Buffer*>( buffer ) };
        ID3D12Resource* resource{ *indexBuffer };

        if ( mEnableAutomaticBarriers ) {
            SetTransition( indexBuffer, rhi::ResourceStates::eIndexBuffer );
        }

        D3D12_INDEX_BUFFER_VIEW indexBufferView{};

        indexBufferView.BufferLocation = resource->GetGPUVirtualAddress();
        indexBufferView.Format = d3d12::GetFormat( indexBuffer->GetFormat() );
        indexBufferView.SizeInBytes = indexBuffer->GetSizeBytes();

        mCurrentRecordingContext->mCommandList->IASetIndexBuffer( &indexBufferView );
    }

    auto CommandList::BindVertexBuffer( const renderer::rhi::VertexBufferBinding& binding ) -> void {
        MKT_BEGIN_PROFILER();
        BindVertexBuffers( eastl::array{ binding } );
    }

    auto CommandList::BindVertexBuffers( eastl::span<const renderer::rhi::VertexBufferBinding> binding ) -> void {
        MKT_BEGIN_PROFILER();

        for ( const auto& vertexBuffer: binding ) {
            Buffer* buffer{ core::checked_cast<Buffer*>( vertexBuffer.mBuffer ) };

            if ( !buffer || vertexBuffer.mSlot >= kMaxVertexBuffers || vertexBuffer.mOffset > buffer->GetSizeBytes() ||
                 buffer->GetSizeBytes() - vertexBuffer.mOffset > UINT32_MAX || vertexBuffer.mElementStride > UINT32_MAX ) {
                MKT_THROW_RUNTIME_ERROR( "Invalid D3D12 vertex buffer binding." );
            }

            RetainResource( buffer );
            ID3D12Resource* resource{ *buffer };

            if ( mEnableAutomaticBarriers ) {
                SetTransition( buffer, rhi::ResourceStates::eVertexBuffer );
            }

            D3D12_VERTEX_BUFFER_VIEW bufferView{};
            bufferView.SizeInBytes = static_cast<UINT>( buffer->GetSizeBytes() - vertexBuffer.mOffset );
            bufferView.BufferLocation = resource->GetGPUVirtualAddress() + vertexBuffer.mOffset;
            bufferView.StrideInBytes = static_cast<UINT>( vertexBuffer.mElementStride );

            mCurrentRecordingContext->mCommandList->IASetVertexBuffers( vertexBuffer.mSlot, 1, &bufferView );
        }
    }

    auto CommandList::BindPipelineResources( const renderer::rhi::BindResourcesDescription& desc ) -> void {
        MKT_BEGIN_PROFILER();
        // https://learn.microsoft.com/en-us/windows/win32/direct3d12/descriptor-heaps-overview

        Device* device{ core::checked_cast<Device*>( mDevice ) };
        PipelineLayout* pipelineLayout{ core::checked_cast<PipelineLayout*>( desc.mPipelineLayout ) };
        RetainResource( pipelineLayout );

        // SetDescriptorHeaps must be called to bind a CBV/SRV/UAV descriptor before setting a
        // root signature with D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED flag
        const DeviceResources* deviceResources{ device->GetHeapResources() };
        const eastl::array<ID3D12DescriptorHeap*, 2> descriptorHeaps{
            deviceResources->mSamplerDescriptorHeap->GetShaderVisibleHeap(),
            deviceResources->mResourceDescriptorHeap->GetShaderVisibleHeap()
        };

        // At most one CBV/SRV/UAV combined heap and one Sampler heap can be bound at any one time.
        // These heaps are shared between both the graphics and compute pipelines (described in their PSOs).
        mCurrentRecordingContext->mCommandList->SetDescriptorHeaps( core::as<UINT>( descriptorHeaps.size() ), descriptorHeaps.data() );

        // Root signature
        ID3D12RootSignature* rootSignature{ *pipelineLayout };

        if ( desc.mBindPoint == rhi::PipelineType::eCompute || desc.mBindPoint == rhi::PipelineType::eRayTracing ) {
            mCurrentRecordingContext->mCommandList->SetComputeRootSignature( rootSignature );
        }
        else {
            mCurrentRecordingContext->mCommandList->SetGraphicsRootSignature( rootSignature );
        }

        const auto bindResource{
            [&]( const DescriptorRange& range, core::u32 rootParamIndex, renderer::rhi::PipelineType pipelineType ) {
            if ( range.mCount == 0 ) {
                return;
            }

            switch ( pipelineType ) {
                case renderer::rhi::PipelineType::eGraphics:
                    mCurrentRecordingContext->mCommandList->SetGraphicsRootDescriptorTable( rootParamIndex, range.mGpuHandle );
                    break;
                case rhi::PipelineType::eRayTracing:
                case renderer::rhi::PipelineType::eCompute:
                    mCurrentRecordingContext->mCommandList->SetComputeRootDescriptorTable( rootParamIndex, range.mGpuHandle );
                    break;
                default:;
            }
        }
        };

        for ( const auto& [rootParameterIndex, bindingSet]: desc.mResourceSets ) {
            RetainResource( bindingSet );
            // FIXME: Descriptor tables use global heap
            if ( const auto* table{ dynamic_cast<DescriptorTable*>( bindingSet ) } ) {
                std::lock_guard lock{ table->mDescriptorMutex };

                for ( const auto& [index, resource]: table->mResources ) {
                    RetainResource( resource.GetPtr() );
                }

                continue;
            }

            auto snapshot{ core::checked_cast<BindingTable*>( bindingSet )->Snapshot() };
            RetainResource( snapshot.GetPtr() );
            const BindingTable* table{ core::checked_cast<BindingTable*>( snapshot.GetPtr() ) };
            const DescriptorRange& srvRange{ table->GetSrvRange() };
            const DescriptorRange& samplerRange{ table->GetSamplerRange() };

            if ( srvRange.mCount > 0 ) {
                bindResource( srvRange, pipelineLayout->GetBindingRootIndex( rootParameterIndex, false ), desc.mBindPoint );
            }

            if ( samplerRange.mCount > 0 ) {
                bindResource( samplerRange, pipelineLayout->GetBindingRootIndex( rootParameterIndex, true ), desc.mBindPoint );
            }
        }
    }

    auto CommandList::Draw( const renderer::rhi::DrawArguments& args ) -> void {
        MKT_BEGIN_PROFILER();
        mCurrentRecordingContext->mCommandList->DrawInstanced(
            args.mVertexCount,
            args.mInstanceCount,
            args.mFirstVertex,
            args.mFirstInstance );
    }

    auto CommandList::DrawIndexed( const renderer::rhi::DrawArguments& args ) -> void {
        MKT_BEGIN_PROFILER();
        mCurrentRecordingContext->mCommandList->DrawIndexedInstanced(
            args.mIndexCount,
            args.mInstanceCount,
            args.mFirstIndex,
            args.mVertexOffset,
            args.mFirstInstance );
    }

    auto CommandList::DrawIndirect( core::u32 offset, core::u32 drawCount ) -> void {
        if ( drawCount == 0 ) {
            return;
        }

        const core::u64 byteSize{ static_cast<core::u64>( drawCount ) * sizeof( D3D12_DRAW_ARGUMENTS ) };

        if ( !mIndirectBuffer || offset % sizeof( core::u32 ) || offset > mIndirectBuffer->GetSizeBytes() ||
             byteSize > mIndirectBuffer->GetSizeBytes() - offset ) {
            MKT_THROW_RUNTIME_ERROR( "D3D12 indirect draw arguments are outside the bound buffer." );
        }

        ID3D12Resource* arguments{ *core::checked_cast<Buffer*>( mIndirectBuffer ) };
        mCurrentRecordingContext->mCommandList->ExecuteIndirect(
            mDrawSignature.Get(), drawCount, arguments, offset, nullptr, 0 );
    }

    auto CommandList::DrawIndexedIndirect( core::u32 offset, core::u32 drawCount ) -> void {
        if ( drawCount == 0 ) {
            return;
        }

        const core::u64 byteSize{ static_cast<core::u64>( drawCount ) * sizeof( D3D12_DRAW_INDEXED_ARGUMENTS ) };

        if ( !mIndirectBuffer || offset % sizeof( core::u32 ) || offset > mIndirectBuffer->GetSizeBytes() ||
             byteSize > mIndirectBuffer->GetSizeBytes() - offset ) {
            MKT_THROW_RUNTIME_ERROR( "D3D12 indexed indirect draw arguments are outside the bound buffer." );
        }

        ID3D12Resource* arguments{ *core::checked_cast<Buffer*>( mIndirectBuffer ) };
        mCurrentRecordingContext->mCommandList->ExecuteIndirect(
            mDrawIndexedSignature.Get(), drawCount, arguments, offset, nullptr, 0 );
    }

    auto CommandList::Dispatch( core::u32 groupsX, core::u32 groupsY, core::u32 groupsZ ) -> void {
        MKT_BEGIN_PROFILER();
        mCurrentRecordingContext->mCommandList->Dispatch( groupsX, groupsY, groupsZ );
    }

    auto CommandList::SetPushConstants( renderer::rhi::IPipelineLayout* pipelineLayout, const void* data, core::usize byteSize, renderer::rhi::ShaderFlags visibility ) -> void {
        MKT_BEGIN_PROFILER();
        RetainResource( pipelineLayout );
        PipelineLayout* d3d12PipelineLayout{ core::checked_cast<PipelineLayout*>( pipelineLayout ) };

        if ( byteSize == 0 ) {
            return;
        }

        if ( !data || byteSize % sizeof( core::u32 ) || byteSize / sizeof( core::u32 ) > d3d12PipelineLayout->GetMax32BitValuesCount() ) {
            MKT_THROW_RUNTIME_ERROR( "D3D12 push constant data exceeds the pipeline layout or is not DWORD aligned." );
        }

        core::u32 rootParameterIndex{ d3d12PipelineLayout->GetRootConstantIndex() };
        core::u32 numValues32Bit{ core::as<core::u32>( ( byteSize + 3 ) / 4 ) };

        // DXR uses compute root arguments. Explicit stage visibility identifies
        // that namespace; All resolves against the currently bound pipeline.
        const auto rayStages{ rhi::ShaderFlagsBits::RayGeneration | rhi::ShaderFlagsBits::Miss | rhi::ShaderFlagsBits::ClosestHit };
        const bool compute{ visibility == rhi::ShaderFlagsBits::Compute ||
                            ( visibility != rhi::ShaderFlagsBits::All && ( visibility & rayStages ) ) ||
                            ( visibility == rhi::ShaderFlagsBits::All &&
                                ( mBoundPipelineType == rhi::PipelineType::eCompute || mBoundPipelineType == rhi::PipelineType::eRayTracing ) ) };

        if ( compute ) {
            mCurrentRecordingContext->mCommandList->SetComputeRoot32BitConstants(
                rootParameterIndex,
                numValues32Bit,
                data,
                0 );
        }
        else {
            mCurrentRecordingContext->mCommandList->SetGraphicsRoot32BitConstants(
                rootParameterIndex,
                numValues32Bit,
                data,
                0 );
        }
    }

    auto CommandList::GetNativeHandle( renderer::rhi::ObjectType type ) -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        if ( type == renderer::rhi::ObjectType::D3D12_CommandList && mCurrentRecordingContext ) {
            return renderer::rhi::Object{ GetCommandList() };
        }

        return renderer::rhi::ICommandList::GetNativeHandle( type );
    }

    auto CommandList::GetNativeHandle( renderer::rhi::ObjectType type ) const -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        if ( type == renderer::rhi::ObjectType::D3D12_CommandList && mCurrentRecordingContext ) {
            return renderer::rhi::Object{ GetCommandList() };
        }

        return renderer::rhi::ICommandList::GetNativeHandle( type );
    }

    auto CommandList::Reset( rhi::IQueryPool* queryPool ) -> void {
        auto& pool{ ValidateQueryPool( queryPool ) };
        auto& commands{ *this };

        if ( pool.mActive != ~0u ) {
            MKT_THROW_RUNTIME_ERROR( "Cannot reset an active query pool." );
        }

        ( RetainResource( &pool ), mCurrentRecordingContext->mCommandList.Get() );
        // D3D12 overwrites queries; there is no reset command. The caller must
        // still wait for the old submission before reusing heap/readback slots.
        eastl::fill( pool.mStates.begin(), pool.mStates.end(), QueryPool::SlotState::eReady );
    }

    auto CommandList::BeginQuery( rhi::IQueryPool* queryPool, core::u32 index ) -> void {
        auto& pool{ ValidateQueryPool( queryPool ) };
        auto& commands{ *this };

        pool.ValidateRange( index, 1 );

        if ( pool.mDescription.mType == renderer::rhi::QueryType::eTimestamp || pool.mStates[index] != QueryPool::SlotState::eReady || pool.mActive != ~0u ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid D3D12 query begin." );
        }

        ( RetainResource( &pool ), mCurrentRecordingContext->mCommandList.Get() )->BeginQuery( pool.mHeap.Get(), pool.mType, index );
        pool.mStates[index] = QueryPool::SlotState::eActive;
        pool.mActive = index;
        pool.mActiveCommands = &commands;
    }

    auto CommandList::EndQuery( rhi::IQueryPool* queryPool, core::u32 index ) -> void {
        auto& pool{ ValidateQueryPool( queryPool ) };
        auto& commands{ *this };

        pool.ValidateRange( index, 1 );

        if ( pool.mStates[index] != QueryPool::SlotState::eActive || pool.mActive != index || pool.mActiveCommands != &commands ) {
            MKT_THROW_RUNTIME_ERROR( "D3D12 query end has no matching begin." );
        }

        ( RetainResource( &pool ), mCurrentRecordingContext->mCommandList.Get() )->EndQuery( pool.mHeap.Get(), pool.mType, index );
        pool.mStates[index] = QueryPool::SlotState::eEnded;
        pool.mActive = ~0u;
        pool.mActiveCommands = nullptr;
    }

    auto CommandList::WriteTimestamp( rhi::IQueryPool* queryPool, core::u32 index ) -> void {
        auto& pool{ ValidateQueryPool( queryPool ) };
        auto& commands{ *this };

        pool.ValidateRange( index, 1 );

        if ( pool.mDescription.mType != renderer::rhi::QueryType::eTimestamp || pool.mStates[index] != QueryPool::SlotState::eReady ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid D3D12 timestamp query." );
        }

        ( RetainResource( &pool ), mCurrentRecordingContext->mCommandList.Get() )->EndQuery( pool.mHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, index );
        pool.mStates[index] = QueryPool::SlotState::eEnded;
    }

    auto CommandList::Resolve( rhi::IQueryPool* queryPool, core::u32 first, core::u32 count ) -> void {
        auto& pool{ ValidateQueryPool( queryPool ) };
        auto& commands{ *this };

        pool.ValidateRange( first, count );

        for ( core::u32 index{ first }; index < first + count; ++index ) {
            if ( pool.mStates[index] != QueryPool::SlotState::eEnded ) {
                MKT_THROW_RUNTIME_ERROR( "Cannot resolve an unwritten D3D12 query." );
            }
        }

        ( RetainResource( &pool ), mCurrentRecordingContext->mCommandList.Get() )->ResolveQueryData( pool.mHeap.Get(), pool.mType, first, count, pool.mReadback.Get(), core::u64{ first } * pool.mStride );
        eastl::fill( pool.mStates.begin() + first, pool.mStates.begin() + first + count, QueryPool::SlotState::eResolved );
    }

    auto CommandList::ValidateQueryPool( rhi::IQueryPool* queryPool ) const -> QueryPool& {
        auto* nativePool{ dynamic_cast<QueryPool*>( queryPool ) };

        if ( !nativePool ) {
            MKT_THROW_RUNTIME_ERROR( "Query pool belongs to a different backend or is null." );
        }

        auto& pool{ *nativePool };

        if ( !pool.IsReady() || mDevice != pool.mDevice || !mRecording ) {
            MKT_THROW_RUNTIME_ERROR( "Queries require an initialized pool and a recording command list from the same D3D12 device." );
        }

        // Logical compute currently aliases the graphics queue in this backend.
        if ( this->mQueue != pool.mDevice->GetQueue( pool.mDescription.mQueue ) ) {
            MKT_THROW_RUNTIME_ERROR( "Query pool and command list use different queue types." );
        }

        return pool;
    }

#if defined( MIKOTO_ENABLE_TRACY )
    auto CommandList::CollectGpuProfile() -> void {
        if ( mQueueType != QueueType::eGraphics || !TracyIsConnected ) {
            return;
        }

#if defined( TRACY_ON_DEMAND )
        const core::u64 connection{ tracy::GetProfiler().ConnectionId() };
#else
        constexpr core::u64 connection{};
#endif
        auto& queue{ *core::checked_cast<Queue*>( mQueue ) };
        if ( queue.mGpuProfilerConnection != connection ) {
            queue.InitializeGpuProfiler( connection );
        }

        auto& profiler{ mCurrentRecordingContext->mGpuProfiler };
        profiler = queue.mGpuProfiler;
        if ( profiler ) {
            TracyD3D12Collect( profiler.get() );
        }
    }

    auto CommandList::BeginGpuProfileZone( eastl::string_view name, std::source_location source ) -> void {
        auto* profiler{ mCurrentRecordingContext->mGpuProfiler.get() };
        if ( !profiler || !TracyIsConnected ) {
            return;
        }

        if ( !mGpuProfileZone ) {
            mGpuProfileZone = eastl::make_unique<GpuProfileZone>();
        }

        // Tracy resolves the timestamp pair as part of this command list.
        // https://github.com/wolfpld/tracy/blob/v0.14.1/public/tracy/TracyD3D12.hpp
        const eastl::string_view file{ source.file_name() };
        const eastl::string_view function{ source.function_name() };
        mGpuProfileZone->mScope.emplace( profiler, source.line(), file.data(), file.size(),
            function.data(), function.size(), name.data(), name.size(), mCurrentRecordingContext->mCommandList.Get(), true );
    }

    auto CommandList::EndGpuProfileZone() -> void {
        if ( mGpuProfileZone ) {
            mGpuProfileZone->mScope.reset();
        }
    }
#endif

    auto CommandList::BeginDebugLabel( eastl::string_view name, renderer::rhi::Color color ) -> void {
    }

    auto CommandList::EndDebugLabel() -> void {
    }

    auto CommandList::IsInUse() const -> bool {
        std::lock_guard lock{ mRecordingMutex };
        MKT_BEGIN_PROFILER();
        // Check if there is any of the recording contexts still running
        Queue* queue{ core::checked_cast<Queue*>( mQueue ) };

        for ( const auto& item: mRecordingContext ) {
            if ( queue->GetCurrentTimeline() < item.mSubmissionID ) {
                return true;
            }
        }

        return false;
    }

    CommandList::operator ID3D12GraphicsCommandList7*() const {
        MKT_BEGIN_PROFILER();
        return mCurrentRecordingContext->mCommandList.Get();
    }

    auto CommandList::GetCommandList() const -> ID3D12CommandList* {
        return mQueueType == QueueType::eVideoDecode
            ? static_cast<ID3D12CommandList*>( mCurrentRecordingContext->mVideoCommandList.Get() )
            : static_cast<ID3D12CommandList*>( mCurrentRecordingContext->mCommandList.Get() );
    }

    CommandList::~CommandList() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto CommandList::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        Device* device{ core::checked_cast<Device*>( mDevice ) };

        mUploadManager = core::checked_cast<Device*>( mDevice )->GetUploadManager();

        mRecordingContext.resize( kMaxRecordingContext );
        D3D12_INDIRECT_ARGUMENT_DESC argument{};
        argument.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
        D3D12_COMMAND_SIGNATURE_DESC signature{};
        signature.ByteStride = sizeof( D3D12_DRAW_ARGUMENTS );
        signature.NumArgumentDescs = 1;
        signature.pArgumentDescs = &argument;
        ThrowIfFailed( device->GetDevice()->CreateCommandSignature(
            &signature, nullptr, IID_PPV_ARGS( &mDrawSignature ) ) );
        argument.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
        signature.ByteStride = sizeof( D3D12_DRAW_INDEXED_ARGUMENTS );
        ThrowIfFailed( device->GetDevice()->CreateCommandSignature(
            &signature, nullptr, IID_PPV_ARGS( &mDrawIndexedSignature ) ) );

        for ( auto& item: mRecordingContext ) {
            item.mInFlightResources.reserve( 128 );
            const D3D12_COMMAND_LIST_TYPE cmdQueueType{ d3d12::GetQueueType( mQueueType ) };

            ThrowIfFailed( device->GetDevice()->CreateCommandAllocator( cmdQueueType, IID_PPV_ARGS( &item.mCommandAllocator ) ) );
            // Video recordings share allocator/fence retirement, but have their own native interface.
            if ( mQueueType == QueueType::eVideoDecode ) {
                ThrowIfFailed( device->GetDevice()->CreateCommandList( 0, cmdQueueType,
                    item.mCommandAllocator.Get(), nullptr, IID_PPV_ARGS( &item.mVideoCommandList ) ) );
                ThrowIfFailed( item.mVideoCommandList->Close() );
            }
            else {
                ThrowIfFailed( device->GetDevice()->CreateCommandList( 0, cmdQueueType,
                    item.mCommandAllocator.Get(), nullptr, IID_PPV_ARGS( &item.mCommandList ) ) );
                ThrowIfFailed( item.mCommandList->Close() );
            }
        }

        mCurrentRecordingContext = MKT_ADDRESSOF( mRecordingContext[0] );

        mUploadManager = core::checked_cast<Device*>( mDevice )->GetUploadManager();

        mIsAllocated = true;
    }

    auto CommandList::Destroy() -> void {
#if defined( MIKOTO_ENABLE_TRACY )
        EndGpuProfileZone();
#endif
        // Submitted lists are retained by their queue until completion.
        for ( auto& context: mRecordingContext ) {
            for ( auto* allocation: context.mInFlightSubAllocations ) {
                allocation->mInUse.clear();
            }

            context.mInFlightSubAllocations.clear();
            context.mInFlightResources.clear();
#if defined( MIKOTO_ENABLE_TRACY )
            context.mGpuProfiler.reset();
#endif
        }

        mIsAllocated = false;
    }

    auto CommandList::ClearState() -> void {
        for ( auto* allocation: mCurrentRecordingContext->mInFlightSubAllocations ) {
            allocation->mInUse.clear();
        }

        mCurrentRecordingContext->mInFlightSubAllocations.clear();
        mCurrentRecordingContext->mInFlightResources.clear();
#if defined( MIKOTO_ENABLE_TRACY )
        mCurrentRecordingContext->mGpuProfiler.reset();
#endif
        mResourceBarriers.clear();
        mBufferBarriers.clear();
        mTextureBarriers.clear();
        mIsRenderScopeActive = false;
        mIndirectBuffer = nullptr;
        mBoundPipelineType = renderer::rhi::PipelineType::eGraphics;
    }

    auto CommandList::ReleaseCompletedResources() -> void {
        std::lock_guard lock{ mRecordingMutex };
        const auto completed{ core::checked_cast<Queue*>( mQueue )->GetCurrentTimeline() };

        for ( auto& context: mRecordingContext ) {
            // A completed slot may already contain a new unsubmitted recording.
            if ( &context == mCurrentRecordingContext && ( mRecording || mReadyForSubmission ) ) {
                continue;
            }

            if ( context.mSubmissionID <= completed ) {
                for ( auto* allocation: context.mInFlightSubAllocations ) {
                    allocation->mInUse.clear();
                }

                context.mInFlightSubAllocations.clear();
                context.mInFlightResources.clear();
#if defined( MIKOTO_ENABLE_TRACY )
                context.mGpuProfiler.reset();
#endif
            }
        }
    }

    auto CommandList::RetainResource( const renderer::rhi::DeviceObject* resource ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !mRecording || resource == this ) {
            MKT_THROW_RUNTIME_ERROR( "RetainResource requires a recording command list and a distinct resource." );
        }

        if ( resource ) {
            // Repeated barriers/binds retain one reference in expected O(1)
            // time. Clearing still happens only after this context's fence.
            mCurrentRecordingContext->mInFlightResources.try_emplace(
                resource, const_cast<renderer::rhi::DeviceObject*>( resource ) );
        }
    }

    auto CommandList::MarkExecuted( rhi::IQueue* queue, core::u64 submissionID ) -> void {
        std::lock_guard lock{ mRecordingMutex };
        mCurrentRecordingContext->mSubmissionID = submissionID;
        mReadyForSubmission = false;
    }

    auto CommandList::BindIndirectBuffer( renderer::rhi::IBuffer* buffer ) -> void {
        mIndirectBuffer = buffer;
        RetainResource( buffer );
        MKT_BEGIN_PROFILER();

        if ( buffer && mEnableAutomaticBarriers ) {
            SetTransition( buffer, rhi::ResourceStates::eIndirectArgument );
        }
    }

    GpuUploadManager::GpuUploadManager( renderer::rhi::IGpuDevice* device )
        : mDevice{ device } {
    }

    auto GpuUploadManager::SubAllocate( core::usize byteSize, core::usize alignment ) -> GpuUploadAllocation* {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mMutex };
        auto* device{ core::checked_cast<Device*>( mDevice ) };

        StagingAllocation* stagingAlloc{};
        eastl::optional<memory::Allocation> subAllocProperties{};

        // Find the first allocation I can suballocate from and lock it
        if ( mBuffers.empty() ) {
            stagingAlloc = CreateBuffer( byteSize );
            subAllocProperties = stagingAlloc->mMemoryArena->Allocate( byteSize, alignment );

            if ( !subAllocProperties.has_value() ) {
                MKT_ASSERT( false, "Failed to sub-allocate" );
            }
        }
        else {
            // Try to find any buffer that has ranges available
            for ( auto& bufferInfo: mBuffers | std::views::values ) {
                stagingAlloc = bufferInfo.get();
                subAllocProperties = stagingAlloc->mMemoryArena->Allocate( byteSize, alignment );

                if ( subAllocProperties.has_value() ) {
                    break;
                }
            }

            // If none does allocate a new staging for uploads
            if ( !subAllocProperties.has_value() ) {
                stagingAlloc = CreateBuffer( byteSize );
                subAllocProperties = stagingAlloc->mMemoryArena->Allocate( byteSize, alignment );
            }
        }

        MKT_ASSERT( subAllocProperties.has_value() && subAllocProperties->mSize >= byteSize, "Failed to sub-allocate" );

        // Fill params
        GpuUploadAllocation* result{ CreateSubAllocation( stagingAlloc->mBuffer.GetPtr() ) };

        result->mMappedMemory = core::as<core::ubyte*>( core::checked_cast<Buffer*>( stagingAlloc->mBuffer.GetPtr() )->GetMappedAddress() ) + subAllocProperties->mOffset;
        result->mSize = subAllocProperties->mSize;
        result->mOffset = subAllocProperties->mOffset;
        result->mBuffer = stagingAlloc->mBuffer.GetPtr();
        result->mAllocation = *subAllocProperties;

        return result;
    }

    auto GpuUploadManager::ReclaimMemory() -> void {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mMutex };

        // Check fence, I'm not sure maybe timeline semaphores can be used instead
        auto it{ mSubAllocations.begin() };

        for ( ; it != mSubAllocations.end(); ) {
            auto& [buffer, subAllocations]{ *it };
            // Is it safe to return them back here?
            for ( auto itSub{ subAllocations.begin() }; itSub != subAllocations.end(); ) {
                if ( itSub->get() && !( *itSub )->mInUse.test() ) {
                    mBuffers[buffer]->mMemoryArena->Free( ( *itSub )->mAllocation );
                    itSub = subAllocations.erase( itSub );
                }
                else {
                    ++itSub;
                }
            }

            if ( subAllocations.empty() ) {
                subAllocations.clear();
                mBuffers.erase( it->first );
                it = mSubAllocations.erase( it );
            }
            else {
                ++it;
            }
        }
    }

    GpuUploadManager::~GpuUploadManager() {
        MKT_BEGIN_PROFILER();
        mBuffers.clear();
        mSubAllocations.clear();
    }

    auto GpuUploadManager::CreateBuffer( core::usize minimumSize ) -> StagingAllocation* {
        MKT_BEGIN_PROFILER();
        const core::usize initialSize{ glm::max( minimumSize, static_cast<core::usize>( MKT_MEGABYTES( 512 ) ) ) };

        auto bufferDes{ renderer::rhi::BufferCreateDescription{}
            .SetByteSize( initialSize )
            .SetCpuAccessType( renderer::rhi::AccessType::eWrite )
            .SetHeapType( renderer::rhi::HeapType::eUpload )
            .SetResourceType( renderer::rhi::ResourceType::eInvalid )// Is not a shader resource
            .SetBufferUsage( renderer::rhi::BufferUsageFlagsBits::CopySource ) };
        renderer::rhi::BufferHandle result{ mDevice->CreateBuffer( bufferDes ) };

        result->SetDebugName( string::Format( "Device uploadBuffer Size: {}", initialSize ) );

        auto& newAllocation{ mBuffers[result.GetPtr()] };
        newAllocation = eastl::make_unique<StagingAllocation>();
        newAllocation->mBuffer = result;
        newAllocation->mMemoryArena = eastl::make_unique<memory::MemoryArena<renderer::rhi::IBuffer, memory::FreeListFirstFitAllocator>>( result, initialSize );

        // Keep it mapped permanently
        ( void )mDevice->Map( result.GetPtr() );

        return newAllocation.get();
    }

    auto GpuUploadManager::CreateSubAllocation( renderer::rhi::IBuffer* buffer ) -> GpuUploadAllocation* {
        MKT_BEGIN_PROFILER();
        auto& result{ mSubAllocations[buffer].emplace_back( eastl::make_unique<GpuUploadAllocation>() ) };
        return result.get();
    }

    Device::Device( const renderer::rhi::GpuDeviceCreateInfo& createInfo )
        : mApi{ createInfo.mApi }, mDeviceType{ createInfo.mDeviceType }, mRequestedFeatures{ createInfo.mFeatureSupportFlags } {
    }

    auto Device::GetGraphicsApi() const -> renderer::rhi::GraphicsAPI {
        MKT_BEGIN_PROFILER();
        return mApi;
    }

    auto Device::IsInitialized() const -> bool {
        MKT_BEGIN_PROFILER();
        return mIsInitialized;
    }

    auto Device::IsGraphicsApi( renderer::rhi::GraphicsAPI api ) const -> bool {
        MKT_BEGIN_PROFILER();
        return mApi == api;
    }

    auto Device::IsFeatureSupported( rhi::GpuFeatureSupportFlags supportFlags ) const -> bool {
        MKT_BEGIN_PROFILER();
        return ( mSupportedFeatures & supportFlags ) == supportFlags;
    }

    auto Device::IsFeatureRequested( rhi::GpuFeatureSupportFlags requestFlags ) const -> bool {
        MKT_BEGIN_PROFILER();
        return ( mRequestedFeatures & requestFlags ) == requestFlags;
    }

    auto Device::GetDeviceName() const -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return mDeviceName;
    }

    auto Device::GetVendorID() const -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return mVendorID;
    }

    auto Device::GetDriverVersion() const -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return mDriverVersion;
    }

    auto Device::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        IDXGIFactory4* factory4{ core::checked_cast<Context*>( RenderSystem::Get()->GetContext() )->GetDxGIFactory4() };

        MKT_ASSERT( factory4, "A valid DirectX factory interface is required to create the device." );

        core::usize maxDedicatedVideoMemory{};
        Microsoft::WRL::ComPtr<IDXGIAdapter1> adapterForDescription{};

        // Here we basically keep in mAdapter1 an instance of the adapter that matches our look up criteria
        // adapterForDescription is simply used to fetch descriptions and not mAdapter1 as the later would otherwise get overwritten
        // from EnumAdapterByGpuPreference
        for ( UINT adapterIndex{};
            DXGI_ERROR_NOT_FOUND != factory4->EnumAdapters1( adapterIndex, &adapterForDescription );
            ++adapterIndex ) {
            DXGI_ADAPTER_DESC1 desc{};
            adapterForDescription->GetDesc1( &desc );

            if ( mDeviceType == renderer::rhi::GpuDeviceType::eSoftwareRasterizer ) {
                if ( desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE ) {
                    // Check to see if the adapter supports Direct3D 12, but don't create the actual device yet.
                    if ( SUCCEEDED( D3D12CreateDevice( adapterForDescription.Get(), D3D_FEATURE_LEVEL_12_0, _uuidof( ID3D12Device ), nullptr ) ) ) {
                        mAdapter1 = adapterForDescription;
                        break;
                    }
                }
            }
            else if ( mDeviceType == renderer::rhi::GpuDeviceType::eDiscrete ) {
                if ( desc.DedicatedVideoMemory > maxDedicatedVideoMemory && !( desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE ) ) {
                    maxDedicatedVideoMemory = desc.DedicatedVideoMemory;

                    // Only when we know it has more dedicated video memory than the previous one we check for D3D12 support.
                    if ( SUCCEEDED( D3D12CreateDevice( adapterForDescription.Get(), D3D_FEATURE_LEVEL_12_0, _uuidof( ID3D12Device ), nullptr ) ) ) {
                        mAdapter1 = adapterForDescription;
                    }
                }
            }
            else {
                if ( !( desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE ) ) {
                    // Here we pick whatever we get
                    if ( SUCCEEDED( D3D12CreateDevice( adapterForDescription.Get(), D3D_FEATURE_LEVEL_12_0, _uuidof( ID3D12Device ), nullptr ) ) ) {
                        mAdapter1 = adapterForDescription;
                    }
                }
            }
        }

        // Get our adapter 4 which is the
        // default interface we work with
        ThrowIfFailed( mAdapter1.As( &mAdapter4 ) );
        ThrowIfFailed( mAdapter4->GetDesc3( &mDeviceDescription3 ) );

        ThrowIfFailed( D3D12CreateDevice( mAdapter4.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS( &mDevice ) ) );
        mDevice->SetName( mDeviceDescription3.Description );

        // The renderer uses base-instance semantics for GPU-generated draws.
        // These are available in DXIL starting with shader model 6.8.
        D3D12_FEATURE_DATA_SHADER_MODEL shaderModelData{ D3D_SHADER_MODEL_6_8 };

        if ( FAILED( mDevice->CheckFeatureSupport( D3D12_FEATURE_SHADER_MODEL, &shaderModelData, sizeof( shaderModelData ) ) ) ||
             shaderModelData.HighestShaderModel < D3D_SHADER_MODEL_6_8 ) {
            MKT_THROW_RUNTIME_ERROR( "The D3D12 scene renderer requires Shader Model 6.8 and a compatible graphics driver/runtime." );
        }

        mDeviceFeaturesSupported[DeviceFeatureSupport::eShaderModel6_6] = true;
        mDeviceFeaturesSupported[DeviceFeatureSupport::eShaderModel6_8] = true;

        QuerySupportedFeatures();

        if ( !IsFeatureSupported( mRequestedFeatures ) ) {
            MKT_THROW_RUNTIME_ERROR( "The selected D3D12 device does not support all requested GPU features." );
        }

        mDeviceName = string::FromWChar( mDeviceDescription3.Description );
        mVendorID = string::Format( "{}", mDeviceDescription3.VendorId );

        InitInfoQueue();
        InitCommandQueues();
        InitMemoryAllocator();
        InitDescriptorHeapManager();

        InitDummyResources();

#if defined( _DEBUG )
        ThrowIfFailed( mDevice->QueryInterface( IID_PPV_ARGS( &mDebugDevice ) ) );
#endif
    }

    auto Device::Shutdown() -> void {
        MKT_BEGIN_PROFILER();
        WaitIdle();
        mQueues.clear();

#if defined( _DEBUG )
        mDebugDevice->ReportLiveDeviceObjects( D3D12_RLDO_DETAIL );

        if ( mInfoQueue1 ) {
            mInfoQueue1->UnregisterMessageCallback( mInfoQueueCallbackCookie );
        }
#endif
    }

    auto Device::QuerySupportedFeatures() -> void {
        MKT_BEGIN_PROFILER();

        // These capabilities are guaranteed by our D3D12 feature-level baseline.
        // D3D12 does not require a Vulkan-style feature-enable chain at device creation.
        mSupportedFeatures = rhi::GpuFeatureSupportFlagsBits::AnisotropicFiltering |
                             rhi::GpuFeatureSupportFlagsBits::HardwareWireframe |
                             rhi::GpuFeatureSupportFlagsBits::WindowPresentation |
                             rhi::GpuFeatureSupportFlagsBits::BCTextureCompression;

        // Direct heap indexing needs Shader Model 6.6 and resource binding tier 3.
        D3D12_FEATURE_DATA_D3D12_OPTIONS bindingOptions{};

        if ( SUCCEEDED( mDevice->CheckFeatureSupport( D3D12_FEATURE_D3D12_OPTIONS, &bindingOptions, sizeof( bindingOptions ) ) ) &&
             bindingOptions.ResourceBindingTier >= D3D12_RESOURCE_BINDING_TIER_3 &&
             IsSupported( DeviceFeatureSupport::eShaderModel6_6 ) ) {
            mSupportedFeatures |= rhi::GpuFeatureSupportFlagsBits::BindlessDescriptorTables;
        }

        // Probe optional hardware capabilities once, even when the caller did not request them.
        D3D12_FEATURE_DATA_D3D12_OPTIONS5 tracingOptions{};

        if ( SUCCEEDED( mDevice->CheckFeatureSupport( D3D12_FEATURE_D3D12_OPTIONS5, &tracingOptions, sizeof( tracingOptions ) ) ) &&
             tracingOptions.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0 ) {
            mSupportedFeatures |= rhi::GpuFeatureSupportFlagsBits::HardwareRayTracing;
        }

        D3D12_FEATURE_DATA_D3D12_OPTIONS7 meshOptions{};

        if ( SUCCEEDED( mDevice->CheckFeatureSupport( D3D12_FEATURE_D3D12_OPTIONS7, &meshOptions, sizeof( meshOptions ) ) ) &&
             meshOptions.MeshShaderTier >= D3D12_MESH_SHADER_TIER_1 ) {
            mSupportedFeatures |= rhi::GpuFeatureSupportFlagsBits::MeshShaders;
        }

        if ( SUCCEEDED( mDevice.As( &mVideoDevice ) ) && SupportsH264Decode( *mVideoDevice.Get() ) ) {
            mSupportedFeatures |= rhi::GpuFeatureSupportFlagsBits::VideoDecoding;
        }
    }

    auto Device::InitInfoQueue() -> void {
        MKT_BEGIN_PROFILER();
#if defined( _DEBUG )
        if ( FAILED( mDevice.As( &mInfoQueue ) ) ) {
            MKT_CORE_LOGGER_DEBUG( "Failed to acquire ID3D12InfoQueue." );
            return;
        }

        mInfoQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_CORRUPTION, IsDebuggerPresent() );
        mInfoQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_ERROR, IsDebuggerPresent() );

        // Expose break severity
#if false
        mInfoQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_WARNING, TRUE );
        mInfoQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_INFO, TRUE );
        mInfoQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_MESSAGE, TRUE );
#endif

        // Skip warnings
        // [06:03:38] STDOUT LOG [thread 37372] [D3D12] [WARNING] ID3D12CommandList::ClearRenderTargetView: The clear
        // values do not match those passed to resource creation. The clear operation is typically slower as a result;
        // but will still clear to the desired value.
        D3D12_INFO_QUEUE_FILTER filter{};

        D3D12_INFO_QUEUE_FILTER_DESC denyListDesc{};
        eastl::array denyIds{
            D3D12_MESSAGE_ID_CREATE_COMMANDLIST12,
            D3D12_MESSAGE_ID_DESTROY_COMMANDLIST12,
            D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE
        };

        denyListDesc.pIDList = denyIds.data();
        denyListDesc.NumIDs = core::as<UINT>( denyIds.size() );
        filter.DenyList = denyListDesc;

        ThrowIfFailed( mInfoQueue->PushStorageFilter( &filter ) );

        // https://github.com/microsoft/DirectX-Specs/blob/master/d3d/MessageCallback.md
        if ( SUCCEEDED( mDevice.As( &mInfoQueue1 ) ) ) {
            ThrowIfFailed(
                mInfoQueue1->RegisterMessageCallback(
                    &DebugMessageCallback,
                    D3D12_MESSAGE_CALLBACK_FLAG_NONE,
                    this,
                    &mInfoQueueCallbackCookie ) );

            MKT_CORE_LOGGER_DEBUG( "Using ID3D12InfoQueue1 message callback." );

            ThrowIfFailed( mInfoQueue1->PushStorageFilter( &filter ) );
        }
        else {
            MKT_CORE_LOGGER_DEBUG( "ID3D12InfoQueue1 unavailable, falling back to polling." );
        }
#endif
    }

    auto Device::InitCommandQueues() -> void {
        MKT_BEGIN_PROFILER();
        // D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_TYPE_COMPUTE and D3D12_COMMAND_LIST_TYPE_COPY
        // are guaranteed to be supported which is all we need for Graphics, Compute, Transfer and Present.
        // For the time being we will use one command queue type for all operations (D3D12_COMMAND_LIST_TYPE_DIRECT in this case)
        renderer::rhi::QueueHandle queue{ core::Ref<Queue>::New( renderer::rhi::QueueType::eGraphics,
            renderer::rhi::QueueOpSupportFlagsBits::Graphics | renderer::rhi::QueueOpSupportFlagsBits::Compute |
                renderer::rhi::QueueOpSupportFlagsBits::Transfer | renderer::rhi::QueueOpSupportFlagsBits::Presentation ) };

        queue->Initialize( this );

        // Same queue for all types
        eastl::vector<renderer::rhi::QueueType> queueTypes{
            renderer::rhi::QueueType::eGraphics, renderer::rhi::QueueType::eCompute,
            renderer::rhi::QueueType::eTransfer, renderer::rhi::QueueType::ePresent
        };

        for ( const auto& type: queueTypes ) {
            mQueues[type] = queue;
        }

        if ( IsFeatureSupported( GpuFeatureSupportFlagsBits::VideoDecoding ) ) {
            rhi::QueueHandle decode{ Ref<Queue>::New( QueueType::eVideoDecode, QueueOpSupportFlagsBits::VideoDecode ) };
            decode->Initialize( this );
            mQueues[QueueType::eVideoDecode] = decode;
        }
    }

    auto Device::InitDummyResources() -> void {
        MKT_BEGIN_PROFILER();
        // Empty Root signature
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData{};
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

        if ( FAILED( mDevice->CheckFeatureSupport( D3D12_FEATURE_ROOT_SIGNATURE,
                 MKT_ADDRESSOF( featureData ), MKT_SIZEOF( featureData ) ) ) ) {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc{};
        rootSignatureDesc.Version = featureData.HighestVersion;
        rootSignatureDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
        rootSignatureDesc.Desc_1_1.NumParameters = 0;
        rootSignatureDesc.Desc_1_1.pParameters = nullptr;
        rootSignatureDesc.Desc_1_1.NumStaticSamplers = 0;
        rootSignatureDesc.Desc_1_1.pStaticSamplers = nullptr;

        HRESULT hr{ D3D12SerializeVersionedRootSignature( &rootSignatureDesc,
            &mEmptyRootSignatureBlob, &mEmptyRootSignatureErrorMessages ) };

        if ( mEmptyRootSignatureErrorMessages && mEmptyRootSignatureErrorMessages->GetBufferSize() != 0 ) {
            eastl::string msg{ ( const char* )mEmptyRootSignatureErrorMessages->GetBufferPointer() };
            MKT_CORE_LOGGER_ERROR( "D3D12SerializeVersionedRootSignature Errors: {}", msg.c_str() );
        }

        ThrowIfFailed( hr );

        ThrowIfFailed( mDevice->CreateRootSignature( 0,
            mEmptyRootSignatureBlob->GetBufferPointer(),
            mEmptyRootSignatureBlob->GetBufferSize(),
            IID_PPV_ARGS( &mEmptyRootSignature ) ) );
    }

    auto Device::InitMemoryAllocator() -> void {
        MKT_BEGIN_PROFILER();
        mGpuAllocator = rhi::IGpuAllocator::Create( this );

        if ( !mGpuAllocator ) {
            MKT_THROW_RUNTIME_ERROR( "D3D12Device - Could not create GPU Allocator." );
        }

        mUploadManager = eastl::make_unique<GpuUploadManager>( this );

        mGpuAllocator->Init();
    }

    auto Device::InitDescriptorHeapManager() -> void {
        MKT_BEGIN_PROFILER();
        mResourceHeaps.mRenderTargetViewHeap = eastl::make_unique<StaticDescriptorHeap>( this, "RenderTargets" );
        mResourceHeaps.mDepthStencilViewHeap = eastl::make_unique<StaticDescriptorHeap>( this, "DepthStencil" );

        //mResourceHeaps.mSamplerHeap = eastl::make_unique<StaticDescriptorHeap>( this, "Samplers" );
        //mResourceHeaps.mShaderResourceViewHeap = eastl::make_unique<StaticDescriptorHeap>( this, "ShaderResources" );

        mResourceHeaps.mSamplerDescriptorHeap = eastl::make_unique<DynamicDescriptorHeap>( this, "SamplersDescriptors" );
        mResourceHeaps.mResourceDescriptorHeap = eastl::make_unique<DynamicDescriptorHeap>( this, "ResourcesDescriptors" );

        // Pre-allocate
        // Global heaps (require 6.6 support)
        // They are shader visible by default
        // https://learn.microsoft.com/en-gb/%20%20windows/win32/direct3d12/hardware-support
        ThrowIfFailed( mResourceHeaps.mSamplerDescriptorHeap->AllocateResources( D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 2048 ) );
        ThrowIfFailed( mResourceHeaps.mResourceDescriptorHeap->AllocateResources( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1'000'000 ) );


        // Normal heaps
        ThrowIfFailed( mResourceHeaps.mRenderTargetViewHeap->AllocateResources( D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 100, false ) );
        ThrowIfFailed( mResourceHeaps.mDepthStencilViewHeap->AllocateResources( D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 100, false ) );

        //ThrowIfFailed( mResourceHeaps.mSamplerHeap->AllocateResources( D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 100, true ) );
        //ThrowIfFailed( mResourceHeaps.mShaderResourceViewHeap->AllocateResources( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 100, true ) );
    }

    auto Device::CreateTexture( const renderer::rhi::TextureCreateDescription& description ) -> renderer::rhi::TextureHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::TextureHandle texture{ core::Ref<Texture>::New( description, mResourceHeaps ) };

        if ( texture.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "D3D12Device - Failed to allocate texture" );
            return renderer::rhi::TextureHandle::CreateEmpty();
        }

        texture->Initialize( this );

        return texture;
    }

    auto Device::CreateTexture( const ExternalTextureDescription& info ) -> renderer::rhi::TextureHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::TextureHandle texture{ core::Ref<Texture>::New( info, mResourceHeaps ) };

        if ( texture.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "D3D12Device - Failed to allocate texture" );
            return renderer::rhi::TextureHandle::CreateEmpty();
        }

        texture->Initialize( this );

        return texture;
    }

    auto Device::GetEmptyRootSignature() const -> ID3D12RootSignature* {
        MKT_BEGIN_PROFILER();
        return mEmptyRootSignature.Get();
    }

    auto Device::IsSupported( DeviceFeatureSupport feature ) const -> bool {
        MKT_BEGIN_PROFILER();
        const auto it{ mDeviceFeaturesSupported.find( feature ) };

        if ( it != mDeviceFeaturesSupported.end() ) {
            return it->second;
        }

        return false;
    }

    auto Device::CreateTextureNative( renderer::rhi::ObjectType type, renderer::rhi::Object object, const renderer::rhi::TextureCreateDescription& description ) -> renderer::rhi::TextureHandle {
        MKT_BEGIN_PROFILER();
        return renderer::rhi::TextureHandle::CreateEmpty();
    }

    auto Device::CreateSampler( const renderer::rhi::SamplerCreateDescription& description ) -> renderer::rhi::SamplerHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::SamplerHandle sampler{ core::Ref<Sampler>::New( description, mResourceHeaps ) };

        if ( sampler.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate sampler resource." );
            return renderer::rhi::SamplerHandle::CreateEmpty();
        }

        sampler->Initialize( this );

        return sampler;
    }


    auto Device::CreateCommandList( renderer::rhi::QueueType queueType ) -> renderer::rhi::CommandListHandle {
        MKT_BEGIN_PROFILER();
        Queue* queue{ core::checked_cast<Queue*>( GetQueue( queueType ) ) };

        auto handle{ renderer::rhi::CommandListHandle::CreateEmpty() };

        if ( queue ) {
            handle = queue->AllocateCmdList();
        }

        return handle;
    }

    auto Device::CreateCommandList( const rhi::CommandListCreateDescription& desc ) -> rhi::CommandListHandle {
        auto command{ CreateCommandList( desc.mQueueType ) };

        if ( command && !desc.mName.empty() ) {
            command->SetDebugName( desc.mName );
        }

        return command;
    }

    auto Device::CreateShader( const renderer::rhi::ShaderModuleCreateDescription& desc ) -> renderer::rhi::ShaderModuleHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::ShaderModuleHandle result{ core::Ref<Shader>::New( desc ) };

        if ( result.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to create shader." );
            return renderer::rhi::ShaderModuleHandle::CreateEmpty();
        }

        result->Initialize( this );

        return result;
    }

    auto Device::CreateInputLayout( const renderer::rhi::InputLayoutCreateDescription& desc ) -> renderer::rhi::InputLayoutHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::InputLayoutHandle layout{ core::Ref<InputLayout>::New( desc ) };

        if ( layout.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate binding layout resource." );
            return renderer::rhi::InputLayoutHandle::CreateEmpty();
        }

        layout->Initialize( this );

        return layout;
    }

    auto Device::CreateBindingLayout( const renderer::rhi::BindingLayoutDescription& desc ) -> renderer::rhi::BindingLayoutHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::BindingLayoutHandle layout{ core::Ref<BindingLayout>::New( desc ) };

        if ( layout.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate binding layout resource." );
            return renderer::rhi::BindingLayoutHandle::CreateEmpty();
        }

        layout->Initialize( this );

        return layout;
    }

    auto Device::CreatePipelineLayout( const renderer::rhi::PipelineLayoutCreateDescription& desc ) -> renderer::rhi::PipelineLayoutHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::PipelineLayoutHandle layout{ core::Ref<PipelineLayout>::New( desc ) };

        if ( layout.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate pipeline layout resource." );
            return renderer::rhi::PipelineLayoutHandle::CreateEmpty();
        }

        layout->Initialize( this );

        return layout;
    }

    auto Device::CreateBindingTable( const renderer::rhi::BindingTableDescription& desc, renderer::rhi::BindingLayoutHandle layout ) -> renderer::rhi::BindingTableHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::BindingTableHandle set{ core::Ref<BindingTable>::New( desc, layout, mResourceHeaps ) };

        if ( set.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate binding set resource." );
            return renderer::rhi::BindingTableHandle::CreateEmpty();
        }

        set->Initialize( this );

        return set;
    }

    auto Device::CreateFence( core::u64 fenceInitialValue ) -> renderer::rhi::FenceHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::FenceHandle fence{ core::Ref<Fence>::New( fenceInitialValue ) };

        if ( fence.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate fence resource." );
            return renderer::rhi::FenceHandle::CreateEmpty();
        }

        fence->Initialize( this );

        return fence;
    }

    auto Device::UnMap( renderer::rhi::IBuffer* buffer ) -> void {
        MKT_BEGIN_PROFILER();
        Buffer* b{ core::checked_cast<Buffer*>( buffer ) };

        if ( b->IsMapped() ) {
            b->PersistentUnmap();
        }
    }

    auto Device::Map( renderer::rhi::IBuffer* buffer ) -> void* {
        MKT_BEGIN_PROFILER();
        Buffer* b{ core::checked_cast<Buffer*>( buffer ) };

        if ( !b->IsMapped() ) {
            b->PersistentMap();
        }

        return b->GetMappedAddress();
    }

    auto Device::ReadTextureRegion( const rhi::ITexture*, const rhi::Rect&, eastl::span<core::u8> ) -> bool {
        return false;
    }

    auto Device::CreateDescriptorTableLayout( const renderer::rhi::DescriptorTableLayoutDescription& desc ) -> renderer::rhi::BindingLayoutHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::BindingLayoutHandle layout{ core::Ref<BindingLayout>::New( desc ) };

        if ( layout.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate binding layout resource." );
            return renderer::rhi::BindingLayoutHandle::CreateEmpty();
        }

        layout->Initialize( this );

        return layout;
    }

    auto Device::CreateDescriptorTable( renderer::rhi::BindingLayoutHandle layout ) -> renderer::rhi::DescriptorTableHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::DescriptorTableHandle table{ core::Ref<DescriptorTable>::New( layout, mResourceHeaps ) };

        if ( table.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate descriptor table resource." );
            return renderer::rhi::DescriptorTableHandle::CreateEmpty();
        }

        table->Initialize( this );

        return table;
    }

    auto Device::ResizeDescriptorTable( renderer::rhi::DescriptorTableHandle descriptorTable, core::u32 newSize, bool keepContents ) -> bool {
        MKT_BEGIN_PROFILER();
        return false;
    }

    auto Device::ReleaseDescriptorTableIndex( rhi::DescriptorTableHandle descriptorTable, core::u32 bindingIndex, rhi::DescriptorTableIndex index ) -> void {
        MKT_BEGIN_PROFILER();
        DescriptorTable* table{ core::checked_cast<DescriptorTable*>( descriptorTable.GetPtr() ) };

        if ( !table ) {
            return;
        }

        // The binding determines which global heap owns this descriptor index.
        const auto& layout{ table->GetLayout()->GetBindlessLayoutDesc() };

        for ( const auto& item: layout.mSlots ) {
            if ( item.mSlot == bindingIndex ) {
                table->UnRegisterIndex( index, item.mResourceType, bindingIndex );
                return;
            }
        }

        MKT_THROW_RUNTIME_ERROR( "D3D12 bindless descriptor slot does not exist." );
    }

    auto Device::WriteDescriptorTable( renderer::rhi::DescriptorTableHandle descriptorTable, const renderer::rhi::BindingTableItem& item ) -> rhi::DescriptorTableIndex {
        MKT_BEGIN_PROFILER();

        if ( descriptorTable.IsEmpty() ) {
            return rhi::kInvalidBindingItemIndex;
        }

        // This method assumes shader model 6.6 for this to access resources in shaders we'd use
        // ResourceDescriptorHeap or SamplerDescriptorHeap
        if ( item.mResourceType == renderer::rhi::ResourceType::eRayTracingAccelStruct ) {
            auto* structure{ dynamic_cast<AccelerationStructure*>( item.mResource ) };

            if ( !structure || structure->GetOwningDevice() != this ||
                 structure->GetType() != renderer::rhi::AccelStructureType::eTopLevel ) {
                MKT_THROW_RUNTIME_ERROR( "Bindless acceleration-structure descriptors require a TLAS on this device." );
            }
        }

        DescriptorTable* table{ core::checked_cast<DescriptorTable*>( descriptorTable.GetPtr() ) };
        DescriptorIndex newIndex{ table->AllocateIndex( item.mResourceType, item.mBindingIndex ) };

        if ( renderer::rhi::IsSampler( item.mResourceType ) ) {
            Sampler* sampler{ core::checked_cast<Sampler*>( item.mResource ) };
            D3D12_CPU_DESCRIPTOR_HANDLE cpu{ mResourceHeaps.mSamplerDescriptorHeap->GetCpuHandle( newIndex ) };
            sampler->AllocateSampler( cpu );
        }
        else {
            D3D12_CPU_DESCRIPTOR_HANDLE cpu{ mResourceHeaps.mResourceDescriptorHeap->GetCpuHandle( newIndex ) };

            switch ( item.mResourceType ) {
                case renderer::rhi::ResourceType::eRayTracingAccelStruct: {
                    D3D12_SHADER_RESOURCE_VIEW_DESC view{};
                    view.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
                    view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                    view.RaytracingAccelerationStructure.Location = static_cast<AccelerationStructure*>( item.mResource )->GetAddress();
                    mDevice->CreateShaderResourceView( nullptr, &view, cpu );
                    break;
                }

                case renderer::rhi::ResourceType::eTexture_SRV: {
                    Texture* texture{ core::checked_cast<Texture*>( item.mResource ) };
                    texture->CreateSrv( cpu.ptr, item.mSubResourceSet, item.mFormat, item.mDimension );
                    break;
                }

                case renderer::rhi::ResourceType::eTexture_UAV: {
                    Texture* texture{ core::checked_cast<Texture*>( item.mResource ) };
                    texture->CreateUav( cpu.ptr, item.mSubResourceSet, item.mFormat, item.mDimension );
                    break;
                }

                case renderer::rhi::ResourceType::eRawBuffer_UAV:
                case renderer::rhi::ResourceType::eTypedBuffer_UAV:
                case renderer::rhi::ResourceType::eStructuredBuffer_UAV: {
                    Buffer* buffer{ core::checked_cast<Buffer*>( item.mResource ) };
                    buffer->CreateUAV( cpu.ptr, item.mRange, item.mResourceType, item.mFormat );
                    break;
                }

                case renderer::rhi::ResourceType::eRawBuffer_SRV:
                case renderer::rhi::ResourceType::eTypedBuffer_SRV:
                case renderer::rhi::ResourceType::eStructuredBuffer_SRV: {
                    Buffer* buffer{ core::checked_cast<Buffer*>( item.mResource ) };
                    buffer->CreateSRV( cpu.ptr, item.mRange, item.mResourceType, item.mFormat );
                    break;
                }

                case renderer::rhi::ResourceType::eConstantBuffer: {
                    Buffer* buffer{ core::checked_cast<Buffer*>( item.mResource ) };
                    buffer->CreateCBV( cpu.ptr, item.mRange, item.mFormat );
                    break;
                }

                default:;
            }
        }

        {
            std::lock_guard lock{ table->mDescriptorMutex };
            table->mResources[( core::u64{ item.mBindingIndex } << 32 ) | newIndex] = core::checked_cast<rhi::DeviceObject*>( item.mResource );
        }

        return newIndex;
    }

    auto Device::GetQueryCapabilities( renderer::rhi::QueryType type, renderer::rhi::QueueType queue ) const -> renderer::rhi::QueryCapabilities {
        renderer::rhi::QueryCapabilities result{};

        if ( !mDevice || ( queue != renderer::rhi::QueueType::eGraphics && queue != renderer::rhi::QueueType::eCompute ) ) {
            result.mReason = "Queries are exposed on graphics and compute queues.";
            return result;
        }

        if ( type == renderer::rhi::QueryType::eTimestamp ) {
            const auto found{ mQueues.find( queue ) };

            if ( found != mQueues.end() ) {
                ID3D12CommandQueue* native{ *found->second };
                core::u64 frequency{};

                if ( native && SUCCEEDED( native->GetTimestampFrequency( &frequency ) ) && frequency ) {
                    result.mSupported = true;
                    result.mTimestampValidBits = 64;
                    result.mTimestampPeriodNs = 1e9 / static_cast<core::f64>( frequency );
                }
            }
        }
        else if ( type == renderer::rhi::QueryType::ePipelineStatistics && queue == renderer::rhi::QueueType::eGraphics ) {
            result.mSupported = true;
            result.mStatisticsMask = ( core::u64{ 1 } << 11 ) - 1;
        }
        else if ( queue == renderer::rhi::QueueType::eGraphics && ( type == renderer::rhi::QueryType::eOcclusion || type == renderer::rhi::QueryType::eTransformFeedback ) ) {
            result.mSupported = true;
            result.mPreciseOcclusion = type == renderer::rhi::QueryType::eOcclusion;
        }
        else if ( type == renderer::rhi::QueryType::eMeshShader && queue == renderer::rhi::QueueType::eGraphics ) {
            D3D12_FEATURE_DATA_D3D12_OPTIONS9 options{};

            if ( SUCCEEDED( mDevice->CheckFeatureSupport( D3D12_FEATURE_D3D12_OPTIONS9, &options, sizeof( options ) ) ) &&
                 options.MeshShaderPipelineStatsSupported ) {
                result.mSupported = true;
                result.mStatisticsMask = ( core::u64{ 1 } << 14 ) - 1;
            }
        }

        if ( result.mSupported ) {
            result.mReason = "";
        }
        else {
            result.mReason = "No enabled D3D12 mapping for this query type.";
        }

        return result;
    }

    auto Device::CreateQueryPool( const renderer::rhi::QueryPoolDescription& description ) -> renderer::rhi::QueryPoolHandle {
        if ( !GetQueryCapabilities( description.mType, description.mQueue ).mSupported ) {
            return renderer::rhi::QueryPoolHandle::CreateEmpty();
        }

        renderer::rhi::QueryPoolHandle queryPool{ core::Ref<QueryPool>::New( description ) };

        if ( queryPool.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate query pool." );
            return renderer::rhi::QueryPoolHandle::CreateEmpty();
        }

        queryPool->Initialize( this );

        return queryPool;
    }

    auto Device::WaitIdle() -> void {
        MKT_BEGIN_PROFILER();
        eastl::fixed_vector<renderer::rhi::IQueue*, 4> waited{};

        for ( auto& queue: mQueues | std::views::values ) {
            if ( eastl::find( waited.begin(), waited.end(), queue.GetPtr() ) == waited.end() ) {
                queue->WaitIdle();
                waited.push_back( queue.GetPtr() );
            }
        }
    }

    auto Device::GetDevice() -> ID3D12Device2* {
        MKT_BEGIN_PROFILER();
        return mDevice.Get();
    }

    auto Device::GetAdapter() -> IDXGIAdapter4* {
        MKT_BEGIN_PROFILER();
        return mAdapter4.Get();
    }

    auto Device::GetHeapResources() -> DeviceResources* {
        MKT_BEGIN_PROFILER();
        return MKT_ADDRESSOF( mResourceHeaps );
    }

    auto Device::GetHeapResources() const -> const DeviceResources* {
        MKT_BEGIN_PROFILER();
        return MKT_ADDRESSOF( mResourceHeaps );
    }

    auto Device::GetQueue( renderer::rhi::QueueType type ) -> renderer::rhi::IQueue* {
        MKT_BEGIN_PROFILER();
        const auto found{ mQueues.find( type ) };
        return found != mQueues.end() ? found->second.GetPtr() : nullptr;
    }

    auto Device::GetVideoDevice() const -> ID3D12VideoDevice* {
        return mVideoDevice.Get();
    }

    auto Device::CreateVideoDecoder( const VideoDecoderDescription& description ) -> VideoDecoderHandle {
        if ( !description.mConfiguration || !GetQueue( QueueType::eVideoDecode ) ) {
            return {};
        }

        VideoDecoderHandle decoder{ Ref<VideoDecoder>::New( description ) };
        decoder->Initialize( this );
        return decoder;
    }

    auto Device::GetProcessMemoryUsage() const -> rhi::ProcessGpuMemory {
        return mGpuAllocator ? mGpuAllocator->GetProcessMemoryUsage() : rhi::ProcessGpuMemory{};
    }

    auto Device::GetMemoryUsage() const -> core::usize {
        MKT_BEGIN_PROFILER();
        return mGpuAllocator->GetMemoryUsage();
    }

    auto Device::GetMemoryTotal() const -> core::usize {
        MKT_BEGIN_PROFILER();
        return mGpuAllocator->GetMemoryTotal();
    }

    auto Device::GetMemoryAvailable() const -> core::usize {
        MKT_BEGIN_PROFILER();
        return mGpuAllocator->GetMemoryAvailable();
    }

    auto Device::GetAllocator() -> GpuMemoryAllocator* {
        MKT_BEGIN_PROFILER();
        return core::checked_cast<GpuMemoryAllocator*>( mGpuAllocator.get() );
    }

    auto Device::GetUploadManager() -> GpuUploadManager* {
        MKT_BEGIN_PROFILER();
        return mUploadManager.get();
    }

    auto Device::UpdateBindingTable( const rhi::BindingTableDescription& desc, rhi::BindingTableHandle table ) -> void {
        if ( !table ) {
            MKT_THROW_RUNTIME_ERROR( "Cannot update an empty D3D12 binding table." );
        }

        core::checked_cast<BindingTable*>( table.GetPtr() )->Update( desc );
    }

    auto Device::CreateSwapChain( const renderer::rhi::SwapChainDescription& description ) -> rhi::SwapChainHandle {
        MKT_BEGIN_PROFILER();
        return rhi::SwapChainHandle::CreateEmpty();
    }

    auto Device::SupportsSceneMultisampling( renderer::rhi::Multisampling samples ) const -> bool {
        // Both attachments must support the requested sample count.
        for ( const auto format: { DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_D32_FLOAT } ) {
            D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS levels{};
            levels.Format = format;
            levels.SampleCount = d3d12::GetSampleCount( samples );

            if ( FAILED( mDevice->CheckFeatureSupport( D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &levels, sizeof( levels ) ) ) ||
                 levels.NumQualityLevels == 0 ) {
                return false;
            }
        }

        return true;
    }

    auto Device::DumpMessages() -> void {
        MKT_BEGIN_PROFILER();
#if !defined( NDEBUG )

        if ( !mInfoQueue ) {
            return;
        }

        const UINT64 messageCount{ mInfoQueue->GetNumStoredMessages() };

        for ( UINT64 i{}; i < messageCount; ++i ) {
            core::usize messageSize{};
            ThrowIfFailed( mInfoQueue->GetMessage( i, nullptr, &messageSize ) );

            eastl::vector<std::byte> storage( messageSize );
            auto* message{ reinterpret_cast<D3D12_MESSAGE*>( storage.data() ) };

            ThrowIfFailed( mInfoQueue->GetMessage( i, message, &messageSize ) );

            switch ( message->Severity ) {
                case D3D12_MESSAGE_SEVERITY_CORRUPTION:
                    MKT_CORE_LOGGER_CRITICAL( "[D3D12] {}", message->pDescription );
                    break;

                case D3D12_MESSAGE_SEVERITY_ERROR:
                    MKT_CORE_LOGGER_ERROR( "[D3D12] {}", message->pDescription );
                    break;

                case D3D12_MESSAGE_SEVERITY_WARNING:
                    MKT_CORE_LOGGER_WARN( "[D3D12] {}", message->pDescription );
                    break;

                case D3D12_MESSAGE_SEVERITY_INFO:
                    MKT_CORE_LOGGER_INFO( "[D3D12] {}", message->pDescription );
                    break;

                case D3D12_MESSAGE_SEVERITY_MESSAGE:
                    MKT_CORE_LOGGER_DEBUG( "[D3D12] {}", message->pDescription );
                    break;

                default:
                    MKT_CORE_LOGGER_DEBUG( "[D3D12][UNKNOWN] {}", message->pDescription );
                    break;
            }
        }

        mInfoQueue->ClearStoredMessages();
#endif
    }

    auto CALLBACK Device::DebugMessageCallback(
        D3D12_MESSAGE_CATEGORY,
        D3D12_MESSAGE_SEVERITY severity,
        D3D12_MESSAGE_ID,
        LPCSTR description,
        void* ) -> void {
        MKT_BEGIN_PROFILER();

        switch ( severity ) {
            case D3D12_MESSAGE_SEVERITY_CORRUPTION:
                MKT_CORE_LOGGER_CRITICAL( "[D3D12] [CORRUPTION] {}", description );
                break;
            case D3D12_MESSAGE_SEVERITY_ERROR:
                MKT_CORE_LOGGER_ERROR( "[D3D12] [ERROR] {}", description );
                break;
            case D3D12_MESSAGE_SEVERITY_WARNING:
                MKT_CORE_LOGGER_WARN( "[D3D12] [WARNING] {}", description );
                break;
            case D3D12_MESSAGE_SEVERITY_INFO:
                MKT_CORE_LOGGER_INFO( "[D3D12] [INFO] {}", description );
                break;
            case D3D12_MESSAGE_SEVERITY_MESSAGE:
                MKT_CORE_LOGGER_DEBUG( "[D3D12] [MESSAGE] {}", description );
                break;
            default:
                MKT_CORE_LOGGER_DEBUG( "[D3D12][UNKNOWN] {}", description );
                break;
        }
    }

    auto Device::CreateSwapChain( platform::IWindow* window, Microsoft::WRL::ComPtr<IDXGIFactory4> dxgiFactory ) -> D3D12SwapChainHandle {
        MKT_BEGIN_PROFILER();
        auto handle{ D3D12SwapChainHandle::New( window, dxgiFactory ) };

        if ( !handle.IsEmpty() ) {
            handle->Initialize( this );
        }

        return handle;
    }

    auto Device::CreateBuffer( const renderer::rhi::BufferCreateDescription& description ) -> renderer::rhi::BufferHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::BufferHandle buffer{ core::Ref<Buffer>::New( description, mResourceHeaps ) };

        if ( buffer.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate buffer resource." );
            return renderer::rhi::BufferHandle::CreateEmpty();
        }

        buffer->Initialize( this );

        return buffer;
    }

    auto Device::CreateShaderTable( const rhi::ShaderTableDescription& description ) -> rhi::ShaderTableHandle {
        MKT_BEGIN_PROFILER();
        rhi::ShaderTableHandle table{ core::Ref<ShaderTable>::New( description ) };

        if ( table.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "CreateShaderTable - Failed to allocate shader table." );
            return rhi::ShaderTableHandle::CreateEmpty();
        }

        table->Initialize( this );
        return table;
    }

    auto Device::CreatePipeline( const renderer::rhi::ComputePipelineDescription& description ) -> renderer::rhi::PipelineHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::PipelineHandle computePipeline{ core::Ref<ComputePipeline>::New( description ) };

        if ( computePipeline.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate compute pipeline resource." );
            return renderer::rhi::PipelineHandle::CreateEmpty();
        }

        computePipeline->Initialize( this );

        return computePipeline;
    }

    auto Device::CreatePipeline( const renderer::rhi::GraphicsPipelineDescription& description ) -> renderer::rhi::PipelineHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::PipelineHandle graphicsPipeline{ core::Ref<GraphicsPipeline>::New( description ) };

        if ( graphicsPipeline.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate graphics pipeline resource." );
            return renderer::rhi::PipelineHandle::CreateEmpty();
        }

        graphicsPipeline->Initialize( this );

        return graphicsPipeline;
    }

    auto Device::RunGarbageCollection() -> void {
        MKT_BEGIN_PROFILER();
        mGpuAllocator->BeginFrame();
        eastl::fixed_vector<Queue*, 4> visited{};

        for ( auto& handle: mQueues | std::views::values ) {
            if ( eastl::find( visited.begin(), visited.end(), handle.GetPtr() ) == visited.end() ) {
                visited.push_back( handle.GetPtr() );
                handle->CollectGarbage();
            }
        }

        mUploadManager->ReclaimMemory();
    }

    ShaderTable::ShaderTable( const rhi::ShaderTableDescription& description )
        : mDescription{ description } {
        mPipeline = rhi::RayTracingPipelineHandle{ core::checked_cast<rhi::RayTracingPipeline*>( description.mPipeline ) };
    }

    ShaderTable::~ShaderTable() {
        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto ShaderTable::GetDesc() const -> const rhi::ShaderTableDescription& {
        return mDescription;
    }

    auto ShaderTable::GetPipeline() const -> rhi::IPipeline* {
        return mDescription.mPipeline;
    }

    auto ShaderTable::GetNumEntries() const -> core::u32 {
        return 1u + mDescription.mMissRecordCount + mDescription.mHitGroupRecordCount;
    }

    auto ShaderTable::GetRayGenerationRegion() const -> const D3D12_GPU_VIRTUAL_ADDRESS_RANGE& {
        return mRayGenRegion;
    }

    auto ShaderTable::GetMissRegion() const -> const D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE& {
        return mMissRegion;
    }

    auto ShaderTable::GetHitGroupRegion() const -> const D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE& {
        return mHitGroupsRegion;
    }

    auto ShaderTable::GetCallableRegion() const -> const D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE& {
        return mCallableRegion;
    }

    auto ShaderTable::GetBuffer() const -> const rhi::BufferHandle& {
        return mSbtBuffer;
    }

    auto ShaderTable::Destroy() -> void {
        mSbtBuffer.Reset();
        mRayGenRegion = {};
        mMissRegion = {};
        mHitGroupsRegion = {};
        mCallableRegion = {};
        mIsAllocated = false;
    }

    auto ShaderTable::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        RayTracingPipeline* pipeline{ core::checked_cast<RayTracingPipeline*>( mPipeline.GetPtr() ) };

        // DXR exports identifiers from the state object in the same group order.
        Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> properties{};
        ThrowIfFailed( pipeline->mPipeline.As( &properties ) );
        const auto& description{ pipeline->GetDescription() };
        const eastl::wstring names[]{
            pipeline->mShaderExports[0], pipeline->mShaderExports[1],
            string::ToWide( description.mHitGroupName )
        };

        constexpr core::u32 handleSize{ D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES };
        eastl::vector<core::u8> shaderHandles{};
        shaderHandles.resize( handleSize * 3 );

        for ( core::u32 index{}; index < 3; ++index ) {
            const void* identifier{ properties->GetShaderIdentifier( names[index].c_str() ) };

            if ( !identifier ) {
                MKT_THROW_RUNTIME_ERROR( "DXR shader export was not found." );
            }

            std::memcpy( shaderHandles.data() + index * handleSize, identifier, handleSize );
        }

        // DXR records use 32-byte alignment and region starts use 64-byte alignment.
        constexpr core::u64 handleAlignment{ D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT };
        constexpr core::u64 baseAlignment{ D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT };

        const core::u64 stride{ memory::AlignUp( handleSize, handleAlignment ) };
        const core::u64 rayGenSize{ stride };
        const core::u64 missSize{ stride * mDescription.mMissRecordCount };
        const core::u64 hitSize{ stride * mDescription.mHitGroupRecordCount };
        const core::u64 missOffset{ memory::AlignUp( rayGenSize, baseAlignment ) };
        const core::u64 hitOffset{ memory::AlignUp( missOffset + missSize, baseAlignment ) };
        const core::u64 bufferSize{ memory::AlignUp( hitOffset + hitSize, baseAlignment ) };

        auto buffer{ mDevice->CreateBuffer( rhi::BufferCreateDescription{}
            .SetName( "ShaderTable" )
            .SetByteSize( bufferSize + baseAlignment - 1 )
            .SetHeapType( rhi::HeapType::eUpload )
            .SetResourceType( rhi::ResourceType::eRawBuffer_SRV )
            .SetBufferUsage( rhi::BufferUsageFlagsBits::ShaderBindingTable ) ) };
        const auto address{ memory::AlignUp( buffer->GetGpuDeviceAddress(), baseAlignment ) };

        eastl::vector<core::u8> records( core::as<core::usize>( bufferSize ), 0 );

        // Each selected record holds its pipeline's immutable shader identifier.
        std::memcpy( records.data(), shaderHandles.data(), handleSize );

        for ( core::u32 index{}; index < mDescription.mMissRecordCount; ++index ) {
            std::memcpy( records.data() + missOffset + index * stride, shaderHandles.data() + handleSize, handleSize );
        }

        for ( core::u32 index{}; index < mDescription.mHitGroupRecordCount; ++index ) {
            std::memcpy( records.data() + hitOffset + index * stride, shaderHandles.data() + 2 * handleSize, handleSize );
        }

        // Upload heaps are written immediately through the command-list interface.
        core::u64 fenceValue{ 0 };
        renderer::rhi::FenceHandle fence{ mDevice->CreateFence( fenceValue++ ) };
        CommandListHandle commands{ mDevice->CreateCommandList( rhi::QueueType::eTransfer ) };

        commands->Open( {} );
        commands->Write( buffer.GetPtr(), core::as<core::usize>( address - buffer->GetGpuDeviceAddress() ), records.data(), records.size() );
        commands->Close();

        // Signal fenceValue on one fence on completion of these
        // commands, then we wait for that completion this blocks the caller
        // but client should ideally offload this task to worker threads
        const auto submitInfo{ renderer::rhi::SubmitInfo{}
            .AddSignal( fence, fenceValue )
            .AddCommandList( commands ) };
        mDevice->GetQueue( renderer::rhi::QueueType::eTransfer )->ExecuteCommandLists( submitInfo );
        ( void )fence->Wait( fenceValue, eastl::numeric_limits<core::u64>::max() );

        mSbtBuffer = eastl::move( buffer );
        mRayGenRegion = { address, rayGenSize };
        mMissRegion = mDescription.mMissRecordCount ? 
            D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE{ address + missOffset, missSize, stride } : 
            D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE{};

        mHitGroupsRegion = mDescription.mHitGroupRecordCount ? 
            D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE{ address + hitOffset, hitSize, stride } : 
            D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE{};

        mCallableRegion = {};

        mIsAllocated = true;
    }

    auto CommandList::BindShaderTable( rhi::IShaderTable* table ) -> void {
        MKT_BEGIN_PROFILER();
        RetainResource( table );

        mBoundShaderTable = table;
    }

    auto CommandList::DispatchRays( const rhi::RayTracingDispatchDescription& desc ) -> void {
        MKT_BEGIN_PROFILER();
        auto& table{ *core::checked_cast<ShaderTable*>( mBoundShaderTable ) };

        RetainResource( table.GetBuffer().GetPtr() );

        D3D12_DISPATCH_RAYS_DESC dispatch{};
        dispatch.RayGenerationShaderRecord = table.GetRayGenerationRegion();
        dispatch.MissShaderTable = table.GetMissRegion();
        dispatch.HitGroupTable = table.GetHitGroupRegion();
        dispatch.CallableShaderTable = table.GetCallableRegion();

        dispatch.Width = desc.mWidth;
        dispatch.Height = desc.mHeight;
        dispatch.Depth = desc.mDepth;

        mCurrentRecordingContext->mCommandList.Get()->DispatchRays( &dispatch );
    }

    auto CommandList::PrepareRayTracing( renderer::rhi::DeviceObject& resource ) -> ID3D12GraphicsCommandList7* {
        if ( GetOwningDevice() != resource.GetOwningDevice() || !mRecording ||
             mIsRenderScopeActive || GetQueueType() != renderer::rhi::QueueType::eGraphics ) {
            MKT_THROW_RUNTIME_ERROR( "Ray tracing requires a recording graphics command list outside a render pass." );
        }

        RetainResource( &resource );
        CommitBarriers();
        return mCurrentRecordingContext->mCommandList.Get();
    }

    auto Device::SupportsBCTextureCompression() const -> bool {
        return IsFeatureSupported( rhi::GpuFeatureSupportFlagsBits::BCTextureCompression );
    }

    auto Device::SupportsRayTracing() const -> bool {
        // D3D12 exposes supported features automatically; honor the engine's request policy as well.
        return IsFeatureRequested( rhi::GpuFeatureSupportFlagsBits::HardwareRayTracing ) &&
               IsFeatureSupported( rhi::GpuFeatureSupportFlagsBits::HardwareRayTracing );
    }

    auto Device::CreateAccelStructure( const renderer::rhi::AccelStructureCreateDescription& description ) -> renderer::rhi::AccelStructureHandle {
        if ( !SupportsRayTracing() ) {
            MKT_THROW_RUNTIME_ERROR( "Hardware ray tracing was not requested or is unsupported on this D3D12 device." );
        }

        renderer::rhi::AccelStructureHandle result{ core::Ref<AccelerationStructure>::New( description ) };
        result->Initialize( this );
        return result;
    }

    auto Device::CreatePipeline( const renderer::rhi::RayTracingPipelineDescription& description ) -> renderer::rhi::RayTracingPipelineHandle {
        if ( !SupportsRayTracing() ) {
            MKT_THROW_RUNTIME_ERROR( "Hardware ray tracing was not requested or is unsupported on this D3D12 device." );
        }

        renderer::rhi::RayTracingPipelineHandle result{ core::Ref<RayTracingPipeline>::New( description ) };
        result->Initialize( this );
        return result;
    }

    auto DescriptorTable::GetBindingLayout() const -> rhi::BindingLayoutHandle {
        return mBindingLayout;
    }

    auto DescriptorTable::ReleaseIndex( rhi::DescriptorTableIndex index, rhi::ResourceType type, core::u32 slot ) -> void {
        UnRegisterIndex( index, type, slot );
    }

    auto Queue::GetType() const -> rhi::QueueType {
        return mType;
    }

    auto Queue::GetOpSupportFlags() const -> rhi::QueueOpSupportFlags {
        return mOpSupportFlags;
    }

    auto CommandList::GetQueueType() const -> rhi::QueueType {
        return mQueueType;
    }
}// namespace mikoto::renderer::d3d12
#endif
