//
// Created by kate on 12/19/25.
//

#ifndef MIKOTO_FRAMEBLACKBOARD_HH
#define MIKOTO_FRAMEBLACKBOARD_HH

#include <string>
#include <string_view>

#include <ankerl/unordered_dense.h>

#include <Assets//Texture.hh>

#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Core/Pipeline.hh>
#include <Renderer/Core/Buffer.hh>
#include <Renderer/Core/FrameResource.hh>

namespace Mikoto {
    class FrameBlackboard {
    public:

        explicit FrameBlackboard(GpuDevice* device);

        auto RegisterTexture(std::string_view name, TextureDescription description) -> void;
        auto RegisterPipeline(std::string_view name, PipelineDescription description) -> void;
        auto RegisterBuffer(std::string_view name, BufferDescription description) -> void;

        auto GetTexture(std::string_view name) -> TextureHandle;
        auto GetPipeline(std::string_view name) -> PipelineHandle;
        auto GetBuffer(std::string_view name) -> BufferHandle;

    private:
        GpuDevice* m_Device{ nullptr };

        ankerl::unordered_dense::map<std::string, TextureHandle> m_TexturesByNames{};
        ankerl::unordered_dense::map<std::string, PipelineHandle> m_PipelinesByNames{};
        ankerl::unordered_dense::map<std::string, BufferHandle> m_BuffersByNames{};
    };

}

#endif//MIKOTO_FRAMEBLACKBOARD_HH
