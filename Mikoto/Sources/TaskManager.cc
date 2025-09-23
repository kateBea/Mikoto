// //
// // Created by zanet on 3/27/2025.
// //
//
// #include "Threading/TaskManager.hh"
//
// namespace Mikoto {
//
//     TaskManager::TaskManager( UInt32_T numThreads )
//         : m_ThreadCount{ numThreads }
//     {
//     }
//
//     auto TaskManager::Init() -> void {
//         m_TaskScheduler = CreateScope<enki::TaskScheduler>();
//
//         const enki::TaskSchedulerConfig config{
//             .numTaskThreadsToCreate{ m_ThreadCount },
//         };
//
//         m_TaskScheduler->Initialize(config);
//     }
//
//     auto TaskManager::Shutdown() -> void {
//         m_TaskScheduler->WaitforAll();
//         m_TaskScheduler->ShutdownNow();
//     }
//
//     auto TaskManager::SubmitTask( Task *task ) -> void {
//
//     }
//
//     auto TaskManager::SubmitPinnedTask( Task *task, UInt32_T threadIndex ) -> void {
//     }
//
//     auto TaskManager::WaitForAllTasks() const -> void {
//         m_TaskScheduler->AddTaskSetToPipe( m_TaskBatch.get() );
//         m_TaskScheduler->WaitforTask( m_TaskBatch.get() );
//     }
// }// namespace Mikoto