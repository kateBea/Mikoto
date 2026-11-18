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

#ifndef MIKOTO_RENDERER_CORE_SLANG_COMPILER_HH
#define MIKOTO_RENDERER_CORE_SLANG_COMPILER_HH

#include <slang.h>
#include <slang-com-ptr.h>

#include <EASTL/vector.h>
#include <EASTL/unique_ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

namespace mikoto::renderer {

    class RenderSystem;

    struct SlangCompilerContext {
        // Configuration storage outlives the sessions, and the global session
        // is destroyed after all compiler objects derived from it.
        std::array<slang::TargetDesc, 1> mTargets{};
        std::array<slang::TargetDesc, 1> mVertexTargets{};
        std::array<slang::CompilerOptionEntry, 4> mOptions{};
        std::array<slang::CompilerOptionEntry, 4> mRayOptions{};
        std::array<slang::PreprocessorMacroDesc, 2> mMacros{};
        Slang::ComPtr<slang::IGlobalSession> mGlobal{};
        Slang::ComPtr<slang::ISession> mSession{};
        Slang::ComPtr<slang::ISession> mVertexSession{};
        Slang::ComPtr<slang::ISession> mRaySession{};
        std::array<eastl::string, 3> mCacheKeys{};
        bool mInUse{};
    };


    /** Temporarily borrows one compiler context from RenderSystem's pool.
     * Other threads cannot borrow that context while this lease holds it.
     * Destruction automatically returns it, including when compilation throws.
     * The pool owns the sessions. Release Slang-derived objects before the lease
     * ends; retain only copied output.
     */
    class SlangSessionLease final : public core::INonCopyable {
    public:
        SlangSessionLease( SlangSessionLease &&other ) noexcept;
        auto operator=( SlangSessionLease &&other ) noexcept -> SlangSessionLease &;

        MKT_NODISCARD auto GetCacheKey( rhi::ShaderType stage ) const -> eastl::string_view;
        MKT_NODISCARD auto GetSession( rhi::ShaderType stage = rhi::ShaderType::eInvalid ) const -> slang::ISession *;

        ~SlangSessionLease();

    private:
        friend class RenderSystem;

    private:
        SlangSessionLease( RenderSystem& owner, SlangCompilerContext &context );

    private:
        RenderSystem *mOwner{};
        SlangCompilerContext *mContext{};

        core::i32 mUncaughtExceptions{};
    };

}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_CORE_SLANG_COMPILER_HH