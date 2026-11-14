// Copyright 2026 ケイト
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MIKOTO_APPLICATION_WORKSPACE_OPERATION_HH
#define MIKOTO_APPLICATION_WORKSPACE_OPERATION_HH

#include <future>

#include <EASTL/chrono.h>
#include <EASTL/functional.h>
#include <EASTL/string.h>
#include <EASTL/vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

namespace mikoto::editor {
    /**
     * @brief Sequences editor workspace loading and saving operations.
     *
     * Owns the ordered work steps and progress state. EditorLayer owns the UI:
     * while IsVisible() is true, it disables workspace interaction, skips normal
     * scene updates and draws the progress popup. This class does not draw ImGui
     * widgets or pause the scene itself.
     *
     * The lifecycle is:
     * 1. Start() stores the steps and makes the operation visible and busy.
     * 2. The UI displays GetStage(), then calls MarkPresented().
     * 3. Update() executes that step or polls its background worker.
     * 4. Each completed step requires its successor's label to be presented
     *    before the successor can execute.
     * 5. Success leaves the completed popup visible until Dismiss(). Failure
     *    leaves the error visible for acknowledgement and skips remaining work.
     * 6. Dismiss() resets the operation, then invokes its success callback.
     *
     * All methods must be called on the editor's main thread. Engine-facing
     * steps also execute there. Background steps may only capture owned,
     * immutable file-I/O snapshots; they must not access live editor or scene
     * state. Steps execute sequentially, never concurrently with one another.
     *
     * For example, saving a scene serializes live entities on the main thread,
     * writes the resulting text snapshot on a worker, then updates project and
     * editor settings on the main thread. A success callback can defer an action
     * such as opening another scene until the save popup has been dismissed.
     *
     * @note This is not a fully asynchronous loader: a long main-thread step
     * still blocks UI updates. Progress labels identify stages, not a measured
     * completion percentage. There is no cancellation or automatic rollback of
     * changes made by completed or partially executed steps.
     */
    class WorkspaceOperation final {
    public:
        /**
         * @brief One labelled unit of work in an operation.
         *
         * The callable must remain valid until its execution finishes. Exceptions
         * are collected by Update() and reported through GetError().
         */
        struct Step {
            /** @brief Status text shown before this step begins. */
            eastl::string mLabel{};

            /** @brief Work to execute once, after the UI presents its label. */
            eastl::function<void()> mExecute{};

            /**
             * @brief Executes the callable through std::async when true.
             *
             * Defaults to main-thread execution. Background callables must own
             * their input snapshots and must not capture live editor state.
             */
            bool mBackground{};
        };

        /**
         * @brief Queues an operation without executing any of its steps yet.
         *
         * @param title User-facing operation name, such as "Saving scene".
         * @param path File path or descriptive target shown below the title.
         * @param steps Nonempty sequence of labelled callables, in execution order.
         * @param onSuccess Optional main-thread callback invoked by Dismiss()
         * after successful completion and reset, not by the final step itself.
         * @returns True if accepted. Returns false without changing the current
         * operation when one is still visible or @p steps is empty.
         */
        MKT_NODISCARD auto Start( eastl::string title, eastl::string path, eastl::vector<Step> steps,
                                 eastl::function<void()> onSuccess = {} ) -> bool;

        /**
         * @brief Advances the current operation from the editor's frame loop.
         *
         * Waits for MarkPresented() before executing a new step. Main-thread
         * work runs inline; background work is launched once and polled without
         * blocking on subsequent calls. At most one step completes per call.
         * Exceptions stop the sequence and discard the success callback.
         */
        auto Update() -> void;

        /**
         * @brief Allows the current step to execute after its status is displayed.
         *
         * Called by the progress UI once the popup is past its initial sizing
         * frame. This is a UI sequencing flag, not a GPU presentation fence.
         * Completing a step clears the flag for the next step.
         */
        auto MarkPresented() -> void;

        /**
         * @brief Hides a finished operation and invokes its success callback.
         *
         * Does nothing while busy. Otherwise resets state before invoking the
         * callback, allowing follow-up work to start a new operation. Failed
         * operations have no success callback. Callback exceptions propagate to
         * the caller; they are not handled by Update().
         */
        auto Dismiss() -> void;

        /**
         * @brief Waits for an active worker and discards the operation.
         *
         * Used during shutdown to finish an outstanding file write before
         * releasing its state. Does not interrupt the worker, execute remaining
         * steps, invoke the success callback or roll back earlier changes.
         * Title, path and error strings are retained until the next Start().
         */
        auto Reset() -> void;

        /**
         * @brief Reports whether the UI may automatically dismiss a success.
         * @returns True when visible, finished without error and at least
         * 800 milliseconds have elapsed since the last step completed.
         */
        MKT_NODISCARD auto CanAutoDismiss() const -> bool;

        /**
         * @brief Reports whether the progress popup should remain displayed.
         * @returns True during work and while awaiting dismissal after completion
         * or failure. Visibility does not imply that work is still running.
         */
        MKT_NODISCARD auto IsVisible() const -> bool;

        /**
         * @brief Reports whether the operation still has unfinished work.
         * @returns True while waiting for presentation, executing or awaiting a worker.
         */
        MKT_NODISCARD auto IsBusy() const -> bool;

        /**
         * @brief Reports whether a nonempty error message is stored.
         * @returns True if GetError() is nonempty. Start() clears the previous error.
         */
        MKT_NODISCARD auto HasFailed() const -> bool;

        /**
         * @brief Retrieves the title supplied to Start().
         * @returns A reference to the stored operation title.
         */
        MKT_NODISCARD auto GetTitle() const -> const eastl::string&;

        /**
         * @brief Retrieves the target description supplied to Start().
         * @returns A reference to the stored path text; this accessor performs no I/O.
         */
        MKT_NODISCARD auto GetPath() const -> const eastl::string&;

        /**
         * @brief Retrieves the diagnostic captured from a failed step.
         * @returns A reference to the stored error, empty when no error is recorded.
         */
        MKT_NODISCARD auto GetError() const -> const eastl::string&;

        /**
         * @brief Retrieves the current step's label while busy.
         * @returns The step label while busy, otherwise the stored error text.
         * The UI supplies its own "Completed" label after successful execution.
         */
        MKT_NODISCARD auto GetStage() const -> const eastl::string&;

    private:
        /**
         * @brief Advances to the next step and requires its label to be presented.
         * Finishing the last step clears busy state and starts the dismissal timer.
         */
        auto CompleteStep() -> void;

        /**
         * @brief Stops execution while leaving the error popup visible.
         * @param message Diagnostic to retain. Remaining steps and the success
         * callback are discarded; previously performed work is not undone.
         */
        auto Fail( eastl::string message ) -> void;

        eastl::string mTitle{};
        eastl::string mPath{};
        eastl::string mError{};
        eastl::vector<Step> mSteps{};
        eastl::function<void()> mOnSuccess{};
        // EASTL has no future/async equivalent; retain the standard worker handle.
        std::future<void> mWorker{};
        eastl::chrono::steady_clock::time_point mFinishedAt{};
        core::usize mStep{};
        bool mVisible{};
        bool mBusy{};
        bool mPresented{};
    };
}

#endif// MIKOTO_APPLICATION_WORKSPACE_OPERATION_HH

