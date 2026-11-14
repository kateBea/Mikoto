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

#include <EASTL/utility.h>
#include <EASTL/numeric_limits.h>

#include <Core/Exception.hh>

#include <Renderer/Rhi/OpenGL/OpenGLBuffer.hh>
#include <Renderer/Rhi/OpenGL/OpenGLHelpers.hh>

namespace mikoto::renderer::opengl {

    Buffer::Buffer( const renderer::rhi::BufferCreateDescription &desc, eastl::shared_ptr<DeviceState> state )
        : mUploadContents{ desc.mSpanHandle }, mElementCount{ desc.mElementCount }, mElementSize{ desc.mElementSize }, mDataType{ desc.mDataType }, mUsage{ desc.mUsageFlags }, mInitialState{ desc.mInitialState }, mIsVolatile{ desc.mIsVolatile }, mMaxVersions{ desc.mMaxVersions }, mFormat{ desc.mFormat }, mState{ eastl::move( state ) }, mKeepInitializers{ desc.mKeepInitializerResources } {
        mHeapType = desc.mHeapType;
        mResourceType = desc.mResourceType;
        mCpuAccess = desc.mCpuAccess;
    }

    Buffer::~Buffer() {
        Destroy();
    }

    auto Buffer::Initialize() -> void {
        mState->RequireOwner();
        const auto size{ GetSizeBytes() };

        if ( !size || size > static_cast<core::usize>( eastl::numeric_limits<GLsizeiptr>::max() ) ) {
            MKT_THROW_INVALID_ARGUMENT( "Invalid OpenGL buffer size." );
        }

        if ( mUploadContents && mUploadContents->GetSize() < size ) {
            MKT_THROW_INVALID_ARGUMENT( "Buffer initializer is smaller than its allocation." );
        }

        glCreateBuffers( 1, &mName );
        GLbitfield flags{ GL_DYNAMIC_STORAGE_BIT };

        if ( mHeapType == renderer::rhi::HeapType::eUpload || mCpuAccess == renderer::rhi::AccessType::eWrite ) {
            flags |= GL_MAP_WRITE_BIT;
        }

        if ( mHeapType == renderer::rhi::HeapType::eReadback || mCpuAccess == renderer::rhi::AccessType::eRead ) {
            flags |= GL_MAP_READ_BIT;
        }

        glNamedBufferStorage( mName, size, mUploadContents ? mUploadContents->GetData() : nullptr, flags );
        CheckError( "buffer allocation" );
        mState->mAllocatedBytes += size;

        if ( !mKeepInitializers ) {
            mUploadContents.Reset();
        }

        SetResourceState( mInitialState );
        mIsAllocated = true;
    }

    auto Buffer::Destroy() -> void {
        if ( mName ) {
            const auto name{ eastl::exchange( mName, 0 ) };
            const auto size{ IsReady() ? GetSizeBytes() : 0 };
            const auto state{ mState.get() };
            mState->Retire( [name, size, state] {
                glDeleteBuffers( 1, &name );
                state->mAllocatedBytes -= size;
            } );
        }

        mIsAllocated = false;
    }

    auto Buffer::GetShaderDeviceAddress() -> rhi::DeviceAddress {
        return GetGpuDeviceAddress();
    }

    auto Buffer::GetGpuDeviceAddress() -> renderer::rhi::DeviceAddress {
        return kNullDeviceAddress;
    }

    auto Buffer::GetNativeHandle( renderer::rhi::ObjectType type ) -> renderer::rhi::Object {
        return type == renderer::rhi::ObjectType::GL_Buffer ? renderer::rhi::Object{ core::u64{ mName } } : renderer::rhi::Object{};
    }

    auto Buffer::Map() -> void * {
        mState->RequireOwner();

        if ( mMapped ) {
            MKT_THROW_LOGIC_ERROR( "Buffer is already mapped." );
        }

        const bool read{ mHeapType == renderer::rhi::HeapType::eReadback || mCpuAccess == renderer::rhi::AccessType::eRead };

        if ( !read && mHeapType != renderer::rhi::HeapType::eUpload && mCpuAccess != renderer::rhi::AccessType::eWrite ) {
            MKT_THROW_LOGIC_ERROR( "Cannot map a device-only buffer." );
        }

        void *result{ glMapNamedBufferRange( mName, 0, GetSizeBytes(), read ? GL_MAP_READ_BIT : GL_MAP_WRITE_BIT ) };
        CheckError( "buffer mapping" );

        if ( !result ) {
            MKT_THROW_RUNTIME_ERROR( "OpenGL buffer mapping failed." );
        }

        mMapped = true;
        return result;
    }

    auto Buffer::UnMap() -> void {
        mState->RequireOwner();

        if ( !mMapped ) {
            MKT_THROW_LOGIC_ERROR( "Buffer is not mapped." );
        }

        mMapped = false;

        if ( !glUnmapNamedBuffer( mName ) ) {
            MKT_THROW_RUNTIME_ERROR( "OpenGL mapped buffer contents became invalid." );
        }
    }

    auto Buffer::GetUsage() const -> rhi::BufferUsageFlags {
        return mUsage;
    }

    auto Buffer::GetDataType() const -> rhi::BufferDataType {
        return mDataType;
    }

    auto Buffer::GetData() const -> memory::BufferSpanHandle {
        return mUploadContents;
    }

    auto Buffer::GetSizeBytes() const -> core::usize {
        return mElementCount == 0 ? mElementSize : mElementCount * mElementSize;
    }

    auto Buffer::GetFormat() const -> rhi::Format {
        return mFormat;
    }

    auto Buffer::GetCount() const -> core::usize {
        return mElementCount == 0 ? rhi::InferElementCount( mFormat, mElementSize ) : mElementCount;
    }

    auto Buffer::Name() const -> GLuint {
        return mName;
    }
}// namespace mikoto::renderer::opengl
