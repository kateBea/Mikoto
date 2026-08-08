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

#ifndef MIKOTO_IMGUI_D3D12BACKEND_HH
#define MIKOTO_IMGUI_D3D12BACKEND_HH

#include <EASTL/functional.h>

#include <Core/Platform.hh>

#include <ImGui/ImGuiService.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Texture.hh>
#include <Renderer/Rhi/CommandList.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

#include <wrl.h>
#include <directx/d3d12.h>

#include <imgui_impl_dx12.h>

namespace mikoto::gui {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    // Simple free list based allocator
    // Taken from imgui examples
    struct ExampleDescriptorHeapAllocator {
        ID3D12DescriptorHeap* mHeap{ nullptr };

        D3D12_CPU_DESCRIPTOR_HANDLE mHeapStartCpu{};
        D3D12_GPU_DESCRIPTOR_HANDLE mHeapStartGpu{};
        D3D12_DESCRIPTOR_HEAP_TYPE mHeapType{ D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES };

        UINT mHeapHandleIncrement{};
        ImVector<int> mFreeIndices{};

        auto Destroy() -> void;
        auto Create( ID3D12Device* device, ID3D12DescriptorHeap* heap ) -> void;

        auto Alloc( D3D12_CPU_DESCRIPTOR_HANDLE* outCpuDescHandle, D3D12_GPU_DESCRIPTOR_HANDLE* outGpuDescHandle ) -> void;
        auto Free( D3D12_CPU_DESCRIPTOR_HANDLE outCpuDescHandle, D3D12_GPU_DESCRIPTOR_HANDLE outGpuDescHandle ) -> void;
    };

    class ImGuiD3D12Backend final : public ImGuiBackend {
    public:
        explicit ImGuiD3D12Backend( const ImGuiBackendCreateInfo& createInfo );

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto BeginFrame() -> void override;
        auto EndFrame() -> void override;

        MKT_NODISCARD auto GetFinalComposition() -> TextureHandle override;

        MKT_NODISCARD auto ConstructImGuiTextureID( const ITexture* texture ) -> ImTextureID override;
        MKT_NODISCARD auto ConstructImGuiTextureID( TextureHandle texture ) -> ImTextureID override;

    private:
        // [Internal usage]
        auto InitImages() -> void;
        auto InitImGuiForD3D12() -> void;

        auto RecordCommands() -> void;

    private:
        D3D12_RESOURCE_DESC mDimensions{};

        CommandListHandle mCommandList{};

        TextureHandle mColorImage{};
        TextureHandle mDepthImage{};

        // Made inline static so the lambda does not require explicit capture
        // Lambdas deduce to function pointer only if capture list is empty
        // I will have for now ImGui its own SRV descriptor heap and have it manage it
        inline static ExampleDescriptorHeapAllocator mSrvDescHeapAlloc{};
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSrvDescHeap{};

        eastl::function<void(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE*, D3D12_GPU_DESCRIPTOR_HANDLE*)> mSrvDescriptorAllocFn{};
        eastl::function<void(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE)> mSrvDescriptorFreeFn{};
    };
}

#endif

#endif //MIKOTO_IMGUI_D3D12BACKEND_HH