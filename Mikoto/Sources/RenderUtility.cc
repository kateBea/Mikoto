// //
// // Created by zanet on 4/12/2025.
// //
//
// #include <Common/Common.hh>
// #include <Library/Utility/Types.hh>
// #include <Renderer/RenderUtility.hh>
//
// namespace Mikoto {
//     MKT_NODISCARD auto LoadImageFromFile( const File* textureFile, Int32_T& outWidth, Int32_T& outHeight, Int32_T& outChannels ) -> stbi_uc* {
//         stbi_set_flip_vertically_on_load( true );
//
//         constexpr int targetChannelCount{ STBI_rgb_alpha };
//         stbi_uc* data{ stbi_load_from_memory(
//                 Cast<stbi_uc*>( textureFile->GetFileBytes() ),
//                 textureFile->GetFileContents().size(),
//                 std::addressof( outWidth ),
//                 std::addressof( outHeight ),
//                 std::addressof( outChannels ),
//                 targetChannelCount ) };
//
//         if ( !data ) {
//             MKT_THROW_RUNTIME_ERROR( fmt::format( "LoadImageFromFile - Failed to load texture image: [{}]", textureFile->GetPathCStr() ) );
//         }
//
//         outChannels = 4;
//         return data;
//     }
//
//     MKT_NODISCARD auto FreeImageData( Byte_T* data ) -> void {
//         stbi_image_free( data );
//     }
//
//     StbImage::StbImage( const File* textureFile ) {
//         m_Data = LoadImageFromFile( textureFile, m_Width, m_Height, m_Channels );
//     }
//
//     StbImage::~StbImage() {
//         if ( m_Data ) {
//             stbi_image_free( m_Data );
//             m_Data = nullptr;
//         }
//     }
//
//     StbImage::StbImage( StbImage&& other ) noexcept
//         : m_Width( other.m_Width ),
//           m_Height( other.m_Height ),
//           m_Channels( other.m_Channels ),
//           m_Data( other.m_Data ) {
//         other.m_Data = nullptr;
//         other.m_Width = 0;
//         other.m_Height = 0;
//         other.m_Channels = 0;
//     }
//
//     auto StbImage::operator=( StbImage&& other ) noexcept -> StbImage& {
//         if ( this != &other ) {
//             if ( m_Data ) {
//                 stbi_image_free( m_Data );
//             }
//
//             m_Data = other.m_Data;
//             m_Width = other.m_Width;
//             m_Height = other.m_Height;
//             m_Channels = other.m_Channels;
//
//             other.m_Data = nullptr;
//             other.m_Width = 0;
//             other.m_Height = 0;
//             other.m_Channels = 0;
//         }
//
//         return *this;
//     }
// }// namespace Mikoto