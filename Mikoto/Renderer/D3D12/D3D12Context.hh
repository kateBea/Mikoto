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

#ifndef MIKOTO_D3D12CONTEXT_HH
#define MIKOTO_D3D12CONTEXT_HH

#include <Core/Platform.hh>
#include <Renderer/Core/RenderService.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

namespace Mikoto {
    class D3D12Context final : public RenderContext {
    public:
        explicit D3D12Context(const RenderContextCreateInfo& createInfo)
           :  RenderContext{ createInfo }
        { }

        auto Init() -> bool override;
        auto Shutdown() -> void override;

        auto SubmitFrame() -> void override;
        auto PrepareFrame() -> void override;

        auto Update() -> void override;

        auto Present() -> void override;

        auto SetPresentTarget( TextureHandle texture ) -> void override;

        auto EnableVSync() -> void override;
        auto DisableVSync() -> void override;

        MKT_NODISCARD auto IsVsyncEnabled() const -> bool override;

        ~D3D12Context() override = default;

    };
}

#endif


#endif//MIKOTO_D3D12CONTEXT_HH
