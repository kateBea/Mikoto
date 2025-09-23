// //
// // Created by zanet on 3/2/2025.
// //
//
// #include "Assets/Font.hh"
//
// #include <Renderer/RenderService.hh>
// #include "Renderer/Vulkan/VulkanFont.hh"
//
// namespace Mikoto {
//
//     Font::Font( const FontLoadDescription &loadInfo )
//         :m_Path{ loadInfo.Path },
//             m_Name{ loadInfo.Path.stem().string() },
//             m_PixelSize{ loadInfo.PixelSize }
//     {
//         m_Atlas = CreateScope<FontAtlas>( m_Path );
//
//         if (!m_Atlas) {
//             MKT_CORE_LOGGER_ERROR( "Font::Font - Failed to allocate memory for font atlas" );
//         } else {
//             m_Atlas->Init();
//         }
//     }
//
//     auto Font::Create( const FontLoadDescription &loadInfo ) -> Scope_T<Font> {
//         switch (RenderService::GetInstance()->GetActiveGraphicsApi()) {
//
//             case GraphicsAPI::VULKAN_API:
//                 return CreateScope<VulkanFont>( loadInfo );
//             default:
//                 MKT_CORE_LOGGER_ERROR( "Font::Create - No font implementation for the given API" );
//                 break;
//         }
//
//         return nullptr;
//     }
//
// }