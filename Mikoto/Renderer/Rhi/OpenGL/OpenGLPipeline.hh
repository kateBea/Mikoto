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

#ifndef MIKOTO_RENDERER_RHI_OPENGL_OPENGL_PIPELINE_HH
#define MIKOTO_RENDERER_RHI_OPENGL_OPENGL_PIPELINE_HH

#include <Renderer/Rhi/Pipeline.hh>
#include <Renderer/Rhi/OpenGL/OpenGLDevice.hh>
#include <Renderer/Rhi/OpenGL/OpenGLShader.hh>

namespace mikoto::renderer::opengl {

    class Pipeline final : public renderer::rhi::IPipeline {
    public:
        Pipeline( const renderer::rhi::GraphicsPipelineDescription &desc, eastl::shared_ptr<DeviceState> state );
        Pipeline( const renderer::rhi::ComputePipelineDescription &desc, eastl::shared_ptr<DeviceState> state );

        auto GetPipelineLayout() const -> renderer::rhi::PipelineLayoutHandle override;
        auto GetPipelineType() const -> rhi::PipelineType override;

        auto Bind( GLuint vao ) const -> void;
        auto BindCombinedSamplers( const BindingTableSnapshots &tables ) const -> void;

        auto Topology() const -> GLenum;

        ~Pipeline() override;
    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        rhi::GraphicsPipelineDescription mGraphics{};
        rhi::ComputePipelineDescription mCompute{};

        // Context ownership and native resource state.
        GLuint mProgram{};
        eastl::shared_ptr<DeviceState> mState{};
        eastl::vector<CombinedSamplerBinding> mCombinedSamplers{};

        const rhi::PipelineType mPipelineType{ rhi::PipelineType::eInvalid };
    };
}// namespace mikoto::renderer::opengl

#endif// MIKOTO_RENDERER_RHI_OPENGL_OPENGL_PIPELINE_HH
