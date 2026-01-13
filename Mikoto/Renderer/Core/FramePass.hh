//    Copyright 2025 ケイト
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

#ifndef MIKOTO_FRAME_PASS_HH
#define MIKOTO_FRAME_PASS_HH

#include <string>
#include <string_view>
#include <vector>

#include <ankerl/unordered_dense.h>

#include <Assets/Model.hh>
#include <Scene/Scene.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/GraphicsContext.hh>

namespace Mikoto {

    enum class FramePassStatus { ACTIVE,
                                 SLEEPING };

    enum class FramePassExecutionPolicy { PER_FRAME,
                                          ON_CHANGE,
                                          ONCE };

    class FramePass {
    public:
        enum class FramePassType { RENDER,
                                   COMPUTE,
                                   UNDEFINED };

        using ResourceHandle = Ref<IResource>;

        /**
        * @brief Virtual destructor for frame passes.
        *
        * Ensures proper cleanup of derived pass implementations.
        */
        virtual ~FramePass() = default;


        /**
        * @brief Declares resource dependencies and execution parameters for the pass.
        *
        * This method is called during frame graph construction and must describe:
        * - Resources read by the pass
        * - Resources written by the pass
        */
        virtual auto Setup(FrameGraphBuilder& builder) -> void = 0;


        /**
        * @brief Records GPU commands for this pass.
        *
        * This method is invoked by the frame graph when the pass is scheduled
        * for execution. All rendering or compute commands must be recorded
        * into the provided command list.
        */
        virtual auto Execute(PassCommandList& cmdList) -> void = 0;

        /**
         * @brief Finalizes pass state after successful execution.
         *
         * Updates internal execution state such as:
         * - Execution flags
         * - Dirty state
         * - Automatic transition to sleeping status when applicable
         *
         * This method is called by the frame graph after Execute().
         */
        auto PostExecute() -> void;


        /**
         * @brief Returns the pipeline type used by this pass.
         *
         * Determines whether the pass is executed using a graphics or compute pipeline.
         */
        MKT_NODISCARD auto GetPassType() const -> FramePassType { return m_PassType; }

        /**
         * @brief Checks whether this pass uses a compute pipeline.
         */
        MKT_NODISCARD auto IsCompute() const -> bool { return m_PassType == FramePassType::COMPUTE; }

        /**
        * @brief Checks whether this pass uses a graphics (render) pipeline.
        */
        MKT_NODISCARD auto IsRender() const -> bool { return m_PassType == FramePassType::RENDER; }

        /**
         * @brief Returns the debug name of the pass.
         *
         * Used for profiling, logging, and render graph visualization.
         */
        MKT_NODISCARD auto GetName() const -> const std::string& { return m_Name; }

        /**
         * @brief Checks whether the pass is currently in the specified status.
         */
        MKT_NODISCARD auto IsStatus(FramePassStatus status) const -> bool { return m_Status == status; }

        /**
        * @brief Sets the current execution status of the pass.
        *
        * Status controls whether the pass is eligible to be executed by the frame graph.
        */
        auto SetPassStatus(FramePassStatus status) -> void { m_Status = status; }

        /**
         * @brief Sets the execution policy that determines when the pass runs.
         */
        auto SetExecutionPolicy(FramePassExecutionPolicy executionPolicy) -> void { m_ExecutePolicy = executionPolicy; }

        /**
        * @brief Returns the execution policy of the pass.
        */
        MKT_NODISCARD auto GetExecutionPolicy() const -> FramePassExecutionPolicy { return m_ExecutePolicy; }


        /**
        * @brief Marks the pass as dirty and reactivates it if necessary.
        *
        * Dirty passes with an ON_DIRTY execution policy will be scheduled
        * for execution on the next frame graph evaluation.
        */

        auto MarkDirty() -> void;

        /**
        * @brief Determines whether the pass should execute during the current frame graph run.
        *
        * This decision is based on:
        * - Pass status
        * - Execution policy
        * - Dirty and execution state flags
        */
        MKT_NODISCARD auto ShouldRun() const -> bool;

    protected:
        explicit FramePass( std::string_view name, FramePassType passType )
            : m_Name{ name }, m_PassType{ passType } {}

    protected:
        std::string m_Name{ "BasePass" };
        FramePassType m_PassType{ FramePassType::UNDEFINED };

        /**
        * @brief Current execution status of the frame pass.
        *
        * Controls whether the pass is eligible for execution by the frame graph.
        *
        * - ACTIVE:
        *   The pass is allowed to execute if its execution policy allows it.
        *
        * - SLEEPING:
        *   The pass is temporarily inactive and will not be executed until it is
        *   explicitly reactivated (e.g. via MarkDirty()).
        *
        * - INACTIVE:
        *   The pass is disabled and will never execute until manually re-enabled.
        *
        * Status does not imply *when* the pass executes — only whether it is
        * eligible to be considered.
        */
        FramePassStatus m_Status{ FramePassStatus::ACTIVE };

        /**
        * @brief Indicates whether the pass has pending changes that require re-execution.
        *
        * This flag is primarily used by passes with an execution policy of
        * ON_DIRTY. When set to true, the pass becomes eligible for execution
        * and will be run on the next frame graph evaluation.
        *
        * The dirty state is typically triggered by:
        * - Changes to input resources
        * - Asset hot-reloads like environment maps
        *
        * The flag is automatically cleared after successful execution.
        */
        bool m_IsDirty{ false };

        /**
        * @brief Indicates whether the pass has already executed at least once.
        *
        * Used by execution policies such as ONCE to ensure that the pass is
        * only executed a single time during its lifetime.
        *
        * This flag is set to true immediately after the pass successfully
        * completes execution.
        *
        * The flag may be reset when the pass is explicitly invalidated or
        * reactivated.
        */
        bool m_HasExecuted{ false };

        /**
        * @brief Defines how often the frame pass is eligible for execution.
        *
        * The execution policy determines when the pass should run relative
        * to frame updates and state changes.
        *
        * Policies include:
        * - PER_FRAME:
        *   The pass executes every frame while active.
        *
        * - ONCE:
        *   The pass executes a single time and then automatically transitions
        *   to a sleeping state.
        *
        * - ON_DIRTY:
        *   The pass executes only when marked dirty and then returns to a
        *   sleeping state.
        *
        * The execution policy is evaluated only if the pass status is ACTIVE.
        */
        FramePassExecutionPolicy m_ExecutePolicy{ FramePassExecutionPolicy::PER_FRAME };
    };

}// namespace Mikoto


#endif//MIKOTO_FRAME_PASS_HH
