// //
// // Created by zanet on 3/27/2025.
// //
//
// #include "Threading/Task.hh"
//
// namespace Mikoto {
//
//
//     Task::Task( const TaskFunction_T &task, TaskPriority priority )
//         : m_Task{ task }, m_Priority{ priority }
//     {
//     }
//     auto Task::Execute() -> void {
//         m_Task();
//     }
//
//     AttachedTask::AttachedTask( const TaskFunction_T &task, UInt32_T threadIndex, TaskPriority priority )
//         : Task{ task, priority }, m_ThreadIndex{ threadIndex }
//     {}
//
//     auto AttachedTask::Execute() -> void {
//         m_Task();
//     }
// }// namespace Mikoto