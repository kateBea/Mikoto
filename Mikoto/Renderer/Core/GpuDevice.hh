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

#ifndef MIKOTO_GPU_DEVICE_HH
#define MIKOTO_GPU_DEVICE_HH

#include <EASTL/span.h>
#include <EASTL/memory.h>
#include <EASTL/unique_ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Filesystem/Path.hh>

#include <Renderer/Core/Rhi.hh>

namespace mikoto::renderer {

    using namespace mikoto::filesystem;
    using namespace mikoto::renderer::rhi;

    enum class GpuDeviceType {
        eInvalid = -1,
        eDiscrete,
        eIntegrated,
        eSoftwareRasterizer
    };

    struct GpuFeatureSupport {
        // Properties we want the device to support
        // used to pick the appropriate physical device
        bool mChooseDiscreteDevice{ true };
        bool mAnisotropicFiltering{ true };
        bool mHardwareWireframe{ true };
        bool mEnablePresentation{ true };

        GpuDeviceType mDeviceType{ GpuDeviceType::eDiscrete };
    };

    struct GpuDeviceCreateInfo {
        GraphicsAPI mApi{ GraphicsAPI::eVulkan };
        GpuFeatureSupport mFeaturesSupport{};
    };

    class GpuDevice {
    public:
        virtual auto Init() -> void = 0;
        virtual auto Shutdown() -> void = 0;

        MKT_NODISCARD virtual auto CreateTexture( const TextureCreateDescription& description ) -> TextureHandle = 0;
        MKT_NODISCARD virtual auto CreateTextureNative( ObjectType type, Object object, const TextureCreateDescription& description ) -> TextureHandle = 0;

        MKT_NODISCARD virtual auto CreateFrameBuffer(const FramebufferDescription& description) -> FramebufferHandle = 0;

        MKT_NODISCARD virtual auto CreateBuffer( const BufferCreateDescription& description ) -> BufferHandle = 0;

        MKT_NODISCARD virtual auto CreateSampler( const SamplerCreateDescription& description ) -> SamplerHandle = 0;

        MKT_NODISCARD virtual auto CreatePipeline( const ComputePipelineDescription& description ) -> PipelineHandle = 0;
        MKT_NODISCARD virtual auto CreatePipeline( const GraphicsPipelineDescription& description ) -> PipelineHandle = 0;

        MKT_NODISCARD virtual auto CreateAccelStructure( const AccelStructureCreateDescription& description ) -> AccelStructureHandle = 0;

        MKT_NODISCARD virtual auto CreateCommandList( QueueType queue ) -> CommandListHandle = 0;
        MKT_NODISCARD virtual auto CreateCommandList( const CommandListParameters& parameters ) -> CommandListHandle = 0;

        MKT_NODISCARD virtual auto CreateShader( const ShaderModuleCreateDescription& desc ) -> ShaderModuleHandle = 0;
        MKT_NODISCARD virtual auto CreateShader( ShaderStage type, const void* code, size_t codeSize ) -> ShaderModuleHandle = 0;

        MKT_NODISCARD virtual auto CreateInputLayout(const InputLayoutCreateDescription& desc) -> InputLayoutHandle = 0;

        MKT_NODISCARD virtual auto CreateFence( u64 fenceInitialValue ) -> FenceHandle = 0;

        // For data read back, user will probably need to handle synchronization externally
        // or have the command lists do it. Map returns a buffer we can copy data from
        // writes need to be done via command lists.
        virtual auto UnMap( IBuffer* buffer ) -> void = 0;
        MKT_NODISCARD virtual auto Map(IBuffer* buffer) -> const void* = 0;

        // For backends that support it this allows us to create the layout from shader reflection
        MKT_NODISCARD virtual auto CreateBindingLayout( const BindingLayoutDescription& desc ) -> BindingLayoutHandle = 0;
        MKT_NODISCARD virtual auto CreatePipelineLayout( const PipelineLayoutCreateDescription& desc ) -> PipelineLayoutHandle = 0;
        MKT_NODISCARD virtual auto CreateBindingSet( const BindingSetDescription& desc, BindingLayoutHandle layout ) -> BindingSetHandle = 0;

        // To support bindless techniques in modern graphics APIs
        MKT_NODISCARD virtual auto CreateBindlessLayout( const BindlessLayoutDescription& desc ) -> BindingLayoutHandle = 0;

        MKT_NODISCARD virtual auto CreateDescriptorTable( BindingLayoutHandle layout ) -> DescriptorTableHandle = 0;
        MKT_NODISCARD virtual auto ResizeDescriptorTable( DescriptorTableHandle descriptorTable, u32 newSize, bool keepContents ) -> bool = 0;
        MKT_NODISCARD virtual auto WriteDescriptorTable( DescriptorTableHandle descriptorTable, const BindingSetItem& item ) -> bool = 0;

        // Synchronization
        virtual auto Wait(FenceHandle handle, u64 fenceValue) -> void = 0;

        // Rework to account for sync objects
        virtual auto Flush() -> void = 0;
        virtual auto RunGarbageCollection() -> void = 0;
        virtual auto SubmitCommands( CommandListHandle cmdList ) -> u64 = 0;
        virtual auto ExecuteCommands( CommandListHandle cmdList ) -> void = 0;

        virtual auto WaitIdle() -> void = 0;

        MKT_NODISCARD auto IsInitialized() const -> bool;
        MKT_NODISCARD auto GetGraphicsApi() const -> GraphicsAPI;
        MKT_NODISCARD auto GetDeviceName() const -> eastl::string_view;

        virtual ~GpuDevice() = default;

        MKT_NODISCARD static auto Create( const GpuDeviceCreateInfo& createInfo ) -> eastl::unique_ptr<GpuDevice>;

    protected:
        explicit GpuDevice( GraphicsAPI api, const GpuFeatureSupport& featuresSupport );

    protected:
        eastl::string mName{};

        GraphicsAPI mApi{};
        GpuFeatureSupport mFeaturesSupport{};

        bool mIsInitialized{ false };
    };
}// namespace mikoto::renderer

#endif//MIKOTO_GPU_DEVICE_HH
