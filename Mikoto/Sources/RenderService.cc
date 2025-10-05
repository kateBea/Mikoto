// /**
//  * Renderer.cc
//  * Created by kate on 6/5/23.
//  * */
//
// // C++ Standard Library
// #include <memory>
// #include <utility>
//
// // Project Headers
// #include <Common/Common.hh>
// #include <Logging/Logger.hh>
// #include <Renderer/RenderService.hh>
// #include <Renderer/Vulkan/VulkanContext.hh>
//
// namespace Mikoto {
//
//     RenderService::RenderService(const RenderServiceCreateInfo& options)
//         : m_Options{ options }
//     {}
//
//     auto RenderService::Init() -> void {
//         MKT_CORE_LOGGER_INFO("Initializing RenderService...");
//
//         const RenderContextCreateInfo createInfo{
//             .TargetWindow{ m_Options.TargetWindow },
//         };
//
//         m_BackendPool.Init( 10 );
//
//         m_Context = RenderContext::Create(createInfo);
//         if (!m_Context->Init()) {
//             MKT_THROW_RUNTIME_ERROR( "RenderSystem::Init - Could not initialize Render context." );
//         }
//
//         m_IsInitialized = true;
//     }
//
//     auto RenderService::Shutdown() -> void {
//         if (!m_IsInitialized) {
//             return;
//         }
//
//         // The Log comes after so we know the service was
//         // initialized before attempting to shut it down
//         MKT_CORE_LOGGER_INFO( "Shutting down AudioService..." );
//
//         m_Context->Shutdown();
//         m_Context = nullptr;
//
//         m_IsInitialized = false;
//     }
//
//     auto RenderService::Update(float ts) -> void {
//
//     }
//
//     auto RenderService::PrepareFrame() const -> void {
//         m_Context->PrepareFrame();
//     }
//
//     auto RenderService::EndFrame() -> void {
//         Flush();
//     }
//
//     auto RenderService::CreateBackend() -> RendererBackend * {
//         RendererDescription description{
//                 .Name{ "Editor Main renderer " },
//                 .GraphicsDevice{ m_Context->GetGraphicsDevice() },
//                 .RendererAPI{ m_Options.RendererAPI },
//         };
//         RendererBackend* result{ m_BackendPool.Allocate( description ) };
//
//         if ( result ) {
//             result->Init();
//         } else {
//             MKT_CORE_LOGGER_ERROR( "RenderService::CreateBackend - Failed to create the editor renderer." );
//         }
//
//         return result;
//     }
//
//     auto RenderService::Flush() -> void {
//
//         m_Context->SubmitFrame();
//     }
// }
