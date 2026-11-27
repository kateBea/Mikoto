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

#include <cstring>

#include <EASTL/algorithm.h>

#include <Core/Exception.hh>

#include <Renderer/Rhi/Vulkan/VulkanDevice.hh>
#include <Renderer/Rhi/Vulkan/VulkanHelpers.hh>
#include <Renderer/Rhi/Vulkan/VulkanVideoDecoder.hh>

namespace mikoto::renderer::vulkan {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    // Adapt session setup and H.264 parameter conversion to Mikoto resource ownership.
    // https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/wiGraphicsDevice_Vulkan.cpp
    // https://docs.vulkan.org/spec/latest/chapters/videocoding.html
    static auto FindVideoMemory( const PhysicalDevice &device, u32 bits, VkMemoryPropertyFlags properties ) -> u32 {
        for ( u32 index{}; index < device.mMemoryProperties.memoryTypeCount; ++index ) {
            if ( ( bits & ( 1u << index ) ) &&
                 ( device.mMemoryProperties.memoryTypes[index].propertyFlags & properties ) == properties ) {
                return index;
            }
        }

        MKT_THROW_RUNTIME_ERROR( "No compatible memory type for Vulkan video resources." );
    }

    static auto H264Level( int level ) -> StdVideoH264LevelIdc {
        constexpr int levels[]{ 10, 11, 12, 13, 20, 21, 22, 30, 31, 32, 40, 41, 42, 50, 51, 52, 60, 61, 62 };

        for ( u32 index{}; index < eastl::size( levels ); ++index ) {
            if ( levels[index] == level ) {
                return static_cast<StdVideoH264LevelIdc>( index );
            }
        }

        MKT_THROW_RUNTIME_ERROR( "Unsupported H.264 level." );
    }

    static auto MakeSequenceParameters( const h264::SPS &source, StdVideoH264ScalingLists &scaling ) -> StdVideoH264SequenceParameterSet {
        StdVideoH264SequenceParameterSet result{};
        result.flags.constraint_set0_flag = source.constraint_set0_flag;
        result.flags.constraint_set1_flag = source.constraint_set1_flag;
        result.flags.constraint_set2_flag = source.constraint_set2_flag;
        result.flags.constraint_set3_flag = source.constraint_set3_flag;
        result.flags.constraint_set4_flag = source.constraint_set4_flag;
        result.flags.constraint_set5_flag = source.constraint_set5_flag;
        result.flags.direct_8x8_inference_flag = source.direct_8x8_inference_flag;
        result.flags.frame_mbs_only_flag = source.frame_mbs_only_flag;
        result.flags.delta_pic_order_always_zero_flag = source.delta_pic_order_always_zero_flag;
        result.flags.frame_cropping_flag = source.frame_cropping_flag;
        result.flags.seq_scaling_matrix_present_flag = source.seq_scaling_matrix_present_flag;
        result.profile_idc = static_cast<StdVideoH264ProfileIdc>( source.profile_idc );
        result.level_idc = H264Level( source.level_idc );
        result.chroma_format_idc = STD_VIDEO_H264_CHROMA_FORMAT_IDC_420;
        result.seq_parameter_set_id = source.seq_parameter_set_id;
        result.log2_max_frame_num_minus4 = source.log2_max_frame_num_minus4;
        result.pic_order_cnt_type = static_cast<StdVideoH264PocType>( source.pic_order_cnt_type );
        result.log2_max_pic_order_cnt_lsb_minus4 = source.log2_max_pic_order_cnt_lsb_minus4;
        result.offset_for_non_ref_pic = source.offset_for_non_ref_pic;
        result.offset_for_top_to_bottom_field = source.offset_for_top_to_bottom_field;
        result.num_ref_frames_in_pic_order_cnt_cycle = source.num_ref_frames_in_pic_order_cnt_cycle;
        result.max_num_ref_frames = source.num_ref_frames;
        result.pic_width_in_mbs_minus1 = source.pic_width_in_mbs_minus1;
        result.pic_height_in_map_units_minus1 = source.pic_height_in_map_units_minus1;
        result.frame_crop_left_offset = source.frame_crop_left_offset;
        result.frame_crop_right_offset = source.frame_crop_right_offset;
        result.frame_crop_top_offset = source.frame_crop_top_offset;
        result.frame_crop_bottom_offset = source.frame_crop_bottom_offset;
        result.pOffsetForRefFrame = source.offset_for_ref_frame;

        if ( source.seq_scaling_matrix_present_flag ) {
            scaling.scaling_list_present_mask = 0xff;
            h264::resolve_scaling_lists( &source, nullptr, scaling.ScalingList4x4, scaling.ScalingList8x8 );
            result.pScalingLists = &scaling;
        }

        return result;
    }

    static auto MakePictureParameters( const h264::SPS &sps, const h264::PPS &source,
        StdVideoH264ScalingLists &scaling ) -> StdVideoH264PictureParameterSet {
        StdVideoH264PictureParameterSet result{};
        result.flags.transform_8x8_mode_flag = source.transform_8x8_mode_flag;
        result.flags.redundant_pic_cnt_present_flag = source.redundant_pic_cnt_present_flag;
        result.flags.constrained_intra_pred_flag = source.constrained_intra_pred_flag;
        result.flags.deblocking_filter_control_present_flag = source.deblocking_filter_control_present_flag;
        result.flags.weighted_pred_flag = source.weighted_pred_flag;
        result.flags.bottom_field_pic_order_in_frame_present_flag = source.pic_order_present_flag;
        result.flags.entropy_coding_mode_flag = source.entropy_coding_mode_flag;
        result.flags.pic_scaling_matrix_present_flag = source.pic_scaling_matrix_present_flag;
        result.seq_parameter_set_id = source.seq_parameter_set_id;
        result.pic_parameter_set_id = source.pic_parameter_set_id;
        result.num_ref_idx_l0_default_active_minus1 = source.num_ref_idx_l0_active_minus1;
        result.num_ref_idx_l1_default_active_minus1 = source.num_ref_idx_l1_active_minus1;
        result.weighted_bipred_idc = static_cast<StdVideoH264WeightedBipredIdc>( source.weighted_bipred_idc );
        result.pic_init_qp_minus26 = source.pic_init_qp_minus26;
        result.pic_init_qs_minus26 = source.pic_init_qs_minus26;
        result.chroma_qp_index_offset = source.chroma_qp_index_offset;
        result.second_chroma_qp_index_offset = source.second_chroma_qp_index_offset;

        if ( source.pic_scaling_matrix_present_flag ) {
            scaling.scaling_list_present_mask = 0xff;
            h264::resolve_scaling_lists( &sps, &source, scaling.ScalingList4x4, scaling.ScalingList8x8 );
            result.pScalingLists = &scaling;
        }

        return result;
    }

    static auto RequireVideoFormat( Device &device, const VkVideoProfileInfoKHR &profile, VkImageUsageFlags usage ) -> void {
        auto profiles{ initializers::VideoProfileListInfoKHR() };
        profiles.profileCount = 1;
        profiles.pProfiles = &profile;

        auto info{ initializers::PhysicalDeviceVideoFormatInfoKHR() };
        info.pNext = &profiles;
        info.imageUsage = usage;

        u32 count{};
        MKT_VK_CHECK( vkGetPhysicalDeviceVideoFormatPropertiesKHR( device.GetPhysicalDevice()->mPhysicalDevice, &info, &count, nullptr ) );

        eastl::vector<VkVideoFormatPropertiesKHR> formats( count, initializers::VideoFormatPropertiesKHR() );
        MKT_VK_CHECK( vkGetPhysicalDeviceVideoFormatPropertiesKHR( device.GetPhysicalDevice()->mPhysicalDevice, &info, &count, formats.data() ) );

        if ( eastl::none_of( formats.begin(), formats.end(), []( const VkVideoFormatPropertiesKHR &format ) {
                 return format.format == VK_FORMAT_G8_B8R8_2PLANE_420_UNORM && format.imageTiling == VK_IMAGE_TILING_OPTIMAL;
             } ) ) {
            MKT_THROW_RUNTIME_ERROR( "Device cannot decode this H.264 profile into sampleable NV12 images." );
        }
    }

    VideoImageStorage::VideoImageStorage( const VkVideoProfileInfoKHR &profile, VkExtent2D extent, VkImageUsageFlags usage )
        : mH264Profile{ *static_cast<const VkVideoDecodeH264ProfileInfoKHR *>( profile.pNext ) }, mProfile{ profile } {
        mH264Profile.pNext = nullptr;
        mProfile.pNext = &mH264Profile;
        mImageInfo = initializers::ImageCreateInfo();
        mImageInfo.imageType = VK_IMAGE_TYPE_2D;
        mImageInfo.format = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
        mImageInfo.extent = { extent.width, extent.height, 1 };
        mImageInfo.mipLevels = 1;
        mImageInfo.arrayLayers = 1;
        mImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        mImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        mImageInfo.usage = usage;
        if ( usage & VK_IMAGE_USAGE_SAMPLED_BIT ) {
            mImageInfo.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
        }
    }

    auto VideoImageStorage::Initialize() -> void {
        auto profiles{ initializers::VideoProfileListInfoKHR() };
        profiles.profileCount = 1;
        profiles.pProfiles = &mProfile;

        const VkFormat planeFormats[]{
            VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
            VK_FORMAT_R8_UNORM, VK_FORMAT_R8G8_UNORM
        };

        auto formats{ initializers::ImageFormatListCreateInfo() };
        formats.pNext = &profiles;
        formats.viewFormatCount = 3;
        formats.pViewFormats = planeFormats;
        mImageInfo.pNext = mImageInfo.flags ? static_cast<const void *>( &formats ) : &profiles;

        // Query the actual video image before binding its memory, as in the Vulkan samples.
        // https://github.com/KhronosGroup/Vulkan-Tutorial/blob/main/attachments/24_texture_image.cpp
        auto *device{ core::checked_cast<Device *>( mDevice ) };

        MKT_VK_CHECK( vkCreateImage( device->GetDevice(), &mImageInfo, nullptr, &mImage ) );

        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements( device->GetDevice(), mImage, &requirements );

        auto dedicated{ initializers::MemoryDedicatedAllocateInfo() };
        dedicated.image = mImage;

        auto memory{ initializers::MemoryAllocateInfo() };
        memory.pNext = &dedicated;
        memory.allocationSize = requirements.size;
        memory.memoryTypeIndex = FindVideoMemory( *device->GetPhysicalDevice(), requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

        MKT_VK_CHECK( vkAllocateMemory( device->GetDevice(), &memory, nullptr, &mMemory ) );
        MKT_VK_CHECK( vkBindImageMemory( device->GetDevice(), mImage, mMemory, 0 ) );

        mImageInfo.pNext = nullptr;
        mIsAllocated = true;
    }

    auto VideoImageStorage::Destroy() -> void {
        if ( mImage ) {
            auto device{ core::checked_cast<Device *>( mDevice )->GetDevice() };
            vkDestroyImage( device, mImage, nullptr );
            vkFreeMemory( device, mMemory, nullptr );
            mMemory = VK_NULL_HANDLE;
            mImage = VK_NULL_HANDLE;
        }
    }

    VideoImageStorage::~VideoImageStorage() {
        Destroy();
    }

    auto VideoImageStorage::GetImage() const -> VkImage {
        return mImage;
    }


    VideoSurface::VideoSurface( const VkVideoProfileInfoKHR &profile, VkExtent2D extent, bool reference, bool coincident )
        : mExtent{ extent }, mSampled{ !reference || coincident } {
        mUsage = reference ? VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR : VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR;

        if ( mSampled ) {
            mUsage |= VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR | VK_IMAGE_USAGE_SAMPLED_BIT;
        }

        mStorage = Ref<VideoImageStorage>::New( profile, extent, mUsage );
    }

    auto VideoSurface::Initialize() -> void {
        static_cast<DeviceObject *>( mStorage.GetPtr() )->Initialize( mDevice );
        auto *device{ core::checked_cast<Device *>( mDevice ) };
        auto usage{ initializers::ImageViewUsageCreateInfo() };
        usage.usage = mUsage & ~VK_IMAGE_USAGE_SAMPLED_BIT;

        auto view{ initializers::ImageViewCreateInfo() };
        view.pNext = &usage;
        view.image = mStorage->GetImage();
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
        view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        ExternalTextureDescription description{
            .mWidth = mExtent.width,
            .mHeight = mExtent.height,
            .mSurfaceFormat = Format::eNV12,
            .mImageViewCreateInfo = view,
            .mUsage = TextureUsageFlagsBits::None,
            .mOwner = mStorage
        };

        mImage = device->CreateTexture( description );

        // Plane-compatible views use ordinary sampled-image descriptors and shared samplers.
        // https://docs.vulkan.org/spec/latest/chapters/formats.html#formats-compatible-planes
        if ( mSampled ) {
            usage.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
            description.mUsage = TextureUsageFlagsBits::ShaderResource;
            description.mSurfaceFormat = Format::eR8_UNORM;
            description.mImageViewCreateInfo.format = VK_FORMAT_R8_UNORM;
            description.mImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_PLANE_0_BIT;
            mLumaImage = device->CreateTexture( description );

            description.mWidth /= 2;
            description.mHeight /= 2;
            description.mSurfaceFormat = Format::eRG8_UNORM;
            description.mImageViewCreateInfo.format = VK_FORMAT_R8G8_UNORM;
            description.mImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_PLANE_1_BIT;
            mChromaImage = device->CreateTexture( description );
        }

        mIsAllocated = true;
    }

    auto VideoSurface::Destroy() -> void {
        mLumaImage.Reset();
        mChromaImage.Reset();
        mImage.Reset();
        mStorage.Reset();
    }

    VideoSurface::~VideoSurface() {
        Destroy();
    }

    auto VideoSurface::GetImage() const -> TextureHandle {
        return mImage;
    }

    auto VideoSurface::GetLumaImage() const -> TextureHandle {
        return mLumaImage;
    }

    auto VideoSurface::GetChromaImage() const -> TextureHandle {
        return mChromaImage;
    }

    VideoDecoder::VideoDecoder( const VideoDecoderDescription &description )
        : mConfiguration{ description.mConfiguration } {
    }

    auto VideoDecoder::Initialize() -> void {
        auto *device{ core::checked_cast<Device *>( mDevice ) };
        const auto &source{ mConfiguration->GetSps() };

        if ( source.vui.colour_description_present_flag && source.vui.transfer_characteristics != 1 &&
             source.vui.transfer_characteristics != 2 && source.vui.transfer_characteristics != 6 && source.vui.transfer_characteristics != 13 ) {
            MKT_THROW_RUNTIME_ERROR( "This video transfer function is not supported by the SDR presentation path." );
        }

        mH264Profile = initializers::VideoDecodeH264ProfileInfoKHR();
        mH264Profile.stdProfileIdc = static_cast<StdVideoH264ProfileIdc>( source.profile_idc );
        // This profile also accepts progressive frames. Prefer mini_video's layout to
        // avoid the NVIDIA submit-time crash reproduced with progressive-only sessions.
        // https://github.com/turanszkij/mini_video/blob/master/mini_video_vulkan.cpp
        // https://github.khronos.org/Vulkan-Site/features/latest/features/proposals/VK_KHR_video_decode_h264.html#_are_interlaced_frames_supported
        mH264Profile.pictureLayout = VK_VIDEO_DECODE_H264_PICTURE_LAYOUT_INTERLACED_INTERLEAVED_LINES_BIT_KHR;

        mProfile = initializers::VideoProfileInfoKHR();
        mProfile.pNext = &mH264Profile;
        mProfile.videoCodecOperation = VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR;
        mProfile.chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR;
        mProfile.lumaBitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
        mProfile.chromaBitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;

        mH264Capabilities = initializers::VideoDecodeH264CapabilitiesKHR();
        mDecodeCapabilities = initializers::VideoDecodeCapabilitiesKHR();
        mDecodeCapabilities.pNext = &mH264Capabilities;
        mCapabilities = initializers::VideoCapabilitiesKHR();
        mCapabilities.pNext = &mDecodeCapabilities;

        const auto physicalDevice{ device->GetPhysicalDevice()->mPhysicalDevice };
        auto capabilityResult{ vkGetPhysicalDeviceVideoCapabilitiesKHR( physicalDevice, &mProfile, &mCapabilities ) };

        if ( capabilityResult == VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR ) {
            // Keep progressive-only implementations usable when interleaved pictures are unavailable.
            mH264Profile.pictureLayout = VK_VIDEO_DECODE_H264_PICTURE_LAYOUT_PROGRESSIVE_KHR;
            capabilityResult = vkGetPhysicalDeviceVideoCapabilitiesKHR( physicalDevice, &mProfile, &mCapabilities );
        }

        MKT_VK_CHECK( capabilityResult );

        mExtent = { static_cast<u32>( source.pic_width_in_mbs_minus1 + 1 ) * 16,
            static_cast<u32>( source.pic_height_in_map_units_minus1 + 1 ) * 16 };
        mMaxReferences = eastl::max( 1u, static_cast<u32>( source.num_ref_frames ) );

        // Some adapters only support decoding directly into the reconstructed DPB image.
        // Wicked uses this same coincident output path; keep presentation on the GPU.
        // https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/wiGraphicsDevice_Vulkan.cpp
        mIsCoincident = ( mDecodeCapabilities.flags & VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_COINCIDE_BIT_KHR ) != 0;
        mDpbSlotCount = eastl::min( mCapabilities.maxDpbSlots, mMaxReferences + ( mIsCoincident ? 8u : 1u ) );

        if ( ( !mIsCoincident && !( mDecodeCapabilities.flags & VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_DISTINCT_BIT_KHR ) ) ||
             !( mCapabilities.flags & VK_VIDEO_CAPABILITY_SEPARATE_REFERENCE_IMAGES_BIT_KHR ) ||
             mMaxReferences > mCapabilities.maxActiveReferencePictures || mMaxReferences + 1 > mCapabilities.maxDpbSlots ||
             mExtent.width < mCapabilities.minCodedExtent.width || mExtent.height < mCapabilities.minCodedExtent.height ||
             mExtent.width > mCapabilities.maxCodedExtent.width || mExtent.height > mCapabilities.maxCodedExtent.height ||
             H264Level( source.level_idc ) > mH264Capabilities.maxLevelIdc ) {
            MKT_THROW_RUNTIME_ERROR( "H.264 profile, extent or independently allocated reference images are not supported by this device." );
        }

        RequireVideoFormat( *device, mProfile, VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR | VK_IMAGE_USAGE_SAMPLED_BIT | ( mIsCoincident ? VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR : 0 ) );
        RequireVideoFormat( *device, mProfile, VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR );

        auto session{ initializers::VideoSessionCreateInfoKHR() };
        session.queueFamilyIndex = core::checked_cast<Queue *>( device->GetQueue( QueueType::eVideoDecode ) )->GetFamilyIndex();
        session.pVideoProfile = &mProfile;
        session.pictureFormat = session.referencePictureFormat = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
        session.maxCodedExtent = mExtent;
        session.maxDpbSlots = mDpbSlotCount;
        session.maxActiveReferencePictures = mMaxReferences;
        session.pStdHeaderVersion = &mCapabilities.stdHeaderVersion;

        MKT_VK_CHECK( vkCreateVideoSessionKHR( device->GetDevice(), &session, nullptr, &mSession ) );

        u32 count{};
        MKT_VK_CHECK( vkGetVideoSessionMemoryRequirementsKHR( device->GetDevice(), mSession, &count, nullptr ) );
        eastl::vector<VkVideoSessionMemoryRequirementsKHR> requirements( count, initializers::VideoSessionMemoryRequirementsKHR() );
        MKT_VK_CHECK( vkGetVideoSessionMemoryRequirementsKHR( device->GetDevice(), mSession, &count, requirements.data() ) );
        eastl::vector<VkBindVideoSessionMemoryInfoKHR> bindings( count, initializers::BindVideoSessionMemoryInfoKHR() );
        mSessionMemory.resize( count );

        // Prefer device-local storage while respecting each binding's memory requirements.
        auto *allocator{ device->GetAllocator() };
        VmaAllocationCreateInfo allocation{};
        allocation.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        for ( u32 index{}; index < count; ++index ) {
            const auto &requirement{ requirements[index] };
            VmaAllocationInfo info{};

            MKT_VK_CHECK( allocator->AllocateMemory( requirement.memoryRequirements, allocation, mSessionMemory[index], info ) );

            bindings[index].memoryBindIndex = requirement.memoryBindIndex;
            bindings[index].memory = info.deviceMemory;
            bindings[index].memoryOffset = info.offset;
            bindings[index].memorySize = requirement.memoryRequirements.size;
        }

        MKT_VK_CHECK( vkBindVideoSessionMemoryKHR( device->GetDevice(), mSession, count, bindings.data() ) );

        StdVideoH264ScalingLists spsScaling{}, ppsScaling{};
        const auto sps{ MakeSequenceParameters( source, spsScaling ) };
        const auto pps{ MakePictureParameters( source, mConfiguration->GetPps(), ppsScaling ) };

        auto add{ initializers::VideoDecodeH264SessionParametersAddInfoKHR() };
        add.stdSPSCount = 1;
        add.pStdSPSs = &sps;
        add.stdPPSCount = 1;
        add.pStdPPSs = &pps;

        auto h264{ initializers::VideoDecodeH264SessionParametersCreateInfoKHR() };
        h264.maxStdSPSCount = 1;
        h264.maxStdPPSCount = 1;
        h264.pParametersAddInfo = &add;

        auto parameters{ initializers::VideoSessionParametersCreateInfoKHR() };
        parameters.pNext = &h264;
        parameters.videoSession = mSession;

        MKT_VK_CHECK( vkCreateVideoSessionParametersKHR( device->GetDevice(), &parameters, nullptr, &mParameters ) );

        mIsAllocated = true;
    }

    auto VideoDecoder::Destroy() -> void {
        if ( !mDevice ) {
            return;
        }

        const auto device{ core::checked_cast<Device *>( mDevice )->GetDevice() };
        vkDestroyVideoSessionParametersKHR( device, mParameters, nullptr );
        vkDestroyVideoSessionKHR( device, mSession, nullptr );

        auto *allocator{ core::checked_cast<Device *>( mDevice )->GetAllocator() };

        for ( const auto allocation: mSessionMemory ) {
            allocator->FreeMemory( allocation );
        }

        mSessionMemory.clear();
    }

    VideoDecoder::~VideoDecoder() {
        Destroy();
    }

    auto VideoDecoder::CreateSurface( bool reference ) -> VideoSurfaceHandle {
        VideoSurfaceHandle surface{ Ref<VideoSurface>::New( mProfile, mExtent, reference, mIsCoincident ) };
        surface->Initialize( mDevice );
        return surface;
    }

    auto VideoDecoder::CreateBitstream( eastl::span<const ubyte> bytes ) -> BufferHandle {
        auto *device{ core::checked_cast<Device *>( mDevice ) };
        auto buffer{ device->CreateBuffer( BufferCreateDescription{}
            .SetName( "Video clip bitstream" )
            .SetByteSize( bytes.size() )
            .SetHeapType( HeapType::eUpload )
            .SetCpuAccessType( AccessType::eWrite )
            .SetResourceType( ResourceType::eRawBuffer_SRV )
            .SetBufferUsage( BufferUsageFlagsBits::VideoDecodeSource ),
            mProfile ) };
        auto *mapped{ mDevice->Map( buffer.GetPtr() ) };

        if ( !mapped ) {
            MKT_THROW_RUNTIME_ERROR( "Cannot map the video bitstream upload buffer." );
        }

        std::memcpy( mapped, bytes.data(), bytes.size() );
        mDevice->UnMap( buffer.GetPtr() );
        return buffer;
    }

    auto VideoDecoder::GetBitstreamLayout() const -> VideoBitstreamLayout {
        return {
            .mOffsetAlignment = mCapabilities.minBitstreamBufferOffsetAlignment,
            .mSizeAlignment = mCapabilities.minBitstreamBufferSizeAlignment,
            .mStartCodeSize = 4
        };
    }

    auto VideoDecoder::MatchesConfiguration( const video::H264Picture &picture ) const -> bool {
        return std::memcmp( &picture.GetSps(), &mConfiguration->GetSps(), sizeof( h264::SPS ) ) == 0 &&
               std::memcmp( &picture.GetPps(), &mConfiguration->GetPps(), sizeof( h264::PPS ) ) == 0;
    }

    auto VideoDecoder::GetMaxReferences() const -> u32 {
        return mMaxReferences;
    }

    auto VideoDecoder::GetDpbSlotCount() const -> u32 {
        return mDpbSlotCount;
    }

    auto VideoDecoder::UsesCoincidentOutput() const -> bool {
        return mIsCoincident;
    }

    auto VideoDecoder::GetSession() const -> VkVideoSessionKHR {
        return mSession;
    }

    auto VideoDecoder::GetParameters() const -> VkVideoSessionParametersKHR {
        return mParameters;
    }

    auto VideoDecoder::GetExtent() const -> VkExtent2D {
        return mExtent;
    }
}// namespace mikoto::renderer::vulkan