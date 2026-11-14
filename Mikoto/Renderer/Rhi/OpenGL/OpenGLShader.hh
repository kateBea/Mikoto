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

#ifndef MIKOTO_RENDERER_RHI_OPENGL_OPENGL_SHADER_HH
#define MIKOTO_RENDERER_RHI_OPENGL_OPENGL_SHADER_HH

#include <Renderer/Rhi/Shader.hh>
#include <Renderer/Rhi/OpenGL/OpenGLDevice.hh>

namespace mikoto::renderer::opengl {

    struct CombinedSamplerBinding {
        eastl::string mName{};
        core::u32 mTextureSlot{};
        core::u32 mTextureSet{};
        core::u32 mSamplerSlot{};
        core::u32 mSamplerSet{};
        bool mDummySampler{};
        GLuint mUnit{};
    };

    class Shader final : public renderer::rhi::IShaderModule {
    public:
        Shader( const renderer::rhi::ShaderModuleCreateDescription &desc, eastl::shared_ptr<DeviceState> state );

        auto GetType() const -> rhi::ShaderType override;

        auto DumpShaderCode() -> void override;

        auto GetContents() const -> const void * override;
        auto GetContentsByteSize() const -> core::usize override;
        auto GetCombinedSamplers() const -> const eastl::vector<CombinedSamplerBinding> &;

        auto Name() const -> GLuint;

        ~Shader() override;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        eastl::string mEntryPoint{};
        rhi::ShaderType mStage{ rhi::ShaderType::eInvalid };
        rhi::ShaderLanguage mLanguage{ rhi::ShaderLanguage::eInvalid };

        // Context ownership and native resource state.
        eastl::shared_ptr<DeviceState> mState{};
        eastl::vector<core::u32> mCode{};
        core::usize mByteSize{};
        eastl::string mModuleName{};
        eastl::string mModulePath{};
        eastl::vector<CombinedSamplerBinding> mCombinedSamplers{};
        GLuint mName{};
    };
}// namespace mikoto::renderer::opengl

#endif// MIKOTO_RENDERER_RHI_OPENGL_OPENGL_SHADER_HH
