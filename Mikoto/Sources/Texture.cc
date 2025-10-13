// //
// // Created by zanet on 3/28/2025.
// //
//
// #include <Library/Utility/Types.hh>
// #include <Assets/Texture.hh>
//
// namespace Mikoto {
//
//     auto TextureLoadInfo::WithPath( const Path_T& path ) -> TextureLoadInfo& {
//         Path = path;
//         return *this;
//     }
//
//     auto TextureLoadInfo::WithType( const TextureType type ) -> TextureLoadInfo& {
//         this->Type = type;
//         return *this;
//     }
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