// /**
//  * RendererAPI.cc
//  * Created by kate on 6/9/23.
//  * */
//
// // C++ Standard Library
//
// // Project Headers
// #include <Core/Logger.hh>
// #include <Library/Utility/Types.hh>
// #include <Renderer/RenderService.hh>
// #include <Renderer/RendererBackend.hh>
// #include <Renderer/Vulkan/VulkanContext.hh>
// #include <Renderer/Vulkan/VulkanRenderer.hh>
//
// namespace Mikoto {
//
//     auto RendererBackend::Create( const RendererDescription& createInfo ) -> Scope_T<RendererBackend> {
//         VulkanRendererCreateInfo vulkanRendererCreateInfo{
//             .Info{ createInfo },
//         };
//
//         switch ( RenderService::GetInstance()->GetActiveGraphicsApi() ) {
//             case GraphicsAPI::VULKAN_API:
//
//                 return CreateScope<VulkanRenderer>(vulkanRendererCreateInfo);
//             default:
//                 MKT_CORE_LOGGER_CRITICAL( "RendererBackend::Create - Error Unsupported renderer API!" );
//                 break;
//         }
//
//         return nullptr;
//     }
// }