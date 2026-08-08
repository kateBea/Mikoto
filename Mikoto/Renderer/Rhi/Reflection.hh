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

#ifndef MIKOTO_RHI_REFLECTION_HH
#define MIKOTO_RHI_REFLECTION_HH

#include <EASTL/unique_ptr.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Shader.hh>
#include <Renderer/Rhi/Pipeline.hh>
#include <Renderer/Rhi/Descriptor.hh>

namespace mikoto::renderer::rhi {

    // Vulkan for instance via the spirv_reflect library allows us to
    // introspect SPIRV binaries to know descriptor layouts, amongst other
    // information, this could be helpful to generate the appropriate
    // binding layouts from the RHI for our pipeline.

    // TODO: See if it is proper to have this here and extend the API to create resources via API native handles
    // like descriptor set layouts, etc...

    struct ReflectionDescription {
        ShaderLanguage mLanguage{};

        ankerl::unordered_dense::map<ShaderType, ShaderModuleHandle> mShadersModules{};
    };


    class IReflectionModule {
    public:


        virtual ~IReflectionModule() = default;
    };
}

#endif//MIKOTO_RHI_REFLECTION_HH
