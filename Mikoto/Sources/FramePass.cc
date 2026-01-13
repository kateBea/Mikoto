//
// Created by kate on 11/24/25.
//

#include <Renderer/Core/FramePass.hh>

namespace Mikoto {

    auto FramePass::PostExecute() -> void {
        m_HasExecuted = true;
        m_IsDirty = false;

        if (m_ExecutePolicy != FramePassExecutionPolicy::PER_FRAME) {
            m_Status = FramePassStatus::SLEEPING;
        }
    }

    auto FramePass::MarkDirty() -> void {
        m_IsDirty = true;
        m_Status = FramePassStatus::ACTIVE;
    }

    auto FramePass::ShouldRun() const -> bool {
        if (!IsStatus(FramePassStatus::ACTIVE)) {
            return false;
        }

        switch (m_ExecutePolicy) {
            case FramePassExecutionPolicy::PER_FRAME:
                return true;

            case FramePassExecutionPolicy::ONCE:
                return !m_HasExecuted;

            case FramePassExecutionPolicy::ON_CHANGE:
                return m_IsDirty;
        }

        return false;
    }

}// namespace Mikoto
