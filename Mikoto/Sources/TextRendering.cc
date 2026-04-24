//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// #include <Core/Profiler.hh>
// #include <Renderer/Core/CommandContext.hh>
// #include <Renderer/Passes/TextRendering.hh>
//
// namespace mikoto {
//
//     TextRendering::TextRendering( RenderResolution resolution )
//         : m_Resolution{ resolution }
//     {}
//
//     auto TextRendering::SetScene( const Scene *scene ) -> void {
//     }
//
//     auto TextRendering::SetCamera( const Camera *camera ) -> void {
//     }
//
//     auto TextRendering::RegisterPasses( FrameGraph &graph, GpuDevice *device ) -> void {
//         RegisterSlugPass( graph, device );
//         RegisterDebugPass( graph, device );
//     }
//
//     auto TextRendering::RegisterSlugPass( FrameGraph &graph, GpuDevice *device ) -> void {
//         MKT_BEGIN_PROFILER_NAMED();
//
//         graph.RegisterPass(
//             "SlugTextRendering",
//             []( FramePassBuilder& b ) {
//                 MKT_BEGIN_PROFILER_NAMED();
//             },
//             []( CommandContext& ctx, FrameGraphBlackboard& ) -> void {
//                 MKT_BEGIN_PROFILER_NAMED();
//             } );
//     }
//
//     auto TextRendering::RegisterDebugPass( FrameGraph &graph, GpuDevice *device ) -> void {
//         MKT_BEGIN_PROFILER_NAMED();
//
//         // This is used to render text on top of an image
//         // Would be used to for instance render debug information on top of the final
//         // image like frame time, or a specific type of effect's properties
//         graph.RegisterPass(
//             "TextDebugPass",
//             []( FramePassBuilder& b ) {
//                 MKT_BEGIN_PROFILER_NAMED();
//             },
//             []( CommandContext& ctx, FrameGraphBlackboard& ) -> void {
//                 MKT_BEGIN_PROFILER_NAMED();
//             } );
//     }
// }// namespace Mikoto