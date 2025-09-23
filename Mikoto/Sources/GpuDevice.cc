// //
// // Created by zanet on 3/25/2025.
// //
//
// #include <Renderer/GpuDevice.hh>
// #include <Renderer/Vulkan/VulkanDevice.hh>
//
// namespace Mikoto {
//
//     auto GpuDevice::Create( const GpuDeviceCreateInfo &createInfo) -> Scope_T<GpuDevice> {
//         switch ( createInfo.Api ) {
//             case GraphicsAPI::VULKAN_API:
//                 return CreateScope<VulkanDevice>( createInfo );
//             default:
//                 return nullptr;
//         }
//     }
// }// namespace Mikoto
