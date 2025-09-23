// #include "Renderer/RenderPass.hh"
//
// #include <Renderer/RendererBackend.hh>
// #include <Scene/Component.hh>
// #include <Renderer/Pipeline.hh>
//
// namespace Mikoto {
//
//     GBufferPass::GBufferPass( const GBufferPassDescription &description ) {}
//
//     auto GBufferPass::Init( GpuDevice *device ) -> void {
//
//     }
//
//     auto GBufferPass::Shutdown() -> void {
//
//     }
//
//     auto GBufferPass::Execute( RendererBackend *backend ) -> void {
//         CommandListHandle commandList{ backend->CreateCommandList() };
//         commandList->BeginRecording();
//
//         commandList->BeginRenderPass( this );
//
//         for (const auto &entity: m_Scene->GetEntities() | std::views::values) {
//             if (entity->HasComponent<RenderComponent>()) {
//                 RenderComponent &renderComponent{ entity->GetComponent<RenderComponent>() };
//                 MaterialComponent &materialComponent{ entity->GetComponent<MaterialComponent>() };
//                 TransformComponent &transformComponent{ entity->GetComponent<TransformComponent>() };
//
//                 const auto subMesh{ renderComponent.GetMesh() };
//                 const auto material{ materialComponent.GetMaterial() };
//                 const auto& transform{ transformComponent.GetTransform() };
//
//                 commandList->SubmitMeshDraw( subMesh, material, transform );
//             }
//         }
//
//         commandList->EndRecording();
//         backend->SubmitCommandList( commandList );
//     }
//
//     auto LightCullingPass::Init( GpuDevice *device ) -> void {
//
//     }
//
//     auto LightCullingPass::Shutdown() -> void {
//
//     }
//
//     auto LightCullingPass::Execute( RendererBackend *backend ) -> void {
//         CommandListHandle commandList{ backend->CreateCommandList() };
//         commandList->BeginRecording();
//         commandList->BeginComputePass( this );
//
//         commandList->EndRecording();
//         backend->SubmitCommandList( commandList );
//     }
//
// #if false
// #endif
// }
