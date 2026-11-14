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

#ifndef MIKOTO_RENDERER_RHI_OPENGL_OPENGL_DEVICE_HH
#define MIKOTO_RENDERER_RHI_OPENGL_OPENGL_DEVICE_HH

#include <mutex>
#include <memory>
#include <thread>

#include <EASTL/atomic.h>
#include <EASTL/vector.h>
#include <EASTL/functional.h>
#include <EASTL/shared_ptr.h>

#include <glad/gl.h>
#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Rhi/GpuDevice.hh>

namespace mikoto::renderer::opengl {

    class Pipeline;
    class QueryPool;

    // GL work is submitted on the thread owning the current context. Worker
    // threads may record command lists and release resources, but do not call GL.
    struct DeviceState {
        std::thread::id mOwner{ std::this_thread::get_id() };
        eastl::atomic<bool> mAlive{ true };
        eastl::atomic<core::usize> mAllocatedBytes{};

        // Resources released by workers are deleted on the context thread.
        std::mutex mDeletionMutex{};
        eastl::vector<eastl::function<void()>> mDeletions{};


        auto RequireOwner() const -> void;
        auto Retire( eastl::function<void()> deletion ) -> void;
        auto Collect() -> void;
    };

    class Fence final : public renderer::rhi::IFence {
    public:
        Fence( core::u64 initial, eastl::shared_ptr<DeviceState> state );

        MKT_NODISCARD auto GetCompletionValue() const -> core::u64 override;
        MKT_NODISCARD auto Signal( core::u64 value ) -> bool override;
        MKT_NODISCARD auto Wait( core::u64 value, core::u64 timeoutMs ) -> bool override;

        auto WaitOnGpu( core::u64 value ) -> void;

        MKT_NODISCARD auto SignalOnGpu( core::u64 value ) -> bool;

        ~Fence() override;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        struct Pending {
            core::u64 mValue{};
            GLsync mSync{};
        };

    private:
        // Context ownership and native resource state.
        eastl::shared_ptr<DeviceState> mState{};
        mutable eastl::vector<Pending> mPending{};
        mutable eastl::atomic<core::u64> mCompleted{};

        core::u64 mLastSignal{};
    };

    class BindingLayout final : public renderer::rhi::IBindingLayout {
    public:
        explicit BindingLayout( const renderer::rhi::BindingLayoutDescription &desc );

        auto GetRegisterSpace() const -> core::u32 override;

        MKT_NODISCARD auto IsBindless() const -> bool override;

        MKT_NODISCARD auto GetDescription() const -> const rhi::BindingLayoutDescription&;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        rhi::BindingLayoutDescription mDescription{};
    };

    // Explicit GLSL bindings are global, with no descriptor-set namespace.
    // Different sets must use disjoint binding numbers in the same GL namespace.
    class BindingTable final : public renderer::rhi::IBindingTable {
    public:
        BindingTable( const renderer::rhi::BindingTableDescription &desc, renderer::rhi::BindingLayoutHandle layout );

        auto Update( const renderer::rhi::BindingTableDescription &desc ) -> void;

        auto Bind() const -> void;
        auto Find( renderer::rhi::ResourceType type, core::u32 slot ) const -> const renderer::rhi::BindingTableItem *;

        auto Snapshot() const -> core::Ref<BindingTable>;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        rhi::BindingLayoutHandle mLayout{};
        rhi::BindingTableDescription mDesc{};
        eastl::vector<core::Ref<core::Resource>> mRetained{};
    };

    class PipelineLayout final : public renderer::rhi::IPipelineLayout {
    public:
        explicit PipelineLayout( const renderer::rhi::PipelineLayoutCreateDescription &desc );

        MKT_NODISCARD auto GetDescription() const -> const rhi::PipelineLayoutCreateDescription&;

        MKT_NODISCARD static auto GetPushConstantBinding() -> GLuint;
    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        rhi::PipelineLayoutCreateDescription mDescription{};

        // Reserved UBO binding. Portable GLSL declares layout(std140, binding=15).
        static constexpr GLuint kPushConstantBinding{ 15 };
    };

    using BindingTableSnapshots = eastl::vector<eastl::pair<core::u32, core::Ref<BindingTable>>>;

    class Queue final : public renderer::rhi::IQueue {
    public:
        Queue( renderer::rhi::QueueType type, eastl::shared_ptr<DeviceState> state );

        MKT_NODISCARD auto GetType() const -> rhi::QueueType override;
        MKT_NODISCARD auto GetOpSupportFlags() const -> rhi::QueueOpSupportFlags override;

        auto ExecuteCommandLists( const renderer::rhi::SubmitInfo &info ) -> void override;

        // OpenGL specifics
        auto CollectGarbage() -> void;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        rhi::QueueType mType{ rhi::QueueType::eInvalid };
        rhi::QueueOpSupportFlags mOpSupportFlags{ rhi::QueueOpSupportFlagsBits::Graphics };

        // Context ownership and native resource state.
        eastl::shared_ptr<DeviceState> mState{};
        struct PendingResources {
            GLsync mCompletion{};
        };
        eastl::vector<PendingResources> mPendingResources{};
    };

    class CommandList final : public renderer::rhi::ICommandList {
    public:
        explicit CommandList( renderer::rhi::QueueType type, eastl::shared_ptr<DeviceState> state );

        auto Open( const renderer::rhi::CommandListBeginDescription &desc ) -> void override;

        auto Close() -> void override;

        auto SetBarrier( const renderer::rhi::BarrierDescription &description ) -> void override;
        auto SetTransition( const renderer::rhi::TransitionDescription &description ) -> void override;

        auto AcquireResource( const rhi::BufferBarrierDescription& description, rhi::IQueue* sourceQueue ) -> void override;
        auto ReleaseResource( const rhi::BufferBarrierDescription& description, rhi::IQueue* destQueue ) -> void override;

        auto AcquireResource( const rhi::TextureBarrierDescription& description, rhi::IQueue* sourceQueue ) -> void override;
        auto ReleaseResource( const rhi::TextureBarrierDescription& description, rhi::IQueue* destQueue ) -> void override;

        auto SetEnableAutomaticBarriers( bool enable ) -> void override;

        auto SetClearColor( renderer::rhi::ITexture *renderTarget, renderer::rhi::Color color ) -> void override;

        auto Write( renderer::rhi::IBuffer *src, renderer::rhi::ITexture *dest ) -> void override;
        auto Write( renderer::rhi::ITexture *target, const void *data, core::usize byteSize ) -> void override;
        auto Copy( renderer::rhi::ITexture *src, const renderer::rhi::TextureSlice &srcSlice, renderer::rhi::ITexture *dest, const renderer::rhi::TextureSlice &destSlice ) -> void override;

        auto Resolve( renderer::rhi::ITexture *src, const renderer::rhi::TextureSlice &srcSlice, renderer::rhi::ITexture *dest, const renderer::rhi::TextureSlice &destSlice ) -> void override;

        auto Write( renderer::rhi::IBuffer *target, const void *data, core::usize byteSize ) -> void override;
        auto Write( renderer::rhi::IBuffer *target, core::usize destOffset, const void *data, core::usize byteSize ) -> void override;

        auto Copy( renderer::rhi::IBuffer *src, renderer::rhi::IBuffer *dest ) -> void override;
        auto Copy( renderer::rhi::IBuffer *src, renderer::rhi::IBuffer *dest, core::usize destOffset ) -> void override;
        auto Copy( renderer::rhi::IBuffer *dest, renderer::rhi::ITexture *src ) -> void override;
        auto Copy( renderer::rhi::IBuffer *dest, renderer::rhi::ITexture *src, const renderer::rhi::TextureSlice &srcSlice ) -> void override;

        auto BeginRenderPass( renderer::rhi::RenderPassDescription &state ) -> void override;
        auto EndRenderPass() -> void override;

        auto BindPipeline( renderer::rhi::IPipeline *pipeline ) -> void override;

        auto SetViewportState( const renderer::rhi::ViewportState &vs ) -> void override;
        auto SetViewport( eastl::span<const renderer::rhi::Viewport> viewports ) -> void override;
        auto SetScissors( eastl::span<const renderer::rhi::Rect> scissorRects ) -> void override;

        auto SetPolygonLineWidth( core::f32 width ) -> void override;

        auto BindIndexBuffer( renderer::rhi::IBuffer *buffer ) -> void override;
        auto BindIndirectBuffer( renderer::rhi::IBuffer *buffer ) -> void override;
        auto BindVertexBuffer( const renderer::rhi::VertexBufferBinding &binding ) -> void override;
        auto BindVertexBuffers( eastl::span<const renderer::rhi::VertexBufferBinding> binding ) -> void override;

        auto BindPipelineResources( const renderer::rhi::BindResourcesDescription &desc ) -> void override;

        auto Draw( const renderer::rhi::DrawArguments &args ) -> void override;
        auto DrawIndexed( const renderer::rhi::DrawArguments &args ) -> void override;
        auto DrawIndirect( core::u32 offset, core::u32 drawCount ) -> void override;
        auto DrawIndexedIndirect( core::u32 offset, core::u32 drawCount ) -> void override;

        auto Dispatch( core::u32 groupsX, core::u32 groupsY, core::u32 groupsZ ) -> void override;
        auto DispatchRays( const rhi::RayTracingDispatchDescription &desc ) -> void override;
        auto BindShaderTable( rhi::IShaderTable* table ) -> void override;

        auto SetPushConstants( renderer::rhi::IPipelineLayout *pipelineLayout,
            const void *data, core::usize byteSize, renderer::rhi::ShaderFlags stageVisibility ) -> void override;

        auto BeginDebugLabel( eastl::string_view name, renderer::rhi::Color color ) -> void override;

        auto EndDebugLabel() -> void override;

        // A query pool is one submission batch. Reset only after its completion
        // fence. Begin/End must use this same recording; Resolve precedes host reads.
        auto Reset( rhi::IQueryPool *pool ) -> void override;

        auto BeginQuery( rhi::IQueryPool *pool, core::u32 index ) -> void override;
        auto EndQuery( rhi::IQueryPool *pool, core::u32 index ) -> void override;

        auto WriteTimestamp( rhi::IQueryPool *pool, core::u32 index ) -> void override;
        auto Resolve( rhi::IQueryPool *pool, core::u32 first, core::u32 count ) -> void override;

        // Initial immutable build, including any unbuilt BLAS dependencies.
        auto Build( rhi::IAccelerationStructure *structure ) -> void override;
        auto GetQueueType() const -> rhi::QueueType override;

        auto Record( eastl::function<void()> command ) -> void;

        auto Execute() -> void;

        auto MarkSubmitted() -> void;
        auto ValidateSubmission() const -> void;

        ~CommandList() override;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

        auto ValidateQueryPool( rhi::IQueryPool *pool ) const -> QueryPool &;

    private:
        friend class QueryPool;

        // Recorded commands retain every resource until submission completes.
        eastl::shared_ptr<DeviceState> mState{};
        eastl::vector<eastl::function<void()>> mCommands{};

        // Recording lifecycle.
        bool mRecording{};
        bool mExecutable{};
        bool mInsidePass{};

        // Persistent recording policy; commands snapshot it individually.
        bool mAutomaticBarriers{ true };

        // Execution state is private to this list; reset at every submission.
        GLuint mFramebuffer{};
        GLuint mVertexArray{};
        GLuint mPushBuffer{};

        rhi::QueueType mQueueType{ rhi::QueueType::eInvalid };
        core::u32 mSelfManagedCommandListCount{ 4 };


        // Draw state cached while replaying this list.
        GLenum mTopology{ GL_TRIANGLES };
        GLenum mIndexType{ GL_UNSIGNED_INT };
        bool mHasPipeline{};
        bool mIndexed{};
        bool mIndirect{};
        renderer::rhi::PipelineType mBoundType{ renderer::rhi::PipelineType::eInvalid };
        const Pipeline *mBoundPipeline{};
        BindingTableSnapshots mBoundTables{};
        core::usize mIndirectSize{};
        core::usize mIndexSize{};
    };

    class InputLayout final : public renderer::rhi::IInputLayout {
    public:
        explicit InputLayout( const renderer::rhi::InputLayoutCreateDescription &desc );

        auto GetNumAttributes() const -> core::u32 override;

        auto GetAttributeDescription( core::u32 index ) const -> const renderer::rhi::VertexAttributeDescription & override;

        auto Apply( GLuint vao ) const -> void;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        renderer::rhi::InputLayoutCreateDescription mDesc{};
    };

    class Device final : public renderer::rhi::IGpuDevice {
    public:
        explicit Device( const renderer::rhi::GpuDeviceCreateInfo &info );

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        MKT_NODISCARD auto CreateTexture( const renderer::rhi::TextureCreateDescription &description ) -> renderer::rhi::TextureHandle override;
        MKT_NODISCARD auto CreateTextureNative( renderer::rhi::ObjectType type, renderer::rhi::Object object, const renderer::rhi::TextureCreateDescription &description ) -> renderer::rhi::TextureHandle override;

        MKT_NODISCARD auto CreateBuffer( const renderer::rhi::BufferCreateDescription &description ) -> renderer::rhi::BufferHandle override;

        MKT_NODISCARD auto CreateSampler( const renderer::rhi::SamplerCreateDescription &description ) -> renderer::rhi::SamplerHandle override;

        MKT_NODISCARD auto CreatePipeline( const renderer::rhi::ComputePipelineDescription &description ) -> renderer::rhi::PipelineHandle override;
        MKT_NODISCARD auto CreatePipeline( const renderer::rhi::GraphicsPipelineDescription &description ) -> renderer::rhi::PipelineHandle override;

        MKT_NODISCARD auto CreatePipeline( const rhi::RayTracingPipelineDescription & ) -> rhi::RayTracingPipelineHandle override;

        MKT_NODISCARD auto CreateShaderTable( const rhi::ShaderTableDescription& description ) -> rhi::ShaderTableHandle override;

        MKT_NODISCARD auto CreateAccelStructure( const renderer::rhi::AccelStructureCreateDescription &description ) -> renderer::rhi::AccelStructureHandle override;

        MKT_NODISCARD auto CreateCommandList( renderer::rhi::QueueType queue ) -> renderer::rhi::CommandListHandle override;
        MKT_NODISCARD auto CreateCommandList( const renderer::rhi::CommandListCreateDescription &desc ) -> renderer::rhi::CommandListHandle override;

        MKT_NODISCARD auto CreateShader( const renderer::rhi::ShaderModuleCreateDescription &desc ) -> renderer::rhi::ShaderModuleHandle override;

        MKT_NODISCARD auto CreateInputLayout( const renderer::rhi::InputLayoutCreateDescription &desc ) -> renderer::rhi::InputLayoutHandle override;

        MKT_NODISCARD auto CreateFence( core::u64 fenceInitialValue ) -> renderer::rhi::FenceHandle override;

        auto UnMap( renderer::rhi::IBuffer *buffer ) -> void override;
        MKT_NODISCARD auto Map( renderer::rhi::IBuffer *buffer ) -> void * override;

        MKT_NODISCARD auto CreateBindingLayout( const renderer::rhi::BindingLayoutDescription &desc ) -> renderer::rhi::BindingLayoutHandle override;
        MKT_NODISCARD auto CreatePipelineLayout( const renderer::rhi::PipelineLayoutCreateDescription &desc ) -> renderer::rhi::PipelineLayoutHandle override;
        MKT_NODISCARD auto CreateBindingTable( const renderer::rhi::BindingTableDescription &desc, renderer::rhi::BindingLayoutHandle layout ) -> renderer::rhi::BindingTableHandle override;
        auto UpdateBindingTable( const renderer::rhi::BindingTableDescription &desc, renderer::rhi::BindingTableHandle table ) -> void override;

        MKT_NODISCARD auto CreateDescriptorTableLayout( const renderer::rhi::DescriptorTableLayoutDescription &desc ) -> renderer::rhi::BindingLayoutHandle override;
        MKT_NODISCARD auto CreateDescriptorTable( renderer::rhi::BindingLayoutHandle layout ) -> renderer::rhi::DescriptorTableHandle override;
        MKT_NODISCARD auto ResizeDescriptorTable( renderer::rhi::DescriptorTableHandle descriptorTable, core::u32 newSize, bool keepContents ) -> bool override;
        MKT_NODISCARD auto WriteDescriptorTable( renderer::rhi::DescriptorTableHandle descriptorTable, const renderer::rhi::BindingTableItem &item ) -> renderer::rhi::DescriptorTableIndex override;
        auto ReleaseDescriptorTableIndex( rhi::DescriptorTableHandle descriptorTable, core::u32 bindingIndex, rhi::DescriptorTableIndex index ) -> void override;

        MKT_NODISCARD auto CreateSwapChain( const renderer::rhi::SwapChainDescription &description ) -> renderer::rhi::SwapChainHandle override;

        MKT_NODISCARD auto GetQueue( renderer::rhi::QueueType type ) -> renderer::rhi::IQueue * override;

        MKT_NODISCARD auto GetMemoryUsage() const -> core::usize override;
        MKT_NODISCARD auto GetMemoryTotal() const -> core::usize override;
        MKT_NODISCARD auto GetMemoryAvailable() const -> core::usize override;

        MKT_NODISCARD auto SupportsRayTracing() const -> bool override;

        MKT_NODISCARD auto SupportsBCTextureCompression() const -> bool override;

        MKT_NODISCARD auto IsFeatureSupported( rhi::GpuFeatureSupportFlags supportFlags ) const -> bool override;
        MKT_NODISCARD auto IsFeatureRequested( rhi::GpuFeatureSupportFlags requestFlags ) const -> bool override;

        auto RunGarbageCollection() -> void override;

        auto WaitIdle() -> void override;

        MKT_NODISCARD auto IsInitialized() const -> bool override;
        MKT_NODISCARD auto IsGraphicsApi( renderer::rhi::GraphicsAPI api ) const -> bool override;
        MKT_NODISCARD auto GetGraphicsApi() const -> renderer::rhi::GraphicsAPI override;
        MKT_NODISCARD auto GetDeviceName() const -> eastl::string_view override;
        MKT_NODISCARD auto GetVendorID() const -> eastl::string_view override;
        MKT_NODISCARD auto GetDriverVersion() const -> eastl::string_view override;

        MKT_NODISCARD auto GetQueryCapabilities( renderer::rhi::QueryType type, renderer::rhi::QueueType queue ) const -> renderer::rhi::QueryCapabilities override;
        MKT_NODISCARD auto CreateQueryPool( const renderer::rhi::QueryPoolDescription &description ) -> renderer::rhi::QueryPoolHandle override;
        MKT_NODISCARD auto GetState() const -> eastl::shared_ptr<DeviceState>;
        MKT_NODISCARD auto SupportsSceneMultisampling( renderer::rhi::Multisampling samples ) const -> bool override;

        MKT_NODISCARD auto GetProcessMemoryUsage() const -> rhi::ProcessGpuMemory override;

        ~Device() override = default;

    private:
        renderer::rhi::GpuDeviceCreateInfo mInfo{};
        eastl::shared_ptr<DeviceState> mState{};
        eastl::vector<renderer::rhi::QueueHandle> mQueues{};

        // Driver identity shown in diagnostics.
        eastl::string mName{};
        eastl::string mVendor{};
        eastl::string mVersion{};

        bool mInitialized{};
    };
}// namespace mikoto::renderer::opengl

#endif// MIKOTO_RENDERER_RHI_OPENGL_OPENGL_DEVICE_HH
