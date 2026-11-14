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

#ifndef MIKOTO_ANIMATION_ANIMATION_CONTROLLER_HH
#define MIKOTO_ANIMATION_ANIMATION_CONTROLLER_HH

#include <EASTL/string.h>
#include <EASTL/vector.h>

#include <nlohmann/json_fwd.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>

namespace mikoto::animation {

    using ControllerID = core::u32;

    enum class ParameterType {
        eFloat,
        eInteger,
        eBool,
        eTrigger
    };

    enum class ConditionOperator {
        eEqual,
        eNotEqual,
        eGreater,
        eLess,
        eGreaterEqual,
        eLessEqual
    };

    struct ControllerParameter {
        ControllerID mID{};
        eastl::string mName{};
        ParameterType mType{ ParameterType::eFloat };
        core::f32 mFloatValue{};
        core::i32 mIntValue{};
        bool mBoolValue{};
    };

    struct ControllerState {
        ControllerID mID{};
        eastl::string mName{};
        eastl::string mClip{};
        core::f32 mX{};
        core::f32 mY{};
    };

    struct TransitionCondition {
        ControllerID mParameter{};
        ConditionOperator mComparison{ ConditionOperator::eEqual };
        core::f64 mThreshold{};
    };

    struct ControllerTransition {
        ControllerID mID{};
        ControllerID mFrom{};
        ControllerID mTo{};
        bool mEnabled{ true };
        bool mHasExitTime{ true };

        // Normalized clip time; 1 means one complete loop.
        core::f32 mExitTime{ 1.0f };

        eastl::vector<TransitionCondition> mConditions{};

        // Seconds spent crossfading to the destination clip.
        core::f32 mBlendDuration{ 0.2f };
    };

    // A single-layer controller. Transition order is priority order.
    struct AnimationController {
        eastl::string mName{ "Animator Controller" };
        eastl::vector<ControllerState> mStates{};
        eastl::vector<ControllerParameter> mParameters{};
        eastl::vector<ControllerTransition> mTransitions{};
        ControllerID mDefaultState{ 0 };
        ControllerID mNextID{ 1 };

        auto AddParameter( ParameterType type ) -> ControllerParameter&;
        auto AddState( const eastl::string& label = "State" ) -> ControllerState&;

        auto Connect( ControllerID from, ControllerID to ) -> bool;
        auto RemoveState( ControllerID id ) -> void;
        auto RemoveParameter( ControllerID id ) -> void;

        MKT_NODISCARD auto FindState( ControllerID id ) -> ControllerState*;

        MKT_NODISCARD auto FindParameter( ControllerID id ) -> ControllerParameter*;

        MKT_NODISCARD auto FindState( ControllerID id ) const -> const ControllerState*;
        MKT_NODISCARD auto CanConnect( ControllerID from, ControllerID to ) const -> bool;
        MKT_NODISCARD auto FindParameter( ControllerID id ) const -> const ControllerParameter*;

        MKT_NODISCARD auto Validate() const -> eastl::string;

        /**
         * @brief Encodes this controller's states, parameters and transitions as JSON.
         * @returns Versioned controller document.
         */
        MKT_NODISCARD auto EncodeController() const -> nlohmann::json;

        /**
         * @brief Decodes and validates a document before replacing this controller.
         * @param data Versioned controller document to read.
         * @param[out] error Validation diagnostic when decoding fails; empty on success.
         * @returns True after successfully decoding and validating the document.
         * @details Leaves this controller unchanged on failure.
         */
        MKT_NODISCARD auto DecodeController( const nlohmann::json& data, eastl::string& error ) -> bool;
    };

    class ControllerPlayback {
    public:
        auto Start( const AnimationController& controller ) -> bool;
        auto Stop() -> void;

        MKT_NODISCARD auto IsPlaying() const -> bool;
        MKT_NODISCARD auto GetState() const -> ControllerID;
        MKT_NODISCARD auto GetLastTransition() const -> ControllerID;
        MKT_NODISCARD auto GetElapsed() const -> core::f64;

        // At most one transition per update, including cycles with unconditional edges.
        auto Advance( AnimationController& controller, core::f32 dt, core::f32 clipDuration ) -> bool;

    private:
       MKT_NODISCARD static auto Matches( const ControllerParameter& parameter, const TransitionCondition& condition ) -> bool;

    private:
        ControllerID mState{};
        ControllerID mLastTransition{};

        core::f64 mElapsed{};
        bool mPlaying{};
    };

}// namespace mikoto::animation

#endif// MIKOTO_ANIMATION_ANIMATION_CONTROLLER_HH
