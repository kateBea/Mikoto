//
// Created by kate on 11/1/25.
//

/**
 * @file Renderer.hh
 * @brief Mikoto Renderer module.
 *
 * Renderer-related headers in Mikoto;
 */

#ifndef MIKOTO_RENDERING_HH
#define MIKOTO_RENDERING_HH

// ===========================================================
// Core Renderer Abstractions
// ===========================================================
#include <Renderer/Buffer.hh>
#include <Renderer/DeviceObject.hh>
#include <Renderer/FontFactory.hh>
#include <Renderer/Framebuffer.hh>
#include <Renderer/GpuDevice.hh>
#include <Renderer/ImportFont.hh>
#include <Renderer/Light.hh>
#include <Renderer/Pipeline.hh>
#include <Renderer/RendererBackend.hh>
#include <Renderer/RenderPassBase.hh>
#include <Renderer/RenderService.hh>
#include <Renderer/RenderUtility.hh>
#include <Renderer/SceneRenderer.hh>

// ===========================================================
// Vulkan Implementation
// ===========================================================
#include <Renderer/Vulkan/VulkanBuffer.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanDescriptorManager.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>
#include <Renderer/Vulkan/VulkanFramebuffer.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>
#include <Renderer/Vulkan/VulkanMemoryAllocator.hh>
#include <Renderer/Vulkan/VulkanPasses.hh>
#include <Renderer/Vulkan/VulkanPipeline.hh>
#include <Renderer/Vulkan/VulkanRenderer.hh>
#include <Renderer/Vulkan/VulkanShader.hh>
#include <Renderer/Vulkan/VulkanTexture.hh>

#endif // MIKOTO_RENDERING_HH
