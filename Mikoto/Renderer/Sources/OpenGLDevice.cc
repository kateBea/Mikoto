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

#include <chrono>
#include <climits>
#include <utility>

#include <EASTL/algorithm.h>
#include <EASTL/numeric_limits.h>
#include <EASTL/utility.h>

#include <Core/Core.hh>
#include <Core/Exception.hh>
#include <Core/Profiler.hh>

#include <Logging/Logger.hh>

#include <Platform/Window.hh>

#include <Renderer/Core/RenderSystem.hh>
#include <Renderer/Rhi/OpenGL/OpenGLBuffer.hh>
#include <Renderer/Rhi/OpenGL/OpenGLDevice.hh>
#include <Renderer/Rhi/OpenGL/OpenGLHelpers.hh>
#include <Renderer/Rhi/OpenGL/OpenGLPipeline.hh>
#include <Renderer/Rhi/OpenGL/OpenGLQueryPool.hh>
#include <Renderer/Rhi/OpenGL/OpenGLShader.hh>
#include <Renderer/Rhi/OpenGL/OpenGLSwapChain.hh>
#include <Renderer/Rhi/OpenGL/OpenGLTexture.hh>
#include <Renderer/Rhi/Utility.hh>

// https://de45xmedrsdbp.cloudfront.net/Resources/files/UE4_OpenGL4_GDC2014-514746542.pdf

namespace mikoto::renderer::opengl {

    auto DeviceState::RequireOwner() const -> void {
        if ( !mAlive || mOwner != std::this_thread::get_id() ) {
            MKT_THROW_LOGIC_ERROR(
                "OpenGL requires the live device's context thread. Only command recording and resource release may run on workers." );
        }
    }

    auto DeviceState::Retire( eastl::function<void()> deletion ) -> void {
        std::lock_guard lock{ mDeletionMutex };

        if ( mAlive ) {
            mDeletions.emplace_back( eastl::move( deletion ) );
        }
    }

    auto DeviceState::Collect() -> void {
        RequireOwner();
        eastl::vector<eastl::function<void()>> deletions{};
        {
            std::lock_guard lock{ mDeletionMutex };
            deletions.swap( mDeletions );
        }

        for ( auto &deletion: deletions ) {
            deletion();
        }
    }

    Device::Device( const renderer::rhi::GpuDeviceCreateInfo &info )
        : mInfo{ info } {
    }

    auto Device::Initialize() -> void {
        if ( mInitialized ) {
            return;
        }

        // Reject features that this backend cannot represent before creating resources.
        constexpr auto unsupported{
            renderer::rhi::GpuFeatureSupportFlagsBits::BCTextureCompression |
            renderer::rhi::GpuFeatureSupportFlagsBits::HardwareRayTracing |
            renderer::rhi::GpuFeatureSupportFlagsBits::MeshShaders |
            renderer::rhi::GpuFeatureSupportFlagsBits::BindlessDescriptorTables
        };

        if ( ( mInfo.mFeatureSupportFlags & unsupported ) != renderer::rhi::GpuFeatureSupportFlagsBits::None ) {
            MKT_CORE_LOGGER_WARN( "Requested BC formats, ray tracing, mesh shaders or bindless descriptor tables whcih are not supported the current OGL version." );
        }

        auto *context{ mInfo.mWindow ? mInfo.mWindow->GetOpenGLContext() : nullptr };
        if ( !context || !context->MakeCurrent() ) {
            MKT_THROW_RUNTIME_ERROR( "The OpenGL device requires a current window context." );
        }

        // Resolve functions through the context's window backend.
        // https://wiki.libsdl.org/SDL3/SDL_GL_GetProcAddress
        const auto version{ gladLoadGLUserPtr( []( void *user, const char *name ) -> GLADapiproc {
            return static_cast<platform::IOpenGLWindowContext *>( user )->GetProcAddress( name );
        },
            context ) };

        if ( !version || !GLAD_GL_VERSION_4_6 ) {
            MKT_THROW_RUNTIME_ERROR( "An active OpenGL 4.6 core context is required." );
        }

        // Capture context ownership and driver identity for all child resources.
        mState = eastl::make_shared<DeviceState>();
        mName = reinterpret_cast<const char *>( glGetString( GL_RENDERER ) );
        mVendor = reinterpret_cast<const char *>( glGetString( GL_VENDOR ) );
        mVersion = reinterpret_cast<const char *>( glGetString( GL_VERSION ) );

        // Match the RHI depth range and avoid seams between cubemap faces.
        glClipControl( GL_LOWER_LEFT, GL_ZERO_TO_ONE );
        glEnable( GL_TEXTURE_CUBE_MAP_SEAMLESS );

        // Logical RHI queues all submit to this one OpenGL context stream.
        for ( const auto type: { renderer::rhi::QueueType::eGraphics, renderer::rhi::QueueType::eCompute, renderer::rhi::QueueType::eTransfer, renderer::rhi::QueueType::ePresent } ) {
            renderer::rhi::QueueHandle queue{ core::Ref<Queue>::New( type, mState ) };

            if ( queue.IsEmpty() ) {
                MKT_THROW_RUNTIME_ERROR( "Failed to allocate OpenGL queue." );
            }

            queue->Initialize( this );
            mQueues.push_back( eastl::move( queue ) );
        }

        CheckError( "device initialization" );

        mInitialized = true;
    }

    auto Device::Shutdown() -> void {
        // The thread that's owns the context is the one responsible for shutting it down
        if ( !mState || !mState->mAlive || mState->mOwner != std::this_thread::get_id() ) {
            return;
        }

        mState->RequireOwner();

        WaitIdle();

        mQueues.clear();
        mState->Collect();

        // The owning window releases the GL context. Surviving RHI handles may
        // subsequently die on workers; they must not call into a dead context.
        {
            std::lock_guard lock{ mState->mDeletionMutex };
            mState->mAlive = false;
            mState->mDeletions.clear();
        }

        mInitialized = false;
    }

    auto Device::CreateBuffer( const renderer::rhi::BufferCreateDescription &desc ) -> renderer::rhi::BufferHandle {
        MKT_BEGIN_PROFILER();

        if ( !mState || !mState->mAlive ) {
            MKT_THROW_LOGIC_ERROR( "OpenGL device has not been initialized." );
        }

        renderer::rhi::BufferHandle resource{ core::Ref<Buffer>::New( desc, mState ) };

        if ( resource.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate OpenGL Buffer resource." );
            return renderer::rhi::BufferHandle::CreateEmpty();
        }

        resource->Initialize( this );

        return resource;
    }

    auto Device::CreateTexture( const renderer::rhi::TextureCreateDescription &desc ) -> renderer::rhi::TextureHandle {
        MKT_BEGIN_PROFILER();

        if ( !mState || !mState->mAlive ) {
            MKT_THROW_LOGIC_ERROR( "OpenGL device has not been initialized." );
        }

        renderer::rhi::TextureHandle resource{ core::Ref<Texture>::New( desc, mState ) };

        if ( resource.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate OpenGL Texture resource." );
            return renderer::rhi::TextureHandle::CreateEmpty();
        }

        resource->Initialize( this );

        return resource;
    }

    auto Device::CreateTextureNative( renderer::rhi::ObjectType type, renderer::rhi::Object object, const renderer::rhi::TextureCreateDescription &desc ) -> renderer::rhi::TextureHandle {
        if ( type != renderer::rhi::ObjectType::GL_Texture || object.mType != renderer::rhi::Object::Type::Integer || !object.mInteger || object.mInteger > UINT_MAX ) {
            MKT_THROW_INVALID_ARGUMENT( "Expected a nonzero OpenGL texture name." );
        }

        MKT_BEGIN_PROFILER();

        if ( !mState || !mState->mAlive ) {
            MKT_THROW_LOGIC_ERROR( "OpenGL device has not been initialized." );
        }

        renderer::rhi::TextureHandle resource{ core::Ref<Texture>::New( desc, mState, static_cast<GLuint>( object.mInteger ) ) };

        if ( resource.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate OpenGL Texture resource." );
            return renderer::rhi::TextureHandle::CreateEmpty();
        }

        resource->Initialize( this );

        return resource;
    }

    auto Device::CreateSampler( const renderer::rhi::SamplerCreateDescription &desc ) -> renderer::rhi::SamplerHandle {
        MKT_BEGIN_PROFILER();

        if ( !mState || !mState->mAlive ) {
            MKT_THROW_LOGIC_ERROR( "OpenGL device has not been initialized." );
        }

        renderer::rhi::SamplerHandle resource{ core::Ref<Sampler>::New( desc, mState ) };

        if ( resource.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate OpenGL Sampler resource." );
            return renderer::rhi::SamplerHandle::CreateEmpty();
        }

        resource->Initialize( this );

        return resource;
    }

    auto Device::CreateShader( const renderer::rhi::ShaderModuleCreateDescription &desc ) -> renderer::rhi::ShaderModuleHandle {
        MKT_BEGIN_PROFILER();

        if ( !mState || !mState->mAlive ) {
            MKT_THROW_LOGIC_ERROR( "OpenGL device has not been initialized." );
        }

        renderer::rhi::ShaderModuleHandle resource{ core::Ref<Shader>::New( desc, mState ) };

        if ( resource.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate OpenGL Shader resource." );
            return renderer::rhi::ShaderModuleHandle::CreateEmpty();
        }

        resource->Initialize( this );

        return resource;
    }

    auto Device::CreateShaderTable( const rhi::ShaderTableDescription & ) -> rhi::ShaderTableHandle {
        return rhi::ShaderTableHandle::CreateEmpty();
    }

    auto Device::CreatePipeline( const renderer::rhi::GraphicsPipelineDescription &desc ) -> renderer::rhi::PipelineHandle {
        MKT_BEGIN_PROFILER();

        if ( !mState || !mState->mAlive ) {
            MKT_THROW_LOGIC_ERROR( "OpenGL device has not been initialized." );
        }

        renderer::rhi::PipelineHandle resource{ core::Ref<Pipeline>::New( desc, mState ) };

        if ( resource.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate OpenGL Pipeline resource." );
            return renderer::rhi::PipelineHandle::CreateEmpty();
        }

        resource->Initialize( this );

        return resource;
    }

    auto Device::CreatePipeline( const renderer::rhi::ComputePipelineDescription &desc ) -> renderer::rhi::PipelineHandle {
        MKT_BEGIN_PROFILER();

        if ( !mState || !mState->mAlive ) {
            MKT_THROW_LOGIC_ERROR( "OpenGL device has not been initialized." );
        }

        renderer::rhi::PipelineHandle resource{ core::Ref<Pipeline>::New( desc, mState ) };

        if ( resource.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate OpenGL Pipeline resource." );
            return renderer::rhi::PipelineHandle::CreateEmpty();
        }

        resource->Initialize( this );

        return resource;
    }

    auto Device::CreateInputLayout( const renderer::rhi::InputLayoutCreateDescription &desc ) -> renderer::rhi::InputLayoutHandle {
        MKT_BEGIN_PROFILER();

        if ( !mState || !mState->mAlive ) {
            MKT_THROW_LOGIC_ERROR( "OpenGL device has not been initialized." );
        }

        renderer::rhi::InputLayoutHandle resource{ core::Ref<InputLayout>::New( desc ) };

        if ( resource.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate OpenGL InputLayout resource." );
            return renderer::rhi::InputLayoutHandle::CreateEmpty();
        }

        resource->Initialize( this );

        return resource;
    }

    auto Device::CreateBindingLayout( const renderer::rhi::BindingLayoutDescription &desc ) -> renderer::rhi::BindingLayoutHandle {
        MKT_BEGIN_PROFILER();

        if ( !mState || !mState->mAlive ) {
            MKT_THROW_LOGIC_ERROR( "OpenGL device has not been initialized." );
        }

        renderer::rhi::BindingLayoutHandle resource{ core::Ref<BindingLayout>::New( desc ) };

        if ( resource.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate OpenGL BindingLayout resource." );
            return renderer::rhi::BindingLayoutHandle::CreateEmpty();
        }

        resource->Initialize( this );

        return resource;
    }

    auto Device::CreatePipelineLayout( const renderer::rhi::PipelineLayoutCreateDescription &desc ) -> renderer::rhi::PipelineLayoutHandle {
        MKT_BEGIN_PROFILER();

        if ( !mState || !mState->mAlive ) {
            MKT_THROW_LOGIC_ERROR( "OpenGL device has not been initialized." );
        }

        renderer::rhi::PipelineLayoutHandle resource{ core::Ref<PipelineLayout>::New( desc ) };

        if ( resource.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate OpenGL PipelineLayout resource." );
            return renderer::rhi::PipelineLayoutHandle::CreateEmpty();
        }

        resource->Initialize( this );

        return resource;
    }

    auto Device::CreateBindingTable( const renderer::rhi::BindingTableDescription &desc, renderer::rhi::BindingLayoutHandle layout ) -> renderer::rhi::BindingTableHandle {
        MKT_BEGIN_PROFILER();

        if ( !mState || !mState->mAlive ) {
            MKT_THROW_LOGIC_ERROR( "OpenGL device has not been initialized." );
        }

        renderer::rhi::BindingTableHandle resource{ core::Ref<BindingTable>::New( desc, layout ) };

        if ( resource.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate OpenGL BindingTable resource." );
            return renderer::rhi::BindingTableHandle::CreateEmpty();
        }

        resource->Initialize( this );

        return resource;
    }

    auto Device::UpdateBindingTable( const renderer::rhi::BindingTableDescription &desc, renderer::rhi::BindingTableHandle table ) -> void {
        auto *target{ dynamic_cast<BindingTable *>( table.GetPtr() ) };

        if ( !target ) {
            MKT_THROW_INVALID_ARGUMENT( "Expected OpenGL binding table." );
        }

        target->Update( desc );
    }

    auto Device::CreateCommandList( renderer::rhi::QueueType type ) -> renderer::rhi::CommandListHandle {
        if ( !GetQueue( type ) ) {
            MKT_THROW_INVALID_ARGUMENT( "Unsupported OpenGL queue type." );
        }

        MKT_BEGIN_PROFILER();

        if ( !mState || !mState->mAlive ) {
            MKT_THROW_LOGIC_ERROR( "OpenGL device has not been initialized." );
        }

        renderer::rhi::CommandListHandle resource{ core::Ref<CommandList>::New( type, mState ) };

        if ( resource.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate OpenGL CommandList resource." );
            return renderer::rhi::CommandListHandle::CreateEmpty();
        }

        resource->Initialize( this );

        return resource;
    }

    auto Device::CreateCommandList( const renderer::rhi::CommandListCreateDescription &desc ) -> renderer::rhi::CommandListHandle {
        auto result{ CreateCommandList( desc.mQueueType ) };

        if ( !result.IsEmpty() ) {
            result->SetDebugName( desc.mName );
        }

        return result;
    }

    auto Device::CreateFence( core::u64 value ) -> renderer::rhi::FenceHandle {
        MKT_BEGIN_PROFILER();

        if ( !mState || !mState->mAlive ) {
            MKT_THROW_LOGIC_ERROR( "OpenGL device has not been initialized." );
        }

        renderer::rhi::FenceHandle resource{ core::Ref<Fence>::New( value, mState ) };

        if ( resource.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate OpenGL Fence resource." );
            return renderer::rhi::FenceHandle::CreateEmpty();
        }

        resource->Initialize( this );

        return resource;
    }

    auto Device::Map( renderer::rhi::IBuffer *buffer ) -> void * {
        if ( !buffer ) {
            MKT_THROW_INVALID_ARGUMENT( "Cannot map a null buffer." );
        }

        return dynamic_cast<Buffer &>( *buffer ).Map();
    }

    auto Device::UnMap( renderer::rhi::IBuffer *buffer ) -> void {
        if ( !buffer ) {
            MKT_THROW_INVALID_ARGUMENT( "Cannot unmap a null buffer." );
        }

        dynamic_cast<Buffer &>( *buffer ).UnMap();
    }

    auto Device::ReadTextureRegion( const rhi::ITexture *texture, const rhi::Rect &region, eastl::span<core::u8> destination ) -> bool {
        MKT_BEGIN_PROFILER();

        if ( !mInitialized || !texture || texture->GetOwningDevice() != this ) {
            return false;
        }
        mState->RequireOwner();

        const auto rowSize{ rhi::GetTextureReadbackRowSize( *texture, region, destination.size() ) };
        if ( !rowSize ) {
            return false;
        }

        const auto width{ region.mMaxX - region.mMinX };
        const auto height{ region.mMaxY - region.mMinY };
        const auto byteSize{ rowSize * core::as<core::usize>( height ) };
        if ( byteSize > eastl::numeric_limits<GLsizei>::max() ) {
            return false;
        }

        const auto format{ opengl::GetFormat( texture->GetFormat() ) };
        const auto name{ texture->GetNativeHandle( rhi::ObjectType::GL_Texture ) };
        const auto bottom{ texture->GetHeight() - core::as<core::u32>( region.mMaxY ) };

        // Read into CPU memory with tightly packed rows, regardless of the current pack state.
        // https://registry.khronos.org/OpenGL/extensions/ARB/ARB_get_texture_sub_image.txt
        GLint packBuffer{};
        glGetIntegerv( GL_PIXEL_PACK_BUFFER_BINDING, &packBuffer );
        glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );

        struct PackSetting {
            GLenum mName{};
            GLint mValue{};
            GLint mPrevious{};
        };
        PackSetting settings[]{
            { .mName = GL_PACK_ALIGNMENT, .mValue = 1 },
            { .mName = GL_PACK_ROW_LENGTH },
            { .mName = GL_PACK_SKIP_ROWS },
            { .mName = GL_PACK_SKIP_PIXELS },
            { .mName = GL_PACK_SWAP_BYTES }
        };
        for ( auto &setting: settings ) {
            glGetIntegerv( setting.mName, &setting.mPrevious );
            glPixelStorei( setting.mName, setting.mValue );
        }

        glGetTextureSubImage( core::as<GLuint>( name.mInteger ), 0, region.mMinX, core::as<GLint>( bottom ), 0,
            width, height, 1, format.mExternal, format.mType, core::as<GLsizei>( byteSize ), destination.data() );
        const bool read{ glGetError() == GL_NO_ERROR };

        for ( const auto &setting: settings ) {
            glPixelStorei( setting.mName, setting.mPrevious );
        }
        glBindBuffer( GL_PIXEL_PACK_BUFFER, core::as<GLuint>( packBuffer ) );
        if ( !read ) {
            return false;
        }

        // The RHI returns the top row first; GL gives us the bottom row first.
        for ( core::usize row{}; row < core::as<core::usize>( height / 2 ); ++row ) {
            auto *first{ destination.data() + row * rowSize };
            auto *last{ destination.data() + byteSize - ( row + 1 ) * rowSize };
            eastl::swap_ranges( first, first + rowSize, last );
        }
        return true;
    }

    auto Device::CreateAccelStructure( const renderer::rhi::AccelStructureCreateDescription & ) -> renderer::rhi::AccelStructureHandle {
        Unsupported( "ray tracing acceleration structures" );

        return renderer::rhi::AccelStructureHandle::CreateEmpty();
    }

    auto Device::CreateDescriptorTableLayout( const renderer::rhi::DescriptorTableLayoutDescription & ) -> renderer::rhi::BindingLayoutHandle {
        Unsupported( "bindless descriptor tables; use explicit binding layouts" );

        return renderer::rhi::BindingLayoutHandle::CreateEmpty();
    }

    auto Device::CreateDescriptorTable( renderer::rhi::BindingLayoutHandle ) -> renderer::rhi::DescriptorTableHandle {
        Unsupported( "bindless descriptor tables" );

        return renderer::rhi::DescriptorTableHandle::CreateEmpty();
    }

    auto Device::ResizeDescriptorTable( renderer::rhi::DescriptorTableHandle, core::u32, bool ) -> bool {
        Unsupported( "bindless descriptor tables" );

        return false;
    }

    auto Device::ReleaseDescriptorTableIndex( rhi::DescriptorTableHandle, core::u32, rhi::DescriptorTableIndex ) -> void {
        MKT_BEGIN_PROFILER();
        // This backend does not allocate bindless descriptor table indices.
    }

    auto Device::WriteDescriptorTable( renderer::rhi::DescriptorTableHandle, const renderer::rhi::BindingTableItem & ) -> renderer::rhi::DescriptorTableIndex {
        Unsupported( "bindless descriptor tables" );

        // Make compiler Happy
        return rhi::kInvalidBindingItemIndex;
    }

    auto Device::CreateSwapChain( const renderer::rhi::SwapChainDescription &desc ) -> renderer::rhi::SwapChainHandle {
        MKT_BEGIN_PROFILER();

        if ( !mState || !mState->mAlive ) {
            MKT_THROW_LOGIC_ERROR( "OpenGL device has not been initialized." );
        }

        renderer::rhi::SwapChainHandle resource{ core::Ref<SwapChain>::New( desc ) };

        if ( resource.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate OpenGL SwapChain resource." );
            return renderer::rhi::SwapChainHandle::CreateEmpty();
        }

        resource->Initialize( this );

        return resource;
    }

    auto Device::GetQueue( renderer::rhi::QueueType type ) -> renderer::rhi::IQueue * {
        for ( auto &queue: mQueues ) {

            if ( queue->GetType() == type ) {
                return queue.GetPtr();
            }
        }

        return nullptr;
    }

    auto Device::GetMemoryUsage() const -> core::usize {
        return mState ? mState->mAllocatedBytes.load() : 0;
    }

    // Portable GL exposes no total/free VRAM query. Zero denotes unavailable.

    auto Device::GetMemoryTotal() const -> core::usize {
        return 0;
    }

    auto Device::GetMemoryAvailable() const -> core::usize {
        return 0;
    }

    auto Device::IsFeatureSupported( rhi::GpuFeatureSupportFlags supportFlags ) const -> bool {
        MKT_BEGIN_PROFILER();
        rhi::GpuFeatureSupportFlags supported{ rhi::GpuFeatureSupportFlagsBits::None };

        // Report the initialized OpenGL backend's implemented paths, not the request mask.
        if ( mInitialized ) {
            supported = rhi::GpuFeatureSupportFlagsBits::HardwareWireframe |
                        rhi::GpuFeatureSupportFlagsBits::WindowPresentation |
                        rhi::GpuFeatureSupportFlagsBits::TextureRegionReadback |
                        rhi::GpuFeatureSupportFlagsBits::OrderedUniformBufferWrites;
        }

        return ( supported & supportFlags ) == supportFlags;
    }

    auto Device::IsFeatureRequested( rhi::GpuFeatureSupportFlags requestFlags ) const -> bool {
        MKT_BEGIN_PROFILER();
        return ( mInfo.mFeatureSupportFlags & requestFlags ) == requestFlags;
    }

    auto Device::RunGarbageCollection() -> void {
        if ( mState ) {

            for ( auto queue: mQueues ) {
                core::checked_cast<Queue *>( queue.GetPtr() )->CollectGarbage();
            }

            mState->Collect();
        }
    }

    auto Device::WaitIdle() -> void {
        mState->RequireOwner();
        glFinish();
        RunGarbageCollection();
    }

    auto Device::IsInitialized() const -> bool {
        return mInitialized;
    }

    auto Device::IsGraphicsApi( renderer::rhi::GraphicsAPI api ) const -> bool {
        return api == renderer::rhi::GraphicsAPI::eOpenGL;
    }

    auto Device::GetGraphicsApi() const -> renderer::rhi::GraphicsAPI {
        return renderer::rhi::GraphicsAPI::eOpenGL;
    }

    auto Device::GetDeviceName() const -> eastl::string_view {
        return mName;
    }

    auto Device::GetVendorID() const -> eastl::string_view {
        return mVendor;
    }

    auto Device::GetDriverVersion() const -> eastl::string_view {
        return mVersion;
    }

    auto Device::GetQueryCapabilities( renderer::rhi::QueryType type, renderer::rhi::QueueType queue ) const -> renderer::rhi::QueryCapabilities {
        mState->RequireOwner();
        return QueryPool::Capabilities( type, queue );
    }

    auto Device::CreateQueryPool( const renderer::rhi::QueryPoolDescription &desc ) -> renderer::rhi::QueryPoolHandle {
        if ( !GetQueryCapabilities( desc.mType, desc.mQueue ).mSupported ) {
            return renderer::rhi::QueryPoolHandle::CreateEmpty();
        }

        MKT_BEGIN_PROFILER();

        if ( !mState || !mState->mAlive ) {
            MKT_THROW_LOGIC_ERROR( "OpenGL device has not been initialized." );
        }

        renderer::rhi::QueryPoolHandle resource{ core::Ref<QueryPool>::New( desc, mState ) };

        if ( resource.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate OpenGL QueryPool resource." );
            return renderer::rhi::QueryPoolHandle::CreateEmpty();
        }

        resource->Initialize( this );

        return resource;
    }

    auto Device::GetState() const -> eastl::shared_ptr<DeviceState> {
        return mState;
    }

    auto Device::SupportsSceneMultisampling( renderer::rhi::Multisampling samples ) const -> bool {
        return samples == renderer::rhi::Multisampling::eMsaaX1;
    }

    auto Device::SupportsBCTextureCompression() const -> bool {
        return false;
    }

    auto Device::GetProcessMemoryUsage() const -> rhi::ProcessGpuMemory {
        return {};
    }

    auto Device::SupportsRayTracing() const -> bool {
        return false;
    }

    auto Device::CreatePipeline( const rhi::RayTracingPipelineDescription & ) -> rhi::RayTracingPipelineHandle {
        return rhi::RayTracingPipelineHandle::CreateEmpty();
    }

    InputLayout::InputLayout( const renderer::rhi::InputLayoutCreateDescription &desc )
        : mDesc{ desc } {

        if ( desc.mUseReflection ) {
            Unsupported( "automatic vertex input reflection; supply explicit attributes or use SSBO vertex pulling" );
        }
    }

    auto InputLayout::Initialize() -> void {
        mIsAllocated = true;
    }

    auto InputLayout::Destroy() -> void {
        mIsAllocated = false;
    }

    auto InputLayout::GetNumAttributes() const -> core::u32 {
        return static_cast<core::u32>( mDesc.mVertexAttributeDescriptions.size() );
    }

    auto InputLayout::GetAttributeDescription( core::u32 index ) const -> const renderer::rhi::VertexAttributeDescription & {
        return mDesc.mVertexAttributeDescriptions.at( index );
    }

    auto InputLayout::Apply( GLuint vao ) const -> void {
        GLint maxAttributes{};
        glGetIntegerv( GL_MAX_VERTEX_ATTRIBS, &maxAttributes );

        for ( GLint index{}; index < maxAttributes; ++index ) {
            glDisableVertexArrayAttrib( vao, index );
        }

        for ( const auto &binding: mDesc.mVertexBindingDescriptions ) {
            glVertexArrayBindingDivisor( vao, binding.mBinding, binding.mRate == renderer::rhi::InputRate::ePerInstance ? 1 : 0 );
        }

        for ( const auto &attribute: mDesc.mVertexAttributeDescriptions ) {
            const auto format{ opengl::GetFormat( attribute.mFormat ) };
            const auto &info{ renderer::rhi::GetFormatInfo( attribute.mFormat ) };
            const GLint components{ info.mHasRed + info.mHasGreen + info.mHasBlue + info.mHasAlpha };

            if ( format.mDepth || !components ) {
                MKT_THROW_INVALID_ARGUMENT( "Invalid vertex attribute format." );
            }

            glEnableVertexArrayAttrib( vao, attribute.mLocation );
            glVertexArrayAttribBinding( vao, attribute.mLocation, attribute.mBinding );

            if ( format.mInteger ) {
                glVertexArrayAttribIFormat( vao, attribute.mLocation, components, format.mType, attribute.mOffset );
            }
            else {
                glVertexArrayAttribFormat( vao, attribute.mLocation, components, format.mType, info.mKind == rhi::FormatKind::Normalized,
                    attribute.mOffset );
            }
        }
    }

    BindingLayout::BindingLayout( const renderer::rhi::BindingLayoutDescription &desc )
        : mDescription{ desc } {
    }

    auto BindingLayout::Initialize() -> void {
        mIsAllocated = true;
    }

    auto BindingLayout::Destroy() -> void {
        mIsAllocated = false;
    }

    auto BindingLayout::GetRegisterSpace() const -> core::u32 {
        return mDescription.mRegisterSpace;
    }

    auto BindingLayout::IsBindless() const -> bool {
        return false;
    }

    auto BindingLayout::GetDescription() const -> const rhi::BindingLayoutDescription & {
        return mDescription;
    }

    PipelineLayout::PipelineLayout( const renderer::rhi::PipelineLayoutCreateDescription &desc )
        : mDescription{ desc } {

        if ( desc.mPushConstantsSize > renderer::rhi::kMaxPushConstantSize ) {
            MKT_THROW_INVALID_ARGUMENT( "Push constants exceed the RHI limit." );
        }

        eastl::vector<eastl::pair<core::u32, core::u32>> bindings{};
        const auto bindingNamespace{ []( renderer::rhi::ResourceType type ) -> core::u32 {
            switch ( type ) {
                case renderer::rhi::ResourceType::eConstantBuffer:
                case renderer::rhi::ResourceType::eVolatileConstantBuffer:
                    return 0;
                case renderer::rhi::ResourceType::eSampler:
                    return 1;
                case renderer::rhi::ResourceType::eTexture_SRV:
                case renderer::rhi::ResourceType::eTypedBuffer_SRV:
                    return 2;
                case renderer::rhi::ResourceType::eTexture_UAV:
                case renderer::rhi::ResourceType::eTypedBuffer_UAV:
                    return 3;
                default:
                    return 4;
            }
        } };

        for ( const auto &layout: desc.mBindingLayouts ) {

            if ( !layout || layout->IsBindless() ) {
                Unsupported( "bindless pipeline layouts" );
            }

            const auto *glLayout{ core::checked_cast<const BindingLayout *>( layout.GetPtr() ) };

            if ( !glLayout ) {
                MKT_THROW_INVALID_ARGUMENT( "Pipeline layout belongs to another graphics API." );
            }

            for ( const auto &item: glLayout->GetDescription().mBindings ) {
                const auto key{ eastl::make_pair( bindingNamespace( item.mType ), item.mSlot ) };

                for ( const auto &existing: bindings ) {

                    if ( existing == key ) {
                        MKT_THROW_INVALID_ARGUMENT(
                            "OpenGL binding numbers must be unique across descriptor sets within each resource namespace." );
                    }
                }

                bindings.push_back( key );

                if ( item.mSlot == kPushConstantBinding &&
                     ( item.mType == renderer::rhi::ResourceType::eConstantBuffer || item.mType == renderer::rhi::ResourceType::eVolatileConstantBuffer ) ) {
                    MKT_THROW_INVALID_ARGUMENT( "UBO binding 15 is reserved for RHI push constants." );
                }
            }
        }
    }

    auto PipelineLayout::GetDescription() const -> const rhi::PipelineLayoutCreateDescription & {
        return mDescription;
    }

    auto PipelineLayout::GetPushConstantBinding() -> GLuint {
        return kPushConstantBinding;
    }

    auto PipelineLayout::Initialize() -> void {
        mIsAllocated = true;
    }

    auto PipelineLayout::Destroy() -> void {
        mIsAllocated = false;
    }

    auto BindingTable::Snapshot() const -> core::Ref<BindingTable> {
        return core::Ref<BindingTable>::New( mDesc, mLayout );
    }

    BindingTable::BindingTable( const renderer::rhi::BindingTableDescription &desc, renderer::rhi::BindingLayoutHandle layout )
        : mLayout{ eastl::move( layout ) } {
        Update( desc );
    }

    auto BindingTable::Initialize() -> void {
        mIsAllocated = true;
    }

    auto BindingTable::Destroy() -> void {
        mRetained.clear();
        mIsAllocated = false;
    }

    auto BindingTable::Update( const renderer::rhi::BindingTableDescription &desc ) -> void {
        if ( desc.mUseReflection ) {
            Unsupported( "automatic descriptor reflection; supply explicit bindings" );
        }

        const auto *layout{ dynamic_cast<const BindingLayout *>( mLayout.operator->() ) };

        if ( !layout ) {
            MKT_THROW_INVALID_ARGUMENT( "Missing OpenGL binding layout." );
        }

        eastl::vector<core::Ref<core::Resource>> retained{};

        for ( const auto &item: desc.mBindings ) {
            bool found{};

            for ( const auto &declared: layout->GetDescription().mBindings ) {
                found |= declared.mSlot == item.mBindingIndex && declared.mType == item.mResourceType;
            }

            if ( !found || !item.mResource ) {
                MKT_THROW_INVALID_ARGUMENT( "Binding table does not match its declared layout." );
            }

            retained.emplace_back( item.mResource );
        }

        mDesc = desc;
        mRetained = eastl::move( retained );
    }

    auto BindingTable::Bind() const -> void {
        for ( const auto &item: mDesc.mBindings ) {
            const auto slot{ item.mBindingIndex };

            switch ( item.mResourceType ) {
                case renderer::rhi::ResourceType::eSampler: {
                    const auto *sampler{ dynamic_cast<const Sampler *>( item.mResource ) };

                    if ( !sampler ) {
                        MKT_THROW_INVALID_ARGUMENT( "Expected OpenGL sampler." );
                    }

                    glBindSampler( slot, sampler->Name() );
                    break;
                }

                case renderer::rhi::ResourceType::eTexture_SRV:
                case renderer::rhi::ResourceType::eTexture_UAV: {
                    const auto *texture{ dynamic_cast<const Texture *>( item.mResource ) };

                    if ( !texture ) {
                        MKT_THROW_INVALID_ARGUMENT( "Expected OpenGL texture." );
                    }

                    if ( item.mResourceType == renderer::rhi::ResourceType::eTexture_SRV ) {
                        glBindTextureUnit( slot, texture->SampledView( item ) );
                    }
                    else {
                        const auto &sub{ item.mSubResourceSet };

                        if ( sub.mBaseMipLevel >= texture->GetMipLevelCount() || sub.mBaseArraySlice >= texture->Layers() ) {
                            MKT_THROW_OUT_OF_RANGE( "Image binding subresource is invalid." );
                        }

                        if ( ( sub.mNumMipLevels != 1 && sub.mNumMipLevels != renderer::rhi::TextureSubresourceSet::kAllMipLevels ) ||
                             ( sub.mNumArraySlices != 1 &&
                                 ( sub.mBaseArraySlice != 0 || ( sub.mNumArraySlices != texture->Layers() &&
                                                                   sub.mNumArraySlices != renderer::rhi::TextureSubresourceSet::kAllArraySlices ) ) ) ) {
                            Unsupported( "image bindings spanning a subset of array layers or multiple mips" );
                        }

                        if ( item.mFormat != renderer::rhi::Format::eUnknown && item.mFormat != texture->GetFormat() ) {
                            Unsupported( "reinterpreted storage-image formats" );
                        }

                        glBindImageTexture( slot, texture->Name(), sub.mBaseMipLevel, texture->GetDimension() == rhi::TextureDimension::eTexture3D || sub.mNumArraySlices != 1, sub.mBaseArraySlice,
                            GL_READ_WRITE, opengl::GetFormat( texture->GetFormat() ).mInternal );
                    }

                    break;
                }

                case renderer::rhi::ResourceType::eConstantBuffer:
                case renderer::rhi::ResourceType::eVolatileConstantBuffer:
                case renderer::rhi::ResourceType::eStructuredBuffer_SRV:
                case renderer::rhi::ResourceType::eStructuredBuffer_UAV:
                case renderer::rhi::ResourceType::eRawBuffer_SRV:
                case renderer::rhi::ResourceType::eRawBuffer_UAV: {
                    const auto *buffer{ dynamic_cast<const Buffer *>( item.mResource ) };

                    if ( !buffer ) {
                        MKT_THROW_INVALID_ARGUMENT( "Expected OpenGL buffer." );
                    }

                    const auto target{ item.mResourceType == renderer::rhi::ResourceType::eConstantBuffer || item.mResourceType == renderer::rhi::ResourceType::eVolatileConstantBuffer
                                           ? GL_UNIFORM_BUFFER
                                           : GL_SHADER_STORAGE_BUFFER };
                    const auto offset{ item.mRange.mByteOffset };

                    if ( offset >= buffer->GetSizeBytes() ) {
                        MKT_THROW_OUT_OF_RANGE( "Buffer binding offset exceeds allocation." );
                    }

                    const auto size{ item.mRange.mByteSize == 0 || item.mRange.mByteSize == ~core::u64{} ? buffer->GetSizeBytes() - offset
                                                                                                         : item.mRange.mByteSize };

                    if ( size > buffer->GetSizeBytes() - offset ) {
                        MKT_THROW_OUT_OF_RANGE( "Buffer binding range exceeds allocation." );
                    }

                    glBindBufferRange( target, slot, buffer->Name(), offset, size );
                    break;
                }

                default:
                    Unsupported( "this descriptor type" );
            }
        }

        CheckError( "binding resources" );
    }

    auto BindingTable::Find( renderer::rhi::ResourceType type, core::u32 slot ) const -> const renderer::rhi::BindingTableItem * {
        for ( const auto &item: mDesc.mBindings ) {

            if ( item.mResourceType == type && item.mBindingIndex == slot ) {
                return &item;
            }
        }

        return nullptr;
    }

    Fence::Fence( core::u64 initial, eastl::shared_ptr<DeviceState> state )
        : mState{ eastl::move( state ) },
          mCompleted{ initial },
          mLastSignal{ initial } {
    }

    Fence::~Fence() {
        Destroy();
    }

    auto Fence::Initialize() -> void {
        mIsAllocated = true;
    }

    auto Fence::Destroy() -> void {
        for ( const auto &pending: mPending ) {
            mState->Retire( [sync = pending.mSync] {
                glDeleteSync( sync );
            } );
        }

        mPending.clear();
        mIsAllocated = false;
    }

    auto Fence::GetCompletionValue() const -> core::u64 {
        if ( mState->mOwner != std::this_thread::get_id() ) {
            return mCompleted.load();
        }

        mState->RequireOwner();

        while ( !mPending.empty() ) {
            const auto result{ glClientWaitSync( mPending.front().mSync, 0, 0 ) };

            if ( result == GL_WAIT_FAILED ) {
                MKT_THROW_RUNTIME_ERROR( "OpenGL fence polling failed." );
            }

            if ( result == GL_TIMEOUT_EXPIRED ) {
                break;
            }

            mCompleted = mPending.front().mValue;
            glDeleteSync( mPending.front().mSync );
            mPending.erase( mPending.begin() );
        }

        return mCompleted.load();
    }

    auto Fence::Signal( core::u64 value ) -> bool {
        mState->RequireOwner();

        if ( value <= mLastSignal ) {
            return false;
        }

        // A host signal completes immediately. Queue signals insert GLsyncs.

        for ( const auto &pending: mPending ) {
            glDeleteSync( pending.mSync );
        }

        mPending.clear();
        mCompleted = value;
        mLastSignal = value;
        return true;
    }

    auto Fence::SignalOnGpu( core::u64 value ) -> bool {
        mState->RequireOwner();

        if ( value <= mLastSignal ) {
            return false;
        }

        const auto sync{ glFenceSync( GL_SYNC_GPU_COMMANDS_COMPLETE, 0 ) };

        if ( !sync ) {
            MKT_THROW_RUNTIME_ERROR( "OpenGL fence creation failed." );
        }

        mPending.push_back( { value, sync } );
        mLastSignal = value;
        glFlush();
        return true;
    }

    auto Fence::Wait( core::u64 value, core::u64 timeoutMs ) -> bool {
        mState->RequireOwner();

        if ( GetCompletionValue() >= value ) {
            return true;
        }

        for ( const auto &pending: mPending ) {

            if ( pending.mValue < value ) {
                continue;
            }

            const core::u64 timeout{ timeoutMs > ( eastl::numeric_limits<core::u64>::max() - 1 ) / 1000000 ? eastl::numeric_limits<core::u64>::max() - 1
                                                                                                           : timeoutMs * 1000000 };
            const auto result{ glClientWaitSync( pending.mSync, GL_SYNC_FLUSH_COMMANDS_BIT, timeout ) };

            if ( result == GL_WAIT_FAILED ) {
                MKT_THROW_RUNTIME_ERROR( "OpenGL fence wait failed." );
            }

            return result != GL_TIMEOUT_EXPIRED && GetCompletionValue() >= value;
        }

        MKT_THROW_LOGIC_ERROR( "OpenGL cannot wait for a value that has not been submitted on its single context stream." );
    }

    auto Fence::WaitOnGpu( core::u64 value ) -> void {
        mState->RequireOwner();

        if ( GetCompletionValue() >= value ) {
            return;
        }

        for ( const auto &pending: mPending ) {

            if ( pending.mValue >= value ) {
                glWaitSync( pending.mSync, 0, GL_TIMEOUT_IGNORED );
                return;
            }
        }

        MKT_THROW_LOGIC_ERROR( "Submit the OpenGL producer before its consumer queue wait." );
    }

    auto Queue::GetType() const -> rhi::QueueType {
        return mType;
    }

    auto Queue::GetOpSupportFlags() const -> rhi::QueueOpSupportFlags {
        return mOpSupportFlags;
    }

    Queue::Queue( renderer::rhi::QueueType type, eastl::shared_ptr<DeviceState> state )
        : mType{ type }, mOpSupportFlags{ renderer::rhi::QueueOpSupportFlagsBits::Graphics | renderer::rhi::QueueOpSupportFlagsBits::Compute | renderer::rhi::QueueOpSupportFlagsBits::Transfer | renderer::rhi::QueueOpSupportFlagsBits::Presentation }, mState{ eastl::move( state ) } {
    }

    auto Queue::Initialize() -> void {
        mIsAllocated = true;
    }

    auto Queue::Destroy() -> void {
        mIsAllocated = false;
    }

    auto Queue::CollectGarbage() -> void {
        mState->RequireOwner();

        for ( auto it{ mPendingResources.begin() }; it != mPendingResources.end(); ) {
            const auto status{ glClientWaitSync( it->mCompletion, 0, 0 ) };

            if ( status != GL_ALREADY_SIGNALED && status != GL_CONDITION_SATISFIED ) {
                break;
            }

            glDeleteSync( it->mCompletion );
            it = mPendingResources.erase( it );
        }
    }

    auto Queue::ExecuteCommandLists( const renderer::rhi::SubmitInfo &info ) -> void {
        mState->RequireOwner();
        // Native GL commands retain their objects; track submission completion.
        PendingResources pending{};

        for ( auto command: info.mCommands ) {
            core::checked_cast<CommandList *>( command.GetPtr() )->ValidateSubmission();
        }

        mPendingResources.reserve( mPendingResources.size() + 1 );

        try {

            for ( auto wait: info.mWaits ) {
                dynamic_cast<Fence &>( *wait.mSignalFence ).WaitOnGpu( wait.mSignalValue );
            }

            for ( auto command: info.mCommands ) {
                core::checked_cast<CommandList *>( command.GetPtr() )->Execute();
            }

            for ( auto signal: info.mSignals ) {

                if ( !dynamic_cast<Fence &>( *signal.mSignalFence ).SignalOnGpu( signal.mSignalValue ) ) {
                    MKT_THROW_RUNTIME_ERROR( "Fence signals must increase monotonically." );
                }
            }

            pending.mCompletion = glFenceSync( GL_SYNC_GPU_COMMANDS_COMPLETE, 0 );

            if ( !pending.mCompletion ) {
                MKT_THROW_RUNTIME_ERROR( "Cannot track OpenGL submission completion." );
            }

            glFlush();
        } catch ( ... ) {
            // Some GL work may already have executed before recording failed.
            glFinish();

            if ( pending.mCompletion ) {
                glDeleteSync( pending.mCompletion );
            }

            throw;
        }

        mPendingResources.push_back( eastl::move( pending ) );

        for ( auto command: info.mCommands ) {
            core::checked_cast<CommandList *>( command.GetPtr() )->MarkSubmitted();
        }
    }

    CommandList::CommandList( renderer::rhi::QueueType type, eastl::shared_ptr<DeviceState> state )
        : mQueueType{ type }, mState{ eastl::move( state ) } {
    }

    CommandList::~CommandList() {
        Destroy();
    }

    auto CommandList::GetStatistics() const -> const rhi::CommandListStatistics & {
        return mStatistics;
    }

    auto CommandList::DecodeVideo( const rhi::VideoDecodeDescription & ) -> void {
        MKT_THROW_RUNTIME_ERROR( "Video decoding is not supported by the OpenGL command-list backend." );
    }

    auto CommandList::Initialize() -> void {
        mIsAllocated = true;
    }

    auto CommandList::AcquireResource( const rhi::BufferBarrierDescription &, rhi::IQueue * ) -> void {
        // This backend has no explicit queue-family ownership to transfer.
    }

    auto CommandList::ReleaseResource( const rhi::BufferBarrierDescription &, rhi::IQueue * ) -> void {
        // This backend has no explicit queue-family ownership to transfer.
    }

    auto CommandList::AcquireResource( const rhi::TextureBarrierDescription &, rhi::IQueue * ) -> void {
        // This backend has no explicit queue-family ownership to transfer.
    }

    auto CommandList::ReleaseResource( const rhi::TextureBarrierDescription &, rhi::IQueue * ) -> void {
        // This backend has no explicit queue-family ownership to transfer.
    }

    auto CommandList::Build( rhi::IAccelerationStructure * ) -> void {
        MKT_THROW_RUNTIME_ERROR( "Build is unsupported by this backend." );
    }


    auto CommandList::Destroy() -> void {
        // Detach native names now; deletion must run on the context owner.
        const auto framebuffer{ eastl::exchange( mFramebuffer, 0 ) };
        const auto vao{ eastl::exchange( mVertexArray, 0 ) };
        const auto push{ eastl::exchange( mPushBuffer, 0 ) };

        mState->Retire( [framebuffer, vao, push] {
            glDeleteFramebuffers( 1, &framebuffer );
            glDeleteVertexArrays( 1, &vao );
            glDeleteBuffers( 1, &push );
        } );

        mCommands.clear();

        mIsAllocated = false;
    }

    auto CommandList::Open( const renderer::rhi::CommandListBeginDescription & ) -> void {
        if ( mRecording || mExecutable ) {
            MKT_THROW_RUNTIME_ERROR( "The previous OpenGL recording has not been submitted." );
        }

        mStatistics = {};
        mCommands.clear();

        mBoundPipeline = nullptr;
        mBoundTables.clear();
        mRecording = true;
        mExecutable = false;
        mInsidePass = false;
    }

    auto CommandList::Close() -> void {
        if ( !mRecording || mInsidePass ) {
            MKT_THROW_LOGIC_ERROR( "End requires a recording list with no active render pass." );
        }

        mRecording = false;
        mExecutable = true;
    }

    auto CommandList::ValidateSubmission() const -> void {
        if ( !mExecutable ) {
            MKT_THROW_RUNTIME_ERROR( "The command recording must be closed and submitted only once." );
        }
    }

    auto CommandList::MarkSubmitted() -> void {
        mExecutable = false;
    }

    auto CommandList::Record( eastl::function<void()> command ) -> void {
        if ( !mRecording ) {
            MKT_THROW_LOGIC_ERROR( "OpenGL command list is not recording." );
        }

        mCommands.emplace_back( eastl::move( command ) );
    }

    auto CommandList::Execute() -> void {
        mState->RequireOwner();

        if ( !mExecutable ) {
            MKT_THROW_LOGIC_ERROR( "Command list must be ended before submission." );
        }

        if ( !mVertexArray ) {
            glCreateVertexArrays( 1, &mVertexArray );
        }

        if ( !mFramebuffer ) {
            glCreateFramebuffers( 1, &mFramebuffer );
        }

        if ( !mPushBuffer ) {
            glCreateBuffers( 1, &mPushBuffer );
            glNamedBufferStorage( mPushBuffer, rhi::kMaxPushConstantSize, nullptr, GL_DYNAMIC_STORAGE_BIT );
        }

        // Reset replay state so previous submissions cannot affect this list.
        mHasPipeline = false;
        mIndexed = false;
        mIndirect = false;
        mBoundType = renderer::rhi::PipelineType::eInvalid;
        mBoundPipeline = nullptr;
        mBoundTables.clear();

        // Establish the fixed context state expected by the recorded RHI commands.
        glBindVertexArray( mVertexArray );
        glClipControl( GL_LOWER_LEFT, GL_ZERO_TO_ONE );
        glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );
        glBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0 );
        glDisable( GL_FRAMEBUFFER_SRGB );

        for ( auto &command: mCommands ) {
            command();
        }

        // These native calls have consumed their CPU payload. The queue keeps
        // explicit resource refs until its GL sync signals completion.
        mCommands.clear();
        mBoundTables.clear();
        mBoundPipeline = nullptr;
        CheckError( "command list submission" );
    }

    auto CommandList::SetBarrier( const renderer::rhi::BarrierDescription &desc ) -> void {
        if ( desc.mBuffers.empty() && desc.mTextures.empty() ) {
            return;
        }

        // OpenGL has no image layouts or queue-family ownership. A full memory
        // barrier makes earlier shader writes visible to subsequent RHI consumers.
        // Keep this conservative until per-access bit mapping is measured.
        ++mStatistics.mBarriers.mCallCount;
        ++mStatistics.mBarriers.mMemoryCount;

        Record( [] {
            glMemoryBarrier( GL_ALL_BARRIER_BITS );
        } );
    }

    auto CommandList::SetTransition( const renderer::rhi::TransitionDescription &desc ) -> void {
        for ( const auto &item: desc.mBuffers ) {
            auto buffer{ Retain<Buffer>( item.mBuffer ) };
            Record( [buffer, state = item.mState]() mutable {
                buffer->SetResourceState( state );
            } );
        }

        for ( const auto &item: desc.mTextures ) {
            auto texture{ Retain<Texture>( item.mTexture ) };
            Record( [texture, state = item.mState]() mutable {
                texture->SetResourceState( state );
            } );
        }

        if ( !desc.mBuffers.empty() || !desc.mTextures.empty() ) {
            ++mStatistics.mBarriers.mCallCount;
            ++mStatistics.mBarriers.mMemoryCount;

            Record( [] {
                glMemoryBarrier( GL_ALL_BARRIER_BITS );
            } );
        }
    }

    auto CommandList::SetEnableAutomaticBarriers( bool enable ) -> void {
        // Configuration is valid before Begin() and persists across recordings.
        // Draws/dispatches capture it when recorded, so later changes cannot alter
        // an ended command list or commands recorded before a mid-list toggle.
        mAutomaticBarriers = enable;

        if ( mRecording && enable ) {
            ++mStatistics.mBarriers.mCallCount;
            ++mStatistics.mBarriers.mMemoryCount;

            Record( [] {
                glMemoryBarrier( GL_ALL_BARRIER_BITS );
            } );
        }
    }

    auto CommandList::SetClearColor( renderer::rhi::ITexture *target, renderer::rhi::Color color ) -> void {
        auto texture{ Retain<Texture>( target ) };
        Record( [texture, color] {
            const auto format{ opengl::GetFormat( texture->GetFormat() ) };

            if ( format.mDepth || texture->GetSampleCount() != renderer::rhi::Multisampling::eMsaaX1 ) {
                Unsupported( "standalone depth/MSAA clears; use a render pass clear" );
            }

            if ( format.mInteger ) {

                if ( format.mSigned ) {
                    const GLint values[]{ GLint( color.mR ), GLint( color.mG ), GLint( color.mB ), GLint( color.mA ) };
                    glClearTexImage( texture->Name(), 0, format.mExternal, GL_INT, values );
                }
                else {
                    const GLuint values[]{ GLuint( color.mR ), GLuint( color.mG ), GLuint( color.mB ), GLuint( color.mA ) };
                    glClearTexImage( texture->Name(), 0, format.mExternal, GL_UNSIGNED_INT, values );
                }
            }
            else {
                glClearTexImage( texture->Name(), 0, format.mExternal, GL_FLOAT, &color.mR );
            }
        } );
    }

    auto CommandList::Write( renderer::rhi::IBuffer *target, const void *data, core::usize size ) -> void {
        Write( target, 0, data, size );
    }

    auto CommandList::Write( renderer::rhi::IBuffer *target, core::usize offset, const void *data, core::usize size ) -> void {
        auto buffer{ Retain<Buffer>( target ) };

        if ( offset > target->GetSizeBytes() || size > target->GetSizeBytes() - offset ) {
            MKT_THROW_OUT_OF_RANGE( "Buffer write exceeds allocation." );
        }

        Record( [buffer, offset, bytes = Payload( data, size )] {
            if ( !bytes.empty() ) {
                glNamedBufferSubData( buffer->Name(), offset, bytes.size(), bytes.data() );
            }
        } );
    }

    auto CommandList::Write( renderer::rhi::ITexture *target, const void *data, core::usize size ) -> void {
        auto texture{ Retain<Texture>( target ) };
        Record( [texture, bytes = Payload( data, size )]() mutable {
            glBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0 );
            texture->Upload( bytes.data(), bytes.size() );
        } );
    }

    auto CommandList::Write( renderer::rhi::IBuffer *src, renderer::rhi::ITexture *dest ) -> void {
        auto buffer{ Retain<Buffer>( src ) };
        auto texture{ Retain<Texture>( dest ) };
        Record( [buffer, texture]() mutable {
            glBindBuffer( GL_PIXEL_UNPACK_BUFFER, buffer->Name() );

            try {
                texture->Upload( nullptr, buffer->GetSizeBytes() );
            } catch ( ... ) {
                glBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0 );
                throw;
            }

            glBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0 );
        } );
    }

    auto CommandList::Copy( renderer::rhi::IBuffer *src, renderer::rhi::IBuffer *dest ) -> void {
        Copy( src, dest, 0 );
    }

    auto CommandList::Copy( renderer::rhi::IBuffer *src, renderer::rhi::IBuffer *dest, core::usize offset ) -> void {
        auto source{ Retain<Buffer>( src ) };
        auto destination{ Retain<Buffer>( dest ) };

        if ( src == dest || offset > dest->GetSizeBytes() || src->GetSizeBytes() > dest->GetSizeBytes() - offset ) {
            MKT_THROW_INVALID_ARGUMENT( "Invalid buffer copy range." );
        }

        Record( [source, destination, offset] {
            glCopyNamedBufferSubData( source->Name(), destination->Name(), 0, offset, source->GetSizeBytes() );
        } );
    }

    auto CommandList::Copy( renderer::rhi::ITexture *src, const renderer::rhi::TextureSlice &srcSlice, renderer::rhi::ITexture *dest, const renderer::rhi::TextureSlice &destSlice ) -> void {
        auto source{ Retain<Texture>( src ) };
        auto destination{ Retain<Texture>( dest ) };
        ValidateSlice( *source, srcSlice );
        ValidateSlice( *destination, destSlice );

        if ( srcSlice.mWidth != destSlice.mWidth || srcSlice.mHeight != destSlice.mHeight || srcSlice.mDepth != destSlice.mDepth ||
             src->GetFormat() != dest->GetFormat() || src->GetSampleCount() != dest->GetSampleCount() ) {
            MKT_THROW_INVALID_ARGUMENT( "Texture copy requires matching extents, formats and sample counts." );
        }

        Record( [source, destination, srcSlice, destSlice] {
            glCopyImageSubData( source->Name(), source->Target(), srcSlice.mMipLevel, srcSlice.x, srcSlice.y, srcSlice.mArrayLayer,
                destination->Name(), destination->Target(), destSlice.mMipLevel, destSlice.x, destSlice.y,
                destSlice.mArrayLayer, srcSlice.mWidth, srcSlice.mHeight, srcSlice.mDepth );
        } );
    }

    auto CommandList::Resolve( renderer::rhi::ITexture *src, const renderer::rhi::TextureSlice &a, renderer::rhi::ITexture *dest, const renderer::rhi::TextureSlice &b ) -> void {
        auto source{ Retain<Texture>( src ) };
        auto destination{ Retain<Texture>( dest ) };
        ValidateSlice( *source, a );
        ValidateSlice( *destination, b );

        if ( src->GetSampleCount() == renderer::rhi::Multisampling::eMsaaX1 || dest->GetSampleCount() != renderer::rhi::Multisampling::eMsaaX1 ||
             src->GetFormat() != dest->GetFormat() || a.mWidth != b.mWidth || a.mHeight != b.mHeight || a.mDepth != b.mDepth ) {
            MKT_THROW_INVALID_ARGUMENT( "Invalid MSAA resolve." );
        }

        Record( [source, destination, a, b] {
            GLuint fbos[2]{};
            glCreateFramebuffers( 2, fbos );
            const auto format{ opengl::GetFormat( source->GetFormat() ) };
            const GLenum attachment{ static_cast<GLenum>(
                format.mDepth ? ( format.mStencil ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT ) : GL_COLOR_ATTACHMENT0 ) };
            const GLbitfield mask{ static_cast<GLbitfield>(
                format.mDepth ? GL_DEPTH_BUFFER_BIT | ( format.mStencil ? GL_STENCIL_BUFFER_BIT : 0 ) : GL_COLOR_BUFFER_BIT ) };
            glDisable( GL_SCISSOR_TEST );

            for ( core::u32 layer{}; layer < a.mDepth; ++layer ) {
                source->Attach( fbos[0], attachment, { a.mMipLevel, 1, a.mArrayLayer + layer, 1 } );
                destination->Attach( fbos[1], attachment, { b.mMipLevel, 1, b.mArrayLayer + layer, 1 } );
                glNamedFramebufferReadBuffer( fbos[0], format.mDepth ? GL_NONE : GL_COLOR_ATTACHMENT0 );
                glNamedFramebufferDrawBuffer( fbos[1], format.mDepth ? GL_NONE : GL_COLOR_ATTACHMENT0 );
                glBlitNamedFramebuffer( fbos[0], fbos[1], a.x, a.y, a.x + a.mWidth, a.y + a.mHeight, b.x, b.y, b.x + b.mWidth,
                    b.y + b.mHeight, mask, GL_NEAREST );
            }

            glDeleteFramebuffers( 2, fbos );
        } );
    }

    auto CommandList::Copy( renderer::rhi::IBuffer *dest, renderer::rhi::ITexture *src ) -> void {
        if ( !src ) {
            MKT_THROW_INVALID_ARGUMENT( "Texture readback source is null." );
        }

        Copy( dest, src, CreateFullSlice( *src ) );
    }

    auto CommandList::Copy( renderer::rhi::IBuffer *dest, renderer::rhi::ITexture *src, const renderer::rhi::TextureSlice &slice ) -> void {
        auto source{ Retain<Texture>( src ) };
        auto destination{ Retain<Buffer>( dest ) };

        ValidateSlice( *source, slice );

        const auto format{ opengl::GetFormat( src->GetFormat() ) };
        const core::usize size{ core::usize{ slice.mWidth } * slice.mHeight * slice.mDepth * format.mBytes };

        if ( size > dest->GetSizeBytes() || size > INT_MAX || src->GetSampleCount() != renderer::rhi::Multisampling::eMsaaX1 ) {
            MKT_THROW_INVALID_ARGUMENT( "Readback buffer is too small or source is multisampled." );
        }

        Record( [source, destination, slice, format, size] {
            GLint alignment{};
            glGetIntegerv( GL_PACK_ALIGNMENT, &alignment );
            glPixelStorei( GL_PACK_ALIGNMENT, 1 );
            glBindBuffer( GL_PIXEL_PACK_BUFFER, destination->Name() );
            glGetTextureSubImage( source->Name(), slice.mMipLevel, slice.x, slice.y, slice.mArrayLayer, slice.mWidth, slice.mHeight,
                slice.mDepth, format.mExternal, format.mType, static_cast<GLsizei>( size ), nullptr );
            glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );
            glPixelStorei( GL_PACK_ALIGNMENT, alignment );
        } );
    }

    auto CommandList::BeginRenderPass( renderer::rhi::RenderPassDescription &desc ) -> void {
        if ( mInsidePass ) {
            MKT_THROW_LOGIC_ERROR( "Nested render passes are invalid." );
        }

        Record( [this, desc] {
            for ( core::u32 index{}; index < renderer::rhi::kMaxRenderTargets; ++index ) {
                glNamedFramebufferTexture( mFramebuffer, GL_COLOR_ATTACHMENT0 + index, 0, 0 );
            }

            glNamedFramebufferTexture( mFramebuffer, GL_DEPTH_STENCIL_ATTACHMENT, 0, 0 );
            eastl::vector<GLenum> colors{};
            bool srgb{};

            for ( const auto &target: desc.mCurrentRenderTargets ) {
                const auto *texture{ dynamic_cast<const Texture *>( target.mRenderTarget.GetPtr() ) };

                if ( !texture ) {
                    MKT_THROW_INVALID_ARGUMENT( "Invalid color render target." );
                }

                const GLenum attachment{ static_cast<GLenum>( GL_COLOR_ATTACHMENT0 + static_cast<GLenum>( colors.size() ) ) };
                texture->Attach( mFramebuffer, attachment, target.mSubresourceSet );
                colors.push_back( attachment );
                srgb |= renderer::rhi::GetFormatInfo( texture->GetFormat() ).mIsSRGB;
            }

            if ( desc.mDepthTarget.mRenderTarget ) {
                const auto *texture{ dynamic_cast<const Texture *>( desc.mDepthTarget.mRenderTarget.GetPtr() ) };

                if ( !texture ) {
                    MKT_THROW_INVALID_ARGUMENT( "Invalid depth target." );
                }

                const auto format{ opengl::GetFormat( texture->GetFormat() ) };
                texture->Attach( mFramebuffer, format.mStencil ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT,
                    desc.mDepthTarget.mSubresourceSet );
            }

            glNamedFramebufferDrawBuffers( mFramebuffer, static_cast<GLsizei>( colors.size() ), colors.data() );
            glNamedFramebufferReadBuffer( mFramebuffer, colors.empty() ? GL_NONE : colors[0] );

            if ( glCheckNamedFramebufferStatus( mFramebuffer, GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
                MKT_THROW_RUNTIME_ERROR( "OpenGL framebuffer is incomplete." );
            }

            glBindFramebuffer( GL_FRAMEBUFFER, mFramebuffer );

            if ( srgb ) {
                glEnable( GL_FRAMEBUFFER_SRGB );
            }
            else {
                glDisable( GL_FRAMEBUFFER_SRGB );
            }

            glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
            glDepthMask( GL_TRUE );
            glStencilMask( ~0u );

            const auto &area{ desc.mRenderArea };

            glEnable( GL_SCISSOR_TEST );
            glScissor( area.mMinX, area.mMinY, area.mMaxX - area.mMinX, area.mMaxY - area.mMinY );
            GLint index{};

            for ( const auto &target: desc.mCurrentRenderTargets ) {

                if ( target.mLoadOp == renderer::rhi::LoadOp::eClear ) {
                    ClearAttachment( mFramebuffer, GL_COLOR, index, target.mClearColor,
                        opengl::GetFormat( target.mRenderTarget->GetFormat() ) );
                }

                ++index;
            }

            if ( desc.mDepthTarget.mRenderTarget && desc.mDepthTarget.mLoadOp == renderer::rhi::LoadOp::eClear ) {
                ClearAttachment( mFramebuffer, GL_DEPTH, 0, desc.mDepthTarget.mClearColor,
                    opengl::GetFormat( desc.mDepthTarget.mRenderTarget->GetFormat() ) );
            }
        } );
        mInsidePass = true;
    }

    auto CommandList::EndRenderPass() -> void {
        if ( !mInsidePass ) {
            MKT_THROW_LOGIC_ERROR( "No render pass is active." );
        }

        Record( [] {
            glBindFramebuffer( GL_FRAMEBUFFER, 0 );
        } );
        mInsidePass = false;
    }

    auto CommandList::BindPipeline( renderer::rhi::IPipeline *pipeline ) -> void {
        auto retained{ Retain<Pipeline>( pipeline ) };

        ++mStatistics.mPipelineBindCount;

        if ( mAutomaticBarriers ) {
            ++mStatistics.mBarriers.mCallCount;
            ++mStatistics.mBarriers.mMemoryCount;
        }

        Record( [this, retained] {
            retained->Bind( mVertexArray );
            mBoundPipeline = retained.GetPtr();
            mTopology = retained->Topology();
            mHasPipeline = true;
            mBoundType = retained->GetPipelineType();
        } );
    }

    auto CommandList::SetViewportState( const renderer::rhi::ViewportState &state ) -> void {
        SetViewport( state.mViewports );
        SetScissors( state.mScissorRects );
    }

    auto CommandList::SetViewport( eastl::span<const renderer::rhi::Viewport> values ) -> void {
        eastl::vector<renderer::rhi::Viewport> copy( values.begin(), values.end() );
        Record( [copy] {
            if ( copy.empty() ) {
                return;
            }

            for ( core::u32 i{}; i < copy.size(); ++i ) {
                const auto &v{ copy[i] };

                if ( v.mFlip != copy[0].mFlip ) {
                    Unsupported( "mixed viewport origins within one draw" );
                }

                glViewportIndexedf( i, v.mMinX, v.mMinY, v.mMaxX - v.mMinX, v.mMaxY - v.mMinY );
                glDepthRangeIndexed( i, v.mMinZ, v.mMaxZ );
            }

            glClipControl( copy[0].mFlip ? GL_UPPER_LEFT : GL_LOWER_LEFT, GL_ZERO_TO_ONE );
        } );
    }

    auto CommandList::SetScissors( eastl::span<const renderer::rhi::Rect> values ) -> void {
        eastl::vector<renderer::rhi::Rect> copy( values.begin(), values.end() );
        Record( [copy] {
            for ( core::u32 i{}; i < copy.size(); ++i ) {
                const auto &r{ copy[i] };
                glEnablei( GL_SCISSOR_TEST, i );
                glScissorIndexed( i, r.mMinX, r.mMinY, r.mMaxX - r.mMinX, r.mMaxY - r.mMinY );
            }
        } );
    }

    auto CommandList::SetPolygonLineWidth( core::f32 width ) -> void {
        Record( [width] {
            glLineWidth( width );
        } );
    }

    auto CommandList::BindIndexBuffer( renderer::rhi::IBuffer *buffer ) -> void {
        auto retained{ Retain<Buffer>( buffer ) };

        if ( buffer->GetFormat() != renderer::rhi::Format::eR16_UINT && buffer->GetFormat() != renderer::rhi::Format::eR32_UINT ) {
            MKT_THROW_INVALID_ARGUMENT( "Index buffers must use 16-bit or 32-bit unsigned elements." );
        }

        Record( [this, retained] {
            mIndexType = retained->GetDataType() == renderer::rhi::BufferDataType::eUInt16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
            glVertexArrayElementBuffer( mVertexArray, retained->Name() );
            mIndexed = true;
            mIndexSize = retained->GetSizeBytes();
        } );
    }

    auto CommandList::BindIndirectBuffer( renderer::rhi::IBuffer *buffer ) -> void {
        auto retained{ Retain<Buffer>( buffer ) };
        Record( [this, retained] {
            glBindBuffer( GL_DRAW_INDIRECT_BUFFER, retained->Name() );
            mIndirect = true;
            mIndirectSize = retained->GetSizeBytes();
        } );
    }

    auto CommandList::BindVertexBuffer( const renderer::rhi::VertexBufferBinding &binding ) -> void {
        auto retained{ Retain<Buffer>( binding.mBuffer ) };
        Record( [this, retained, binding] {
            glVertexArrayVertexBuffer( mVertexArray, binding.mSlot, retained->Name(), binding.mOffset,
                static_cast<GLsizei>( binding.mElementStride ) );
        } );
    }

    auto CommandList::BindVertexBuffers( eastl::span<const renderer::rhi::VertexBufferBinding> bindings ) -> void {
        for ( const auto &binding: bindings ) {
            BindVertexBuffer( binding );
        }
    }

    auto CommandList::BindPipelineResources( const renderer::rhi::BindResourcesDescription &desc ) -> void {
        for ( const auto &[set, table]: desc.mResourceSets ) {
            auto retained{ Retain<BindingTable>( table ) };
            // Snapshot descriptors while recording; later table updates do not
            // mutate already recorded draws or release their resources early.
            auto snapshot{ retained->Snapshot() };
            Record( [this, set, snapshot] {
                snapshot->Bind();

                for ( auto &[boundSet, boundTable]: mBoundTables ) {

                    if ( boundSet == set ) {
                        boundTable = snapshot;
                        return;
                    }
                }

                mBoundTables.emplace_back( set, snapshot );
            } );
        }

        if ( desc.mPushConstantSize ) {
            SetPushConstants( desc.mPipelineLayout, desc.mPushConstants.data(), desc.mPushConstantSize, desc.mPushConstantVisibility );
        }
    }

    auto CommandList::SetPushConstants( renderer::rhi::IPipelineLayout *layout, const void *data, core::usize size, renderer::rhi::ShaderFlags ) -> void {
        auto retained{ Retain<PipelineLayout>( layout ) };

        if ( size > retained->GetDescription().mPushConstantsSize ) {
            MKT_THROW_OUT_OF_RANGE( "Push constant update exceeds layout." );
        }

        Record( [this, retained, bytes = Payload( data, size )] {
            if ( !bytes.empty() ) {
                glNamedBufferSubData( mPushBuffer, 0, core::as<GLsizeiptr>( bytes.size() ), bytes.data() );
            }

            glBindBufferBase( GL_UNIFORM_BUFFER, PipelineLayout::GetPushConstantBinding(), mPushBuffer );
        } );
    }

    auto CommandList::Draw( const renderer::rhi::DrawArguments &args ) -> void {
        if ( !mInsidePass ) {
            MKT_THROW_LOGIC_ERROR( "Draw requires a render pass." );
        }

        ++mStatistics.mDrawCallCount;

        if ( mAutomaticBarriers ) {
            ++mStatistics.mBarriers.mCallCount;
            ++mStatistics.mBarriers.mMemoryCount;
        }

        Record( [this, args, automaticBarriers = mAutomaticBarriers] {
            if ( !mHasPipeline || mBoundType != renderer::rhi::PipelineType::eGraphics ) {
                MKT_THROW_LOGIC_ERROR( "Draw requires a pipeline." );
            }

            mBoundPipeline->BindCombinedSamplers( mBoundTables );
            glDrawArraysInstancedBaseInstance( mTopology, args.mFirstVertex, args.mVertexCount, args.mInstanceCount, args.mFirstInstance );

            if ( automaticBarriers ) {
                glMemoryBarrier( GL_ALL_BARRIER_BITS );
            }
        } );
    }

    auto CommandList::DrawIndexed( const renderer::rhi::DrawArguments &args ) -> void {
        if ( !mInsidePass ) {
            MKT_THROW_LOGIC_ERROR( "Draw requires a render pass." );
        }

        ++mStatistics.mDrawCallCount;

        if ( mAutomaticBarriers ) {
            ++mStatistics.mBarriers.mCallCount;
            ++mStatistics.mBarriers.mMemoryCount;
        }

        Record( [this, args, automaticBarriers = mAutomaticBarriers] {
            if ( !mHasPipeline || mBoundType != renderer::rhi::PipelineType::eGraphics || !mIndexed ) {
                MKT_THROW_LOGIC_ERROR( "Indexed draw requires a pipeline and index buffer." );
            }

            const auto indexCapacity{ mIndexSize / ( mIndexType == GL_UNSIGNED_SHORT ? 2 : 4 ) };

            if ( args.mFirstIndex > indexCapacity || args.mIndexCount > indexCapacity - args.mFirstIndex ) {
                MKT_THROW_OUT_OF_RANGE( "Indexed draw exceeds the index buffer." );
            }

            mBoundPipeline->BindCombinedSamplers( mBoundTables );
            glDrawElementsInstancedBaseVertexBaseInstance(
                mTopology, args.mIndexCount, mIndexType,
                reinterpret_cast<void *>( core::usize{ args.mFirstIndex } * ( mIndexType == GL_UNSIGNED_SHORT ? 2 : 4 ) ), args.mInstanceCount,
                args.mVertexOffset, args.mFirstInstance );

            if ( automaticBarriers ) {
                glMemoryBarrier( GL_ALL_BARRIER_BITS );
            }
        } );
    }

    auto CommandList::DrawIndirect( core::u32 offset, core::u32 count ) -> void {
        if ( !mInsidePass || offset % 4 ) {
            MKT_THROW_LOGIC_ERROR( "Indirect draw requires a render pass and aligned offset." );
        }

        ++mStatistics.mDrawCallCount;

        if ( mAutomaticBarriers ) {
            ++mStatistics.mBarriers.mCallCount;
            ++mStatistics.mBarriers.mMemoryCount;
        }

        Record( [this, offset, count, automaticBarriers = mAutomaticBarriers] {
            if ( !mHasPipeline || mBoundType != renderer::rhi::PipelineType::eGraphics || !mIndirect ) {
                MKT_THROW_LOGIC_ERROR( "Indirect draw is missing pipeline/buffer." );
            }

            if ( offset > mIndirectSize || core::usize{ count } > ( mIndirectSize - offset ) / sizeof( renderer::rhi::DrawIndirectArguments ) ) {
                MKT_THROW_OUT_OF_RANGE( "Indirect draw exceeds its argument buffer." );
            }

            mBoundPipeline->BindCombinedSamplers( mBoundTables );
            glMultiDrawArraysIndirect( mTopology, reinterpret_cast<void *>( core::usize{ offset } ), count, sizeof( renderer::rhi::DrawIndirectArguments ) );

            if ( automaticBarriers ) {
                glMemoryBarrier( GL_ALL_BARRIER_BITS );
            }
        } );
    }

    auto CommandList::DrawIndexedIndirect( core::u32 offset, core::u32 count ) -> void {
        if ( !mInsidePass || offset % 4 ) {
            MKT_THROW_LOGIC_ERROR( "Indirect draw requires a render pass and aligned offset." );
        }

        ++mStatistics.mDrawCallCount;

        if ( mAutomaticBarriers ) {
            ++mStatistics.mBarriers.mCallCount;
            ++mStatistics.mBarriers.mMemoryCount;
        }

        Record( [this, offset, count, automaticBarriers = mAutomaticBarriers] {
            if ( !mHasPipeline || mBoundType != renderer::rhi::PipelineType::eGraphics || !mIndirect || !mIndexed ) {
                MKT_THROW_LOGIC_ERROR( "Indexed indirect draw is missing pipeline/buffers." );
            }

            if ( offset > mIndirectSize || core::usize{ count } > ( mIndirectSize - offset ) / ( 5 * sizeof( core::u32 ) ) ) {
                MKT_THROW_OUT_OF_RANGE( "Indirect draw exceeds its argument buffer." );
            }

            mBoundPipeline->BindCombinedSamplers( mBoundTables );
            glMultiDrawElementsIndirect( mTopology, mIndexType, reinterpret_cast<void *>( core::usize{ offset } ), count, 5 * sizeof( core::u32 ) );

            if ( automaticBarriers ) {
                glMemoryBarrier( GL_ALL_BARRIER_BITS );
            }
        } );
    }

    auto CommandList::BindShaderTable( rhi::IShaderTable * ) -> void {
        MKT_THROW_RUNTIME_ERROR( "OpenGL does not support ray-tracing shader tables." );
    }

    auto CommandList::DispatchRays( const rhi::RayTracingDispatchDescription & ) -> void {
        MKT_THROW_RUNTIME_ERROR( "OpenGL does not support ray-tracing dispatch." );
    }

    auto CommandList::Dispatch( core::u32 x, core::u32 y, core::u32 z ) -> void {
        if ( mInsidePass ) {
            MKT_THROW_LOGIC_ERROR( "Dispatch cannot run inside a render pass." );
        }

        ++mStatistics.mDispatchCallCount;

        if ( mAutomaticBarriers ) {
            ++mStatistics.mBarriers.mCallCount;
            ++mStatistics.mBarriers.mMemoryCount;
        }

        Record( [this, x, y, z, automaticBarriers = mAutomaticBarriers] {
            if ( !mHasPipeline || mBoundType != renderer::rhi::PipelineType::eCompute ) {
                MKT_THROW_LOGIC_ERROR( "Dispatch requires a compute pipeline." );
            }

            mBoundPipeline->BindCombinedSamplers( mBoundTables );
            glDispatchCompute( x, y, z );

            if ( automaticBarriers ) {
                glMemoryBarrier( GL_ALL_BARRIER_BITS );
            }
        } );
    }

    auto CommandList::BeginDebugLabel( eastl::string_view name, renderer::rhi::Color ) -> void {
        Record( [label = eastl::string{ name }] {
            glPushDebugGroup( GL_DEBUG_SOURCE_APPLICATION, 0, static_cast<GLsizei>( label.size() ), label.c_str() );
        } );
    }

    auto CommandList::EndDebugLabel() -> void {
        Record( [] {
            glPopDebugGroup();
        } );
    }

    auto CommandList::GetQueueType() const -> rhi::QueueType {
        return mQueueType;
    }

    auto CommandList::Reset( rhi::IQueryPool *queryPool ) -> void {
        auto &pool{ ValidateQueryPool( queryPool ) };
        auto &commands{ *this };

        if ( pool.mActive != ~0u ) {
            MKT_THROW_RUNTIME_ERROR( "Cannot reset an active query pool." );
        }

        // GL query objects are reused by the next Begin/QueryCounter. The caller
        // must obey IQueryPool's completion-fence rule before resetting a pool.
        dynamic_cast<CommandList &>( commands ).Record( [keep = core::Ref<QueryPool>{ &pool }] {
        } );
        eastl::fill( pool.mSlots.begin(), pool.mSlots.end(), QueryPool::SlotState::eReady );
    }

    auto CommandList::BeginQuery( rhi::IQueryPool *queryPool, core::u32 index ) -> void {
        auto &pool{ ValidateQueryPool( queryPool ) };
        auto &commands{ *this };

        pool.ValidateRange( index, 1 );

        if ( pool.mDescription.mType == renderer::rhi::QueryType::eTimestamp || pool.mSlots[index] != QueryPool::SlotState::eReady || pool.mActive != ~0u ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid query begin or overlapping query range." );
        }

        dynamic_cast<CommandList &>( commands ).Record( [keep = core::Ref<QueryPool>{ &pool }, index] {
            for ( core::u32 i{}; i < keep->mTargets.size(); ++i ) {
                const auto target{ keep->mTargets[i] };
                const auto stream{ keep->mDescription.mType == renderer::rhi::QueryType::eTransformFeedback ? keep->mDescription.mStream : 0 };
                GLint active{};
                glGetQueryIndexediv( target, stream, GL_CURRENT_QUERY, &active );

                if ( active ) {
                    MKT_THROW_RUNTIME_ERROR( "Another OpenGL query of this type is already active." );
                }
            }

            for ( core::u32 i{}; i < keep->mTargets.size(); ++i ) {
                const auto stream{ keep->mDescription.mType == renderer::rhi::QueryType::eTransformFeedback ? keep->mDescription.mStream : 0 };
                glBeginQueryIndexed( keep->mTargets[i], stream, keep->mQueries[index * keep->mTargets.size() + i] );
            }
        } );
        pool.mSlots[index] = QueryPool::SlotState::eActive;
        pool.mActive = index;
        pool.mActiveCommands = &commands;
    }

    auto CommandList::EndQuery( rhi::IQueryPool *queryPool, core::u32 index ) -> void {
        auto &pool{ ValidateQueryPool( queryPool ) };
        auto &commands{ *this };

        pool.ValidateRange( index, 1 );

        if ( pool.mActive != index || pool.mSlots[index] != QueryPool::SlotState::eActive || pool.mActiveCommands != &commands ) {
            MKT_THROW_RUNTIME_ERROR( "Query end does not match its begin." );
        }

        dynamic_cast<CommandList &>( commands ).Record( [keep = core::Ref<QueryPool>{ &pool }] {
            for ( const auto target: keep->mTargets ) {
                glEndQueryIndexed( target, keep->mDescription.mType == renderer::rhi::QueryType::eTransformFeedback ? keep->mDescription.mStream : 0 );
            }
        } );
        pool.mSlots[index] = QueryPool::SlotState::eEnded;
        pool.mActive = ~0u;
        pool.mActiveCommands = nullptr;
    }

    auto CommandList::WriteTimestamp( rhi::IQueryPool *queryPool, core::u32 index ) -> void {
        auto &pool{ ValidateQueryPool( queryPool ) };
        auto &commands{ *this };

        pool.ValidateRange( index, 1 );

        if ( pool.mDescription.mType != renderer::rhi::QueryType::eTimestamp || pool.mSlots[index] != QueryPool::SlotState::eReady ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid timestamp query write." );
        }

        dynamic_cast<CommandList &>( commands ).Record( [keep = core::Ref<QueryPool>{ &pool }, index] {
            glQueryCounter( keep->mQueries[index], GL_TIMESTAMP );
        } );
        pool.mSlots[index] = QueryPool::SlotState::eEnded;
    }

    auto CommandList::Resolve( rhi::IQueryPool *queryPool, core::u32 first, core::u32 count ) -> void {
        auto &pool{ ValidateQueryPool( queryPool ) };
        auto &commands{ *this };

        pool.ValidateRange( first, count );

        for ( core::u32 i{ first }; i < first + count; ++i ) {

            if ( pool.mSlots[i] != QueryPool::SlotState::eEnded ) {
                MKT_THROW_RUNTIME_ERROR( "Resolve requires completed query ranges." );
            }
        }

        dynamic_cast<CommandList &>( commands ).Record( [keep = core::Ref<QueryPool>{ &pool }] {
        } );
        eastl::fill( pool.mSlots.begin() + first, pool.mSlots.begin() + first + count, QueryPool::SlotState::eResolved );
    }

    auto CommandList::ValidateQueryPool( rhi::IQueryPool *queryPool ) const -> QueryPool & {
        auto *nativePool{ dynamic_cast<QueryPool *>( queryPool ) };

        if ( !nativePool ) {
            MKT_THROW_RUNTIME_ERROR( "Query pool belongs to a different backend or is null." );
        }

        auto &pool{ *nativePool };

        if ( !pool.IsReady() || this->mState != pool.mState || !this->mRecording ) {
            MKT_THROW_RUNTIME_ERROR( "Queries require an initialized pool and a recording command list from the same OpenGL device." );
        }

        if ( GetQueueType() != pool.mDescription.mQueue ) {
            MKT_THROW_RUNTIME_ERROR( "Query pool and command list use different queue types." );
        }

        return pool;
    }
}// namespace mikoto::renderer::opengl
