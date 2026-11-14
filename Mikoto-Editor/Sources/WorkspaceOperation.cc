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

#include <chrono>
#include <exception>

#include <EASTL/utility.h>

#include <Core/LocalizationService.hh>

#include <ImGui/Utility.hh>

#include <Application/WorkspaceOperation.hh>

namespace mikoto::editor {

    auto WorkspaceOperation::Start( eastl::string title,
        eastl::string path,
        eastl::vector<Step> steps,
        eastl::function<void()> onSuccess ) -> bool {
        if ( mVisible || steps.empty() ) {
            return false;
        }

        mStep = 0;
        mError.clear();
        mTitle = eastl::move( title );
        mPath = eastl::move( path );
        mSteps = eastl::move( steps );
        mOnSuccess = eastl::move( onSuccess );
        mVisible = mBusy = true;
        mPresented = false;

        return true;
    }

    auto WorkspaceOperation::Update() -> void {
        if ( !mBusy ) {
            return;
        }

        try {
            if ( mWorker.valid() ) {
                if ( mWorker.wait_for( std::chrono::seconds{ 0 } ) == std::future_status::ready ) {
                    mWorker.get();
                    CompleteStep();
                }

                return;
            }

            if ( !mPresented ) {
                return;
            }

            auto& step{ mSteps[mStep] };

            if ( step.mBackground ) {
                mWorker = std::async( std::launch::async, eastl::move( step.mExecute ) );
            }
            else {
                step.mExecute();
                CompleteStep();
            }
        } catch ( const std::exception& error ) {
            Fail( error.what() );
        } catch ( ... ) {
            Fail( MKT_LOC( "workspace_operation_the_operation_failed_with_an_unknown_error" ).c_str() );
        }
    }

    auto WorkspaceOperation::CompleteStep() -> void {
        ++mStep;
        mPresented = false;

        if ( mStep == mSteps.size() ) {
            mBusy = false;
            mFinishedAt = eastl::chrono::steady_clock::now();
            mSteps.clear();
        }
    }

    auto WorkspaceOperation::Fail( eastl::string message ) -> void {
        mBusy = false;
        mSteps.clear();
        mOnSuccess = {};
        mError = eastl::move( message );
    }

    auto WorkspaceOperation::MarkPresented() -> void {
        mPresented = true;
    }

    auto WorkspaceOperation::IsVisible() const -> bool {
        return mVisible;
    }

    auto WorkspaceOperation::IsBusy() const -> bool {
        return mBusy;
    }

    auto WorkspaceOperation::HasFailed() const -> bool {
        return !mError.empty();
    }

    auto WorkspaceOperation::GetTitle() const -> const eastl::string& {
        return mTitle;
    }

    auto WorkspaceOperation::GetPath() const -> const eastl::string& {
        return mPath;
    }

    auto WorkspaceOperation::GetError() const -> const eastl::string& {
        return mError;
    }

    auto WorkspaceOperation::GetStage() const -> const eastl::string& {
        return mBusy ? mSteps[mStep].mLabel : mError;
    }

    auto WorkspaceOperation::CanAutoDismiss() const -> bool {
        return mVisible && !mBusy && !HasFailed() &&
               eastl::chrono::steady_clock::now() - mFinishedAt >= eastl::chrono::milliseconds{ 800 };
    }

    auto WorkspaceOperation::Dismiss() -> void {
        if ( mBusy ) {
            return;
        }

        auto completion{ eastl::move( mOnSuccess ) };

        Reset();

        if ( completion ) {
            completion();
        }
    }

    auto WorkspaceOperation::Reset() -> void {
        // No background job captures the editor. Wait for a pending atomic
        // write before shutdown, then drop unapplied main-thread steps.
        if ( mWorker.valid() ) {
            mWorker.wait();
            mWorker = {};
        }

        mSteps.clear();
        mOnSuccess = {};
        mVisible = mBusy = mPresented = false;
    }
}
