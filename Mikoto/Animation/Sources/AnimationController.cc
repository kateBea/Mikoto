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

#include <EASTL/algorithm.h>

#include <ankerl/unordered_dense.h>

#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>

#include <nlohmann/json.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Animation/AnimationController.hh>

namespace mikoto::animation {

    auto AnimationController::EncodeController() const -> nlohmann::json {
        MKT_BEGIN_PROFILER();
        nlohmann::json data{
            { "version", 1 },
            { "name", mName.c_str() },
            { "next_id", mNextID },
            { "default_state", mDefaultState },
            { "states", nlohmann::json::array() },
            { "parameters", nlohmann::json::array() },
            { "transitions", nlohmann::json::array() }
        };

        for ( const auto& state: mStates ) {
            data["states"].push_back( { { "id", state.mID }, { "name", state.mName.c_str() }, { "clip", state.mClip.c_str() }, { "x", state.mX }, { "y", state.mY } } );
        }

        for ( const auto& parameter: mParameters ) {
            data["parameters"].push_back( { { "id", parameter.mID }, { "name", parameter.mName.c_str() }, { "type", static_cast<core::i32>( parameter.mType ) }, { "float", parameter.mFloatValue }, { "integer", parameter.mIntValue }, { "bool", parameter.mType == animation::ParameterType::eTrigger ? false : parameter.mBoolValue } } );
        }

        for ( const auto& transition: mTransitions ) {
            nlohmann::json conditions{ nlohmann::json::array() };

            for ( const auto& condition: transition.mConditions ) {
                conditions.push_back( { { "parameter", condition.mParameter }, { "operator", static_cast<core::i32>( condition.mComparison ) }, { "threshold", condition.mThreshold } } );
            }

            data["transitions"].push_back( { { "id", transition.mID }, { "from", transition.mFrom }, { "to", transition.mTo }, { "enabled", transition.mEnabled }, { "has_exit_time", transition.mHasExitTime }, { "exit_time", transition.mExitTime }, { "blend_duration", transition.mBlendDuration }, { "conditions", conditions } } );
        }

        return data;
    }

    // Parse into a candidate so invalid files cannot partially replace the open document.
    auto AnimationController::DecodeController( const nlohmann::json& data, eastl::string& error ) -> bool {
        MKT_BEGIN_PROFILER();

        try {
            if ( data.at( "version" ) != 1 ) {
                MKT_THROW_RUNTIME_ERROR( "Unsupported controller version." );
            }

            const auto id = []( const nlohmann::json& value ) -> animation::ControllerID {
                if ( !value.is_number_integer() || value.get<core::f64>() < 0 || value.get<core::f64>() >= 1000000 ) {
                    MKT_THROW_RUNTIME_ERROR( "Invalid controller object ID." );
                }

                return value.get<animation::ControllerID>();
            };
            const auto text = []( const nlohmann::json& value ) {
                auto result{ value.get<std::string>() };

                if ( result.size() > 512 || result.find( '\0' ) != std::string::npos ) {
                    MKT_THROW_RUNTIME_ERROR( "Invalid controller text." );
                }

                return eastl::string{ result.c_str(), result.size() };
            };
            const auto integer = []( const nlohmann::json& value, core::i32 low, core::i32 high ) {
                if ( !value.is_number_integer() || value.get<core::f64>() < low || value.get<core::f64>() > high ) {
                    MKT_THROW_RUNTIME_ERROR( "Invalid integer or enum value." );
                }

                return value.get<core::i32>();
            };

            if ( !data.at( "states" ).is_array() || data.at( "states" ).size() > 512 ||
                 !data.at( "parameters" ).is_array() || data.at( "parameters" ).size() > 512 ||
                 !data.at( "transitions" ).is_array() || data.at( "transitions" ).size() > 4096 ) {
                MKT_THROW_RUNTIME_ERROR( "Invalid controller arrays or document too large." );
            }

            AnimationController candidate{};
            candidate.mName = text( data.at( "name" ) );
            candidate.mDefaultState = id( data.at( "default_state" ) );
            candidate.mNextID = id( data.at( "next_id" ) );

            for ( const auto& state: data.at( "states" ) ) {
                candidate.mStates.push_back( { id( state.at( "id" ) ), text( state.at( "name" ) ), text( state.at( "clip" ) ), state.at( "x" ).get<core::f32>(), state.at( "y" ).get<core::f32>() } );
            }

            for ( const auto& parameter: data.at( "parameters" ) ) {
                candidate.mParameters.push_back( { id( parameter.at( "id" ) ), text( parameter.at( "name" ) ),
                    static_cast<animation::ParameterType>( integer( parameter.at( "type" ), 0, 3 ) ), parameter.at( "float" ).get<core::f32>(),
                    integer( parameter.at( "integer" ), ( std::numeric_limits<core::i32>::min )(), ( std::numeric_limits<core::i32>::max )() ), parameter.at( "bool" ).get<bool>() } );
            }

            for ( const auto& transition: data.at( "transitions" ) ) {
                auto& value{ candidate.mTransitions.emplace_back() };
                value.mID = id( transition.at( "id" ) );
                value.mFrom = id( transition.at( "from" ) );
                value.mTo = id( transition.at( "to" ) );
                value.mEnabled = transition.at( "enabled" ).get<bool>();
                value.mHasExitTime = transition.at( "has_exit_time" ).get<bool>();
                value.mExitTime = transition.at( "exit_time" ).get<core::f32>();
                value.mBlendDuration = transition.value( "blend_duration", 0.2f );

                if ( !transition.at( "conditions" ).is_array() || transition.at( "conditions" ).size() > 64 ) {
                    MKT_THROW_RUNTIME_ERROR( "Invalid transition conditions." );
                }

                for ( const auto& condition: transition.at( "conditions" ) ) {
                    value.mConditions.push_back( { id( condition.at( "parameter" ) ), static_cast<animation::ConditionOperator>( integer( condition.at( "operator" ), 0, 5 ) ), condition.at( "threshold" ).get<core::f64>() } );
                }
            }

            error = candidate.Validate();

            if ( !error.empty() ) {
                return false;
            }

            for ( auto& parameter: candidate.mParameters ) {
                if ( parameter.mType == animation::ParameterType::eTrigger ) {
                    parameter.mBoolValue = false;
                }
            }

            *this = eastl::move( candidate );
            return true;
        } catch ( const std::exception& exception ) {
            error = exception.what();
            return false;
        }
    }

    auto AnimationController::FindState( ControllerID id ) -> ControllerState* {
        MKT_BEGIN_PROFILER();

        for ( auto& state: mStates ) {
            if ( state.mID == id ) {
                return &state;
            }
        }

        return nullptr;
    }

    auto AnimationController::FindState( ControllerID id ) const -> const ControllerState* {
        MKT_BEGIN_PROFILER();

        for ( const auto& state: mStates ) {
            if ( state.mID == id ) {
                return &state;
            }
        }

        return nullptr;
    }

    auto AnimationController::FindParameter( ControllerID id ) -> ControllerParameter* {
        MKT_BEGIN_PROFILER();

        for ( auto& parameter: mParameters ) {
            if ( parameter.mID == id ) {
                return &parameter;
            }
        }

        return nullptr;
    }

    auto AnimationController::FindParameter( ControllerID id ) const -> const ControllerParameter* {
        MKT_BEGIN_PROFILER();

        for ( const auto& parameter: mParameters ) {
            if ( parameter.mID == id ) {
                return &parameter;
            }
        }

        return nullptr;
    }

    auto AnimationController::AddState( const eastl::string& label ) -> ControllerState& {
        MKT_BEGIN_PROFILER();
        eastl::string unique{ label };
        core::i32 suffix{ 2 };

        while ( eastl::any_of( mStates.begin(), mStates.end(),
            [&]( const auto& s ) {
            return s.mName == unique;
        } ) ) {
            unique = label + " " + string::Format( "{}", suffix++ );
        }

        auto& state{ mStates.emplace_back() };
        state.mID = mNextID++;
        state.mName = unique;
        state.mX = 300.0f + static_cast<core::f32>( ( mStates.size() - 1 ) % 3 ) * 280.0f;
        state.mY = static_cast<core::f32>( ( mStates.size() - 1 ) / 3 ) * 160.0f;

        if ( !mDefaultState ) {
            mDefaultState = state.mID;
        }

        return state;
    }

    auto AnimationController::AddParameter( ParameterType type ) -> ControllerParameter& {
        MKT_BEGIN_PROFILER();
        auto& parameter{ mParameters.emplace_back() };
        parameter.mID = mNextID++;
        parameter.mName = "Parameter " + string::Format( "{}", parameter.mID );
        parameter.mType = type;
        return parameter;
    }

    auto AnimationController::CanConnect( ControllerID from, ControllerID to ) const -> bool {
        MKT_BEGIN_PROFILER();
        return from != to && FindState( from ) && FindState( to ) &&
               eastl::none_of( mTransitions.begin(), mTransitions.end(), [&]( const auto& t ) {
            return t.mFrom == from && t.mTo == to;
        } );
    }

    auto AnimationController::Connect( ControllerID from, ControllerID to ) -> bool {
        MKT_BEGIN_PROFILER();

        if ( !CanConnect( from, to ) ) {
            return false;
        }

        mTransitions.push_back( { .mID = mNextID++, .mFrom = from, .mTo = to } );
        return true;
    }

    auto AnimationController::RemoveState( ControllerID id ) -> void {
        MKT_BEGIN_PROFILER();
        eastl::erase_if( mStates, [id]( const auto& state ) {
            return state.mID == id;
        } );
        eastl::erase_if( mTransitions, [id]( const auto& transition ) {
            return transition.mFrom == id || transition.mTo == id;
        } );

        if ( mDefaultState == id ) {
            mDefaultState = mStates.empty() ? 0 : mStates.front().mID;
        }
    }

    auto AnimationController::RemoveParameter( ControllerID id ) -> void {
        MKT_BEGIN_PROFILER();
        eastl::erase_if( mParameters, [id]( const auto& parameter ) {
            return parameter.mID == id;
        } );

        for ( auto& transition: mTransitions ) {
            // Removing a guard must never silently create an unconditional transition.
            const auto count{ eastl::erase_if( transition.mConditions, [id]( const auto& condition ) {
                return condition.mParameter == id;
            } ) };

            if ( count ) {
                transition.mEnabled = false;
            }
        }
    }

    auto AnimationController::Validate() const -> eastl::string {
        MKT_BEGIN_PROFILER();
        const auto validText = []( const eastl::string& value ) {
            return value.size() <= 512 && value.find( '\0' ) == eastl::string::npos;
        };

        if ( mName.empty() || !validText( mName ) ) {
            return "Controller name must contain 1 to 512 characters.";
        }

        if ( mStates.size() > 512 || mParameters.size() > 512 || mTransitions.size() > 4096 ) {
            return "Controller exceeds the supported document size.";
        }

        if ( !mStates.empty() && !FindState( mDefaultState ) ) {
            return "The default state does not exist.";
        }

        if ( mStates.empty() && mDefaultState ) {
            return "An empty controller cannot have a default state.";
        }

        ankerl::unordered_dense::set<ControllerID> ids{};
        ankerl::unordered_dense::set<eastl::string> names{};
        ControllerID maximum{};
        const auto validID = [&]( ControllerID id ) {
            maximum = eastl::max( maximum, id );
            return id > 0 && id < 1000000 && ids.insert( id ).second;
        };

        for ( const auto& state: mStates ) {
            if ( !validID( state.mID ) ) {
                return "State IDs must be unique and nonzero.";
            }

            if ( state.mName.empty() || !validText( state.mName ) || !validText( state.mClip ) || !names.insert( state.mName ).second ) {
                return "State names must be unique and nonempty; text is limited to 512 characters.";
            }
        }

        names.clear();

        for ( const auto& parameter: mParameters ) {
            if ( !validID( parameter.mID ) ) {
                return "Parameter IDs must be unique and nonzero.";
            }

            if ( parameter.mName.empty() || !validText( parameter.mName ) || !names.insert( parameter.mName ).second ) {
                return "Parameter names must be unique and nonempty; text is limited to 512 characters.";
            }

            if ( parameter.mType < ParameterType::eFloat || parameter.mType > ParameterType::eTrigger ) {
                return "Unknown parameter type.";
            }
        }

        ankerl::unordered_dense::set<core::u64> edges{};

        for ( const auto& transition: mTransitions ) {
            if ( !validID( transition.mID ) ) {
                return "Transition IDs must be unique and nonzero.";
            }

            if ( transition.mFrom == transition.mTo || !FindState( transition.mFrom ) || !FindState( transition.mTo ) ) {
                return "A transition must connect two different existing states.";
            }

            const auto edge{ ( static_cast<core::u64>( transition.mFrom ) << 32 ) | transition.mTo };

            if ( !edges.insert( edge ).second ) {
                return "Duplicate transitions are not supported.";
            }

            if ( transition.mExitTime < 0 ) {
                return "Exit time must be nonnegative.";
            }

            if ( transition.mBlendDuration < 0.0f || transition.mBlendDuration > 30.0f ) {
                return "Blend duration must be between 0 and 30 seconds.";
            }

            if ( transition.mConditions.size() > 64 ) {
                return "A transition supports up to 64 conditions.";
            }

            for ( const auto& condition: transition.mConditions ) {
                if ( !FindParameter( condition.mParameter ) ) {
                    return "A transition references a missing parameter.";
                }

                if ( condition.mComparison < ConditionOperator::eEqual || condition.mComparison > ConditionOperator::eLessEqual ) {
                    return "Unknown condition operator.";
                }
            }
        }

        if ( mNextID <= maximum || mNextID >= 1000000 ) {
            return "Invalid next object ID.";
        }

        return {};
    }

    auto ControllerPlayback::Start( const AnimationController& controller ) -> bool {
        MKT_BEGIN_PROFILER();

        Stop();

        if ( !controller.FindState( controller.mDefaultState ) ) {
            return false;
        }

        mState = controller.mDefaultState;
        mPlaying = true;

        return true;
    }

    auto ControllerPlayback::Stop() -> void {
        MKT_BEGIN_PROFILER();
        mPlaying = false;
        mState = 0;
        mElapsed = 0;
        mLastTransition = 0;
    }

    auto ControllerPlayback::IsPlaying() const -> bool {
        MKT_BEGIN_PROFILER();
        return mPlaying;
    }

    auto ControllerPlayback::GetState() const -> ControllerID {
        MKT_BEGIN_PROFILER();
        return mState;
    }

    auto ControllerPlayback::GetLastTransition() const -> ControllerID {
        MKT_BEGIN_PROFILER();
        return mLastTransition;
    }

    auto ControllerPlayback::GetElapsed() const -> core::f64 {
        MKT_BEGIN_PROFILER();
        return mElapsed;
    }

    auto ControllerPlayback::Advance( AnimationController& controller, core::f32 dt, core::f32 clipDuration ) -> bool {
        MKT_BEGIN_PROFILER();
        mLastTransition = 0;

        if ( !mPlaying ) {
            return false;
        }

        if ( !controller.FindState( mState ) ) {
            Stop();
            return false;
        }

        if ( dt > 0 ) {
            mElapsed += dt;
        }

        for ( const auto& transition: controller.mTransitions ) {
            if ( !transition.mEnabled || transition.mFrom != mState || !controller.FindState( transition.mTo ) ) {
                continue;
            }

            if ( transition.mHasExitTime && ( clipDuration <= 0 || mElapsed / clipDuration < transition.mExitTime ) ) {
                continue;
            }

            bool matches{ true };

            for ( const auto& condition: transition.mConditions ) {
                const auto* parameter{ controller.FindParameter( condition.mParameter ) };

                if ( !parameter || !Matches( *parameter, condition ) ) {
                    matches = false;
                    break;
                }
            }

            if ( !matches ) {
                continue;
            }

            // Consume triggers only after every guard has passed.
            for ( const auto& condition: transition.mConditions ) {
                auto* parameter{ controller.FindParameter( condition.mParameter ) };

                if ( parameter->mType == ParameterType::eTrigger ) {
                    parameter->mBoolValue = false;
                }
            }

            mState = transition.mTo;
            mElapsed = 0;
            mLastTransition = transition.mID;

            return true;
        }

        return false;
    }

    auto ControllerPlayback::Matches( const ControllerParameter& parameter, const TransitionCondition& condition ) -> bool {
        MKT_BEGIN_PROFILER();

        if ( parameter.mType == ParameterType::eTrigger ) {
            return parameter.mBoolValue;
        }

        const core::f64 value{ parameter.mType == ParameterType::eFloat ? parameter.mFloatValue : ( parameter.mType == ParameterType::eInteger ? static_cast<core::f64>( parameter.mIntValue ) : static_cast<core::f64>( parameter.mBoolValue ) ) };

        switch ( condition.mComparison ) {
            case ConditionOperator::eEqual:
                return value == condition.mThreshold;
            case ConditionOperator::eNotEqual:
                return value != condition.mThreshold;
            case ConditionOperator::eGreater:
                return value > condition.mThreshold;
            case ConditionOperator::eLess:
                return value < condition.mThreshold;
            case ConditionOperator::eGreaterEqual:
                return value >= condition.mThreshold;
            case ConditionOperator::eLessEqual:
                return value <= condition.mThreshold;
        }

        return false;
    }
}// namespace mikoto::animation
