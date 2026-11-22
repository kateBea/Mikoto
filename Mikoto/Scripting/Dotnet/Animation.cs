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

using System.Runtime.InteropServices;

namespace Mikoto;

// Mirrors ManagedAnimatorOperation and ManagedAnimatorRequest in DotnetInterop.hh.
internal enum AnimatorOperation : uint
{
    FindJoint,
    JointPosition,
    CrossFade,
    TwoBoneIK,
    AimIK
}

[StructLayout(LayoutKind.Sequential)]
internal struct AnimatorRequest
{
    public uint Rig;
    public int Index;
    public int Start;
    public int Middle;
    public int End;
    public Vector3 Target;
    public Vector3 Pole;
    public Vector3 Axis;
    public Vector3 Forward;
    public Vector3 Up;
    public float Weight;
    public float Duration;
}

/// <summary>Controls the existing native animator and its Ozz IK constraints.</summary>
/// <remarks>Targets and pole directions use world space; joint axes use joint-local space.</remarks>
public sealed class AnimatorComponent : Component
{
    internal AnimatorComponent(Entity entity) : base(entity) { }

    /// <summary>Starts a crossfade from the current pose. Call when the desired clip changes.</summary>
    public void CrossFade(string clip, float seconds = 0.2f)
    {
        AnimatorRequest request = new() { Duration = seconds };
        Entity.AnimatorCommand(AnimatorOperation.CrossFade, clip, ref request);
    }

    /// <summary>Finds a named joint in the imported skeleton, or returns -1.</summary>
    public int FindJoint(string name, uint rig = 0)
    {
        AnimatorRequest request = new() { Rig = rig, End = -1 };
        Entity.AnimatorCommand(AnimatorOperation.FindJoint, name, ref request);
        return request.End;
    }

    /// <summary>Returns the sampled/blended world position before IK, avoiding target feedback.</summary>
    public Vector3 GetAnimatedJointPosition(int joint, uint rig = 0)
    {
        AnimatorRequest request = new() { Rig = rig, End = joint };
        Entity.AnimatorCommand(AnimatorOperation.JointPosition, string.Empty, ref request);
        return request.Target;
    }

    /// <summary>Updates a persistent two-bone constraint slot. Zero weight disables it.</summary>
    /// <param name="middleAxis">Knee/elbow hinge in joint-local space. Zero derives it from the sampled bend.</param>
    public void SetTwoBoneIK(int slot, int start, int middle, int end, Vector3 target,
        Vector3 poleDirection, float weight = 1.0f, Vector3 middleAxis = default, uint rig = 0)
    {
        AnimatorRequest request = new()
        {
            Rig = rig, Index = slot, Start = start, Middle = middle, End = end,
            Target = target, Pole = poleDirection, Axis = middleAxis, Weight = weight
        };
        Entity.AnimatorCommand(AnimatorOperation.TwoBoneIK, string.Empty, ref request);
    }

    /// <summary>Updates a persistent aim constraint slot. Forward/up are joint-local axes.</summary>
    public void SetAimIK(int slot, int joint, Vector3 target, Vector3 poleDirection,
        Vector3 forward, Vector3 up, float weight = 1.0f, uint rig = 0)
    {
        AnimatorRequest request = new()
        {
            Rig = rig, Index = slot, End = joint, Target = target,
            Pole = poleDirection, Forward = forward, Up = up, Weight = weight
        };
        Entity.AnimatorCommand(AnimatorOperation.AimIK, string.Empty, ref request);
    }
}
