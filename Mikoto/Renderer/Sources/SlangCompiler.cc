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

#include <utility>
#include <array>
#include <thread>
#include <algorithm>
#include <exception>

#include <slang.h>
#include <slang-com-ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Logging/Logger.hh>

#include <Threading/TaskService.hh>

#include <Renderer/Core/RenderSystem.hh>

#include <ImGui/ImGuiService.hh>

#include <Renderer/Core/RenderSystem.hh>
#include <Renderer/Core/SlangCompiler.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;

    SlangSessionLease::SlangSessionLease( RenderSystem &owner, SlangCompilerContext &context )
        : mOwner{ &owner }, mContext{ &context }, mUncaughtExceptions{ std::uncaught_exceptions() } {
    }

    SlangSessionLease::SlangSessionLease( SlangSessionLease &&other ) noexcept
        : mOwner{ std::exchange( other.mOwner, nullptr ) }, mContext{ std::exchange( other.mContext, nullptr ) }, 
        mUncaughtExceptions{ other.mUncaughtExceptions } {
    }

    auto SlangSessionLease::operator=( SlangSessionLease &&other ) noexcept -> SlangSessionLease & {
        if ( this != &other ) {
            if ( mOwner ) {
                mOwner->ReleaseSlangSession( *mContext );
            }

            mOwner = eastl::exchange( other.mOwner, nullptr );
            mContext = eastl::exchange( other.mContext, nullptr );
            mUncaughtExceptions = other.mUncaughtExceptions;
        }

        return *this;
    }

    SlangSessionLease::~SlangSessionLease() {
        if ( mOwner ) {
            // Slang can cache a failed module. Retry with a fresh context after
            // the caller has released its temporary compiler objects.
            if ( std::uncaught_exceptions() > mUncaughtExceptions ) {
                mContext->mVertexSession = nullptr;
                mContext->mRaySession = nullptr;
                mContext->mSession = nullptr;
                mContext->mGlobal = nullptr;
            }

            mOwner->ReleaseSlangSession( *mContext );
        }
    }

    auto SlangSessionLease::GetSession( rhi::ShaderType stage ) const -> slang::ISession * {
        if ( !mContext ) {
            return nullptr;
        }
        if ( stage == rhi::ShaderType::eRayGeneration && mContext->mRaySession ) {
            return mContext->mRaySession.get();
        }

        return stage == rhi::ShaderType::eVertex && mContext->mVertexSession
                   ? mContext->mVertexSession.get()
                   : mContext->mSession.get();
    }

    auto SlangSessionLease::GetCacheKey( rhi::ShaderType stage ) const -> eastl::string_view {
        const auto index{ stage == rhi::ShaderType::eRayGeneration && mContext->mRaySession ? 2 : stage == rhi::ShaderType::eVertex && mContext->mVertexSession ? 1
                                                                                                                                                                : 0 };
        return mContext->mCacheKeys[index];
    }

}// namespace mikoto::renderer