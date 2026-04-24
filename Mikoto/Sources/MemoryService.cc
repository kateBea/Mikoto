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
// #include <Logging/Logger.hh>
// #include <Memory/MemoryService.hh>
//
// namespace mikoto {
//
//     MemoryService::MemoryService( const MemoryServiceCreateInfo& )
//     {}
//
//     auto MemoryService::Initialize() -> void {
//         MKT_BEGIN_PROFILER_NAMED();
//
//         MKT_CORE_LOGGER_INFO("Initializing MemoryService...");
//
//         m_IsInitialized = true;
//     }
//
//     auto MemoryService::Shutdown() -> void {
//         MKT_BEGIN_PROFILER_NAMED();
//
//         if (!m_IsInitialized) {
//             return;
//         }
//
//         // The Log comes after so we know the service was
//         // initialized before attempting to shut it down
//         MKT_CORE_LOGGER_INFO( "Shutting down MemoryService..." );
//
//         m_IsInitialized = false;
//     }
// }