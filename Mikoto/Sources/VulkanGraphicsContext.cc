//
// Created by kate on 11/25/25.
//

#include "Renderer/Vulkan/VulkanGraphicsContext.hh"

namespace Mikoto {

    VulkanGraphicsContext::VulkanGraphicsContext() {
    }

    auto VulkanGraphicsContext::Init() -> void {
    }
    auto VulkanGraphicsContext::Shutdown() -> void {
    }
    auto VulkanGraphicsContext::BeginRender() -> void {
    }
    auto VulkanGraphicsContext::EndRender() -> void {
    }
    auto VulkanGraphicsContext::BeginCompute() -> void {
    }
    auto VulkanGraphicsContext::EndCompute() -> void {
    }
    auto VulkanGraphicsContext::SetRenderTarget( TextureHandle texture ) -> void {
    }
    auto VulkanGraphicsContext::SetRenderTarget( TextureHandle color, TextureHandle depth ) -> void {
    }
    auto VulkanGraphicsContext::SetScissor( Int32 x, Int32 y, Int32 width, Int32 height ) -> void {
    }
    auto VulkanGraphicsContext::SetViewport( Int32 x, Int32 y, Int32 width, Int32 height ) -> void {
    }
    auto VulkanGraphicsContext::ClearColor( std::string_view resourceName, TextureHandle colorTarget, const Vec4F &color ) -> void {
    }
    auto VulkanGraphicsContext::ClearColor( std::string_view resourceName, TextureHandle colorTarget, float r, float g, float b, float a ) -> void {
    }
    auto VulkanGraphicsContext::ClearDepth( std::string_view resourceName, TextureHandle depthTarget, float depth ) -> void {
    }
    auto VulkanGraphicsContext::BindPipeline( PipelineHandle pipeline ) -> void {
    }
    auto VulkanGraphicsContext::CreateNamedBuffer( std::string_view name, BufferDescription description ) -> void {
    }
    auto VulkanGraphicsContext::CreateNamedPipeline( std::string_view name, PipelineDescription description ) -> void {
    }
    auto VulkanGraphicsContext::CreateNamedRenderTarget( std::string_view name, TextureDescription description, RenderTargetType ) -> void {
    }
    auto VulkanGraphicsContext::BindBuffer( BufferHandle texture ) -> void {
    }
    auto VulkanGraphicsContext::BindTexture( TextureHandle texture ) -> void {
    }
    auto VulkanGraphicsContext::BindSampler( SamplerHandle sampler ) -> void {
    }
    auto VulkanGraphicsContext::Dispatch( UInt32 invX, UInt32 invY, UInt32 invZ ) -> void {
    }
}// namespace Mikoto