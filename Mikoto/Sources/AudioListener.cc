// //
// // Created by zanet on 4/7/2025.
// //
//
// #include <miniaudio.h>
//
// #include "Audio/AudioListener.hh"
// #include <Audio/AudioDevice.hh>
// #include <Audio/AudioService.hh>
//
// namespace Mikoto {
//     auto AudioListener::SetPosition(float x, float y, float z) -> void {
//         m_Position = { x, y, z };
//     }
//
//     auto AudioListener::GetPosition() const -> const glm::vec3& {
//         return m_Position;
//     }
//
//     auto AudioListener::SetOrientation(float forwardX, float forwardY, float forwardZ,
//                                               float upX, float upY, float upZ) -> void {
//         m_Forward = { forwardX, forwardY, forwardZ };
//         m_Up      = { upX, upY, upZ };
//     }
//
//     auto AudioListener::SetVelocity(float x, float y, float z) -> void {
//         m_Velocity = { x, y, z };
//     }
//
//     auto AudioListener::Apply() const -> void {
//         AudioDevice* engine{ AudioService::GetInstance()->GetDevice() };
//
// #if defined(MIKOTO_USE_AUDIO_ENGINE_INTERFACE)
//         ma_engine_listener_set_position(&engine->m_AudioEngine, 0, m_Position.x, m_Position.y, m_Position.z);
//         ma_engine_listener_set_direction(&engine->m_AudioEngine, 0, m_Forward.x, m_Forward.y, m_Forward.z);
//         ma_engine_listener_set_velocity(&engine->m_AudioEngine, 0, m_Velocity.x, m_Velocity.y, m_Velocity.z);
// #endif
//
//     }
// }