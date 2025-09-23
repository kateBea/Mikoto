// /**
//  * Texture2D.cc
//  * Created by kate on 6/8/23.
//  * */
//
// // C++ Standard Library
//
// // Project Headers
// #include <Core/Logging/Logger.hh>
// #include <Core/Service/RenderService.hh>
// #include <Library/Utility/Types.hh>
// #include <Material/Texture/Texture2D.hh>
// #include <Renderer/Vulkan/VulkanContext.hh>
// #include <Renderer/Vulkan/VulkanTexture2D.hh>
//
//
// namespace Mikoto {
//     auto Texture2D::Create( const Texture2DCreateInfo& createInfo ) -> Scope_T<Texture2D> {
//         switch(RenderService::GetInstance()->GetActiveGraphicsApi()) {
//             case GraphicsAPI::VULKAN_API:
//                 return VulkanTexture::Create( VulkanTexture2DCreateInfo{
//                     .Path{ createInfo.Path },
//                     .Name{ createInfo.Name },
//
//                     .Type{ createInfo.Type },
//                     .RetainFileData{ false },
//
//                     .Width{ createInfo.Width },
//                     .Height{ createInfo.Height },
//                     .ChannelCount{ createInfo.ChannelCount },
//
//                     .Format{ createInfo.Format },
//
//                     .BufferData{ createInfo.BufferData },
//                 } );
//             default:
//                 MKT_CORE_LOGGER_CRITICAL("Texture2D::Create - Unsupported renderer API");
//             break;
//         }
//
//         return nullptr;
//     }
//
//     auto Texture2D::Create(const Path_T& path, TextureType type) -> Scope_T<Texture2D> {
//         auto& renderSystem{ ServiceInitializer::GetSystem<RenderSystem>() };
//         switch(renderSystem.GetDefaultApi()) {
//             case GraphicsAPI::VULKAN_API:
//                 return VulkanTexture::Create( VulkanTexture2DCreateInfo{
//                     .Path{ path },
//                     .Type{ type },
//                     .RetainFileData{ false }
//                 } );
//             default:
//                 MKT_CORE_LOGGER_CRITICAL("Texture2D::Create - Unsupported renderer API");
//             break;
//         }
//
//         return nullptr;
//     }
// }