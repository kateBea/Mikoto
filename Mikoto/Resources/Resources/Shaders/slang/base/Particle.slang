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

#pragma once
#include "Memory_Helpers.slang"

static const uint kInvalidParticleTextureIndex = 0xffffffffu;

struct ParticleRenderData {
    float4 mPositionSize;
    float4 mColor;
    float4 mRotationSoftness;
    uint4 mTexture;
};

// Matches EmitterParameters in ParticleSimulationModule.hh.
struct ParticleEmitterParameters {
    float4x4 mWorld;
    float4 mDirectionCone;
    float4 mAccelerationDrag;
    float4 mLifetimeSpeed;
    float4 mEmission;
    float4 mSizeRotationSoftness;
    float4 mStartColor;
    float4 mEndColor;
    uint mOffset, mCapacity, mSteps, mReset;
    uint mSeed, mBurst, mEmitting, mVisible;
    uint mLooping, mInitialize, mMaxParticles, mPreviousCapacity;
    uint mTextureIndex, mTextureOffset;
    float mFadeIn, mFadeOut;
};

struct ParticleGpuState {
    float4 mPositionAge;
    float4 mVelocityLifetime;
    float4 mRotationFade; // Rotation, fade-in fraction, fade-out fraction, reserved.
    float4 mAccelerationDrag;
    float4 mSizeRotationSoftness;
    float4 mStartColor;
    float4 mEndColor;
    uint4 mTexture;
};

struct ParticleGpuHeader {
    uint mRandom;
    float mElapsed;
    float mEmissionRemainder;
    uint mInitialized;
    uint mSeed;
};
