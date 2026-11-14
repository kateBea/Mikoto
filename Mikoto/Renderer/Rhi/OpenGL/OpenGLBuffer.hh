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

#ifndef MIKOTO_RENDERER_RHI_OPENGL_OPENGL_BUFFER_HH
#define MIKOTO_RENDERER_RHI_OPENGL_OPENGL_BUFFER_HH

#include <Renderer/Rhi/Buffer.hh>
#include <Renderer/Rhi/OpenGL/OpenGLDevice.hh>

namespace mikoto::renderer::opengl {

    class Buffer final : public renderer::rhi::IBuffer {
    public:
        Buffer( const renderer::rhi::BufferCreateDescription &desc, eastl::shared_ptr<DeviceState> state );

        auto GetUsage() const -> rhi::BufferUsageFlags override;
        auto GetDataType() const -> rhi::BufferDataType override;
        auto GetData() const -> memory::BufferSpanHandle override;
        auto GetSizeBytes() const -> core::usize override;

        auto GetFormat() const -> rhi::Format override;
        auto GetCount() const -> core::usize override;

        auto GetGpuDeviceAddress() -> renderer::rhi::DeviceAddress override;
        auto GetShaderDeviceAddress() -> renderer::rhi::DeviceAddress override;

        auto GetNativeHandle( renderer::rhi::ObjectType type ) -> renderer::rhi::Object override;

        MKT_NODISCARD auto Name() const -> GLuint;
        MKT_NODISCARD auto Map() -> void*;
        auto UnMap() -> void;

        ~Buffer() override;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

        memory::BufferSpanHandle mUploadContents{};

        // These 2 attributes specify that the buffer is typed
        // meaning it will hold mElementCount elements of mElementSize size in bytes
        // This is useful for the backend API to manage alignment as it considers necessary
        // If it is going to be treated as raw bytes
        // then mElementSize holds the total size in bytes
        core::usize mElementCount{};
        core::usize mElementSize{};

        rhi::BufferDataType mDataType{ rhi::BufferDataType::eInvalid };
        rhi::BufferUsageFlags mUsage{ rhi::BufferUsageFlagsBits::None };

        const rhi::ResourceStates mInitialState;

        bool mIsVolatile{};
        core::usize mMaxVersions{ 0 };

        rhi::Format mFormat{ rhi::Format::eUnknown };

        // Context ownership and native resource state.
        eastl::shared_ptr<DeviceState> mState{};
        GLuint mName{};
        bool mMapped{};
        bool mKeepInitializers{};
    };
}// namespace mikoto::renderer::opengl

#endif// MIKOTO_RENDERER_RHI_OPENGL_OPENGL_BUFFER_HH
