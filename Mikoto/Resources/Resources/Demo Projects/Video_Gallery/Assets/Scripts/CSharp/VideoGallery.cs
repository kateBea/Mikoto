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

using Mikoto;

/// <summary>Camera-relative locomotion, clip crossfades, and ground-aware foot targets.</summary>
public sealed class VideoGalleryPlayer : EntityScript
{
    private const float DegreesToRadians = MathF.PI / 180.0f;
    private CharacterControllerComponent _character = null!;
    private AnimatorComponent _animator = null!;
    private TransformComponent _camera = null!;
    private VideoGalleryFeet _feet = null!;
    private Vector3 _spawn;
    private Vector3 _focus;
    private Vector2 _previousMouse;
    private bool _trackingMouse;
    private bool _previousToggle;
    private bool _previousJump;
    private bool _jumping;
    private bool _leftGround;
    private float _jumpElapsed;
    private bool _footIkEnabled = true;
    private float _yaw;
    private float _pitch = -22.0f;
    private float _distance = 4.2f;
    private float _speed;
    private string _clip = string.Empty;

    public override void OnCreate()
    {
        _character = Entity.GetComponent<CharacterControllerComponent>();
        Entity visual = Entity.Find("Player visual") ?? throw new InvalidOperationException("Player visual is missing.");
        _animator = visual.GetComponent<AnimatorComponent>();
        _camera = (Entity.Find("Orbit camera") ?? throw new InvalidOperationException("Orbit camera is missing.")).GetComponent<TransformComponent>();
        _feet = new VideoGalleryFeet(Entity, visual, _animator);
        _spawn = Position;
        _focus = Position + new Vector3(0, 1.15f, 0);
        SetClip("Idle");
    }

    public override void OnUpdate(float timeStep)
    {
        float dt = Math.Clamp(timeStep, 0.0f, 0.05f);
        bool focused = Input.IsFocused;
        UpdateOrbit(focused);

        float x = focused ? Axis(KeyCode.A, KeyCode.D) : 0;
        float z = focused ? Axis(KeyCode.S, KeyCode.W) : 0;
        Vector3 forward = new(-MathF.Sin(_yaw * DegreesToRadians), 0, -MathF.Cos(_yaw * DegreesToRadians));
        Vector3 right = new(MathF.Cos(_yaw * DegreesToRadians), 0, -MathF.Sin(_yaw * DegreesToRadians));
        Vector3 direction = right * x + forward * z;
        bool moving = direction.Length() > 0.01f;
        float targetSpeed = moving ? (Input.IsKeyPressed(KeyCode.LeftShift) ? 4.3f : 1.65f) : 0;
        _speed += (targetSpeed - _speed) * (1.0f - MathF.Exp(-12.0f * dt));

        if (moving)
        {
            direction = direction.Normalized();
            float targetYaw = MathF.Atan2(direction.X, direction.Z) / DegreesToRadians;
            Vector3 rotation = Rotation;
            rotation.Y += MathF.IEEERemainder(targetYaw - rotation.Y, 360.0f) * (1.0f - MathF.Exp(-14.0f * dt));
            Rotation = rotation;
        }

        bool grounded = _character.IsGrounded;
        bool jump = focused && Input.IsKeyPressed(KeyCode.Space);
        bool beginJump = jump && !_previousJump && grounded && !_jumping;
        _previousJump = jump;

        if (beginJump)
        {
            _jumping = true;
            _leftGround = false;
            _jumpElapsed = 0;
        }

        _character.Move(moving ? direction * _speed : new Vector3(0), beginJump ? 4.3f : 0);

        if (_jumping)
        {
            _jumpElapsed += dt;
            _leftGround |= !grounded;

            // Grounded still describes the previous physics step on takeoff.
            // Wait for flight before accepting a landing, then keep the authored
            // one-second jump from looping on a long fall.
            bool landed = _leftGround && grounded;
            bool takeoffBlocked = !_leftGround && _jumpElapsed > 0.25f;
            if (landed || takeoffBlocked)
                _jumping = false;
        }

        string locomotionClip = !moving ? "Idle" : Input.IsKeyPressed(KeyCode.LeftShift) ? "Running" : "Walking";
        SetClip(_jumping && _jumpElapsed < 0.98f ? "Jump" : locomotionClip);

        bool toggle = focused && Input.IsKeyPressed(KeyCode.I);
        if (toggle && !_previousToggle)
            _footIkEnabled = !_footIkEnabled;
        _previousToggle = toggle;

        if ((focused && Input.IsKeyPressed(KeyCode.R)) || Position.Y < -5.0f)
        {
            Position = _spawn;
            _jumping = false;
            _leftGround = false;
            _speed = 0;
        }

        _feet.Update(dt, _footIkEnabled && grounded && !_jumping);
        _focus += (Position + new Vector3(0, 1.15f, 0) - _focus) * (1.0f - MathF.Exp(-16.0f * dt));
        float yaw = _yaw * DegreesToRadians;
        float pitch = _pitch * DegreesToRadians;
        Vector3 offset = new Vector3(MathF.Sin(yaw) * MathF.Cos(pitch), -MathF.Sin(pitch), MathF.Cos(yaw) * MathF.Cos(pitch)) * _distance;

        // Keep the orbit out of the house walls and stair treads, without changing the look target.
        if (Entity.RaycastStatic(_focus, offset, out Vector3 hit, out _))
        {
            float safeDistance = MathF.Max(0.25f, (hit - _focus).Length() - 0.15f);
            offset = offset.Normalized() * safeDistance;
        }

        _camera.Position = _focus + offset;
        _camera.Rotation = new Vector3(_pitch, _yaw, 0);
    }

    private void UpdateOrbit(bool focused)
    {
        bool active = focused && Input.IsPointerOver;
        bool rotating = active && Input.IsMouseButtonPressed(MouseButton.Right);
        Vector2 mouse = Input.MousePosition;
        if (rotating && _trackingMouse)
        {
            Vector2 delta = mouse - _previousMouse;
            _yaw = MathF.IEEERemainder(_yaw - delta.X * 0.16f, 360.0f);
            _pitch = Math.Clamp(_pitch - delta.Y * 0.12f, -65.0f, -6.0f);
        }
        _trackingMouse = rotating;
        _previousMouse = mouse;
        if (active)
            _distance = Math.Clamp(_distance - Input.MouseScrollDelta.Y * 0.5f, 1.4f, 12.0f);
    }

    private void SetClip(string clip)
    {
        if (_clip == clip)
            return;
        _animator.CrossFade(clip, 0.22f);
        _clip = clip;
    }

    private static float Axis(KeyCode negative, KeyCode positive) =>
        (Input.IsKeyPressed(positive) ? 1 : 0) - (Input.IsKeyPressed(negative) ? 1 : 0);
}

/// <summary>Supplies targets to the native two-bone solver; it does not implement a second IK solver.</summary>
internal sealed class VideoGalleryFeet
{
    private const float PlantedToeOutDegrees = 6.0f;
    private const float KneeOutDegrees = 12.0f;
    private readonly Entity _entity;
    private readonly AnimatorComponent _animator;
    private readonly TransformComponent _actor;
    private readonly TransformComponent _visual;
    private readonly Vector3 _visualOrigin;
    private readonly int[] _hips = new int[2];
    private readonly int[] _knees = new int[2];
    private readonly int[] _ankles = new int[2];
    private readonly int[] _toes = new int[2];
    private readonly float[] _weights = new float[2];
    private readonly float[] _contacts = new float[2];
    private readonly float[] _terrainOffsets = new float[2];
    private readonly Vector3[] _feet = new Vector3[2];
    private readonly Vector3[] _normals = new Vector3[2];
    private readonly float[] _idleHeights = [0.05574059f, 0.05318388f];
    private float _pelvisOffset;
    private float _clearanceOffset;

    internal VideoGalleryFeet(Entity entity, Entity visual, AnimatorComponent animator)
    {
        _entity = entity;
        _animator = animator;
        _actor = entity.GetComponent<TransformComponent>();
        _visual = visual.GetComponent<TransformComponent>();
        Vector3 origin = _visual.Position;
        _visualOrigin = new Vector3(origin.X, 0, origin.Z);
        _visual.Position = _visualOrigin;
        string[] sides = ["Left", "Right"];
        string[] hipSuffix = ["_058", "_062"];
        string[] kneeSuffix = ["_059", "_063"];
        string[] footSuffix = ["_00", "_064"];
        string[] toeSuffix = ["_060", "_065"];

        for (int i = 0; i < 2; ++i)
        {
            _hips[i] = animator.FindJoint("mixamorig:" + sides[i] + "UpLeg" + hipSuffix[i]);
            _knees[i] = animator.FindJoint("mixamorig:" + sides[i] + "Leg" + kneeSuffix[i]);
            _ankles[i] = animator.FindJoint("mixamorig:" + sides[i] + "Foot" + footSuffix[i]);
            _toes[i] = animator.FindJoint("mixamorig:" + sides[i] + "ToeBase" + toeSuffix[i]);
            if (_hips[i] < 0 || _knees[i] < 0 || _ankles[i] < 0 || _toes[i] < 0)
                throw new InvalidOperationException("Xbot foot chain was not found in the imported skeleton.");

            // Clear constraints left by a previous script instance, including the
            // old toe-flattening slots. A reload must start from the sampled pose.
            Vector3 up = new(0, 1, 0);
            animator.SetTwoBoneIK(i, _hips[i], _knees[i], _ankles[i], new Vector3(0), up, 0);
            animator.SetAimIK(i, _ankles[i], new Vector3(0), up, new Vector3(0, 0, 1), up, 0);
            animator.SetAimIK(i + 2, _toes[i], new Vector3(0), up, new Vector3(0, 0, 1), up, 0);
        }
    }

    internal void Update(float dt, bool enabled)
    {
        Vector3 up = new(0, 1, 0);
        float yaw = _actor.Rotation.Y * MathF.PI / 180.0f;
        Vector3 heading = new(MathF.Sin(yaw), 0, MathF.Cos(yaw));
        Vector3 actorPosition = _actor.Position;
        float support = actorPosition.Y;

        // Compare foot support to the actor's support, not a fixed ankle height.
        // Character collision padding must not activate IK on an otherwise flat floor.
        if (enabled && _entity.RaycastStatic(actorPosition + up * 0.15f, up * -0.4f, out Vector3 actorHit, out Vector3 actorNormal) &&
            actorNormal.Y > 0.6f && MathF.Abs(actorHit.Y - actorPosition.Y) < 0.08f)
            support = actorHit.Y;

        float desiredClearance = enabled ? Math.Clamp(support - actorPosition.Y, -0.08f, 0.08f) : 0;
        float amount = 0;
        float desiredPelvis = 0;

        for (int i = 0; i < 2; ++i)
        {
            Vector3 foot = _animator.GetAnimatedJointPosition(_ankles[i]);
            Vector3 toe = _animator.GetAnimatedJointPosition(_toes[i]);
            Vector3 toeDirection = toe - foot;
            Vector3 forward = new(toeDirection.X, 0, toeDirection.Z);
            forward = forward.Length() > 0.001f ? forward.Normalized() : heading;
            foot -= up * (_pelvisOffset + _clearanceOffset);
            _feet[i] = foot;
            _normals[i] = up;
            _contacts[i] = 0;
            _terrainOffsets[i] = 0;

            if (enabled && ProbeSupport(foot, forward, out float floor, out Vector3 normal))
            {
                float offset = floor - support;
                if (offset >= -0.45f && offset <= 0.35f)
                {
                    float lift = foot.Y - actorPosition.Y - _idleHeights[i];
                    float swing = Math.Clamp((lift - 0.025f) / 0.125f, 0, 1);
                    float contact = 1.0f - swing * swing * (3.0f - 2.0f * swing);
                    _contacts[i] = contact;
                    _terrainOffsets[i] = offset;
                    _normals[i] = normal;
                    amount = MathF.Max(amount, MathF.Abs(offset) * contact);
                    amount = MathF.Max(amount, (1.0f - normal.Y) * contact);
                    desiredPelvis = MathF.Min(desiredPelvis, MathF.Min(0, offset) * contact);
                }
            }
        }

        desiredPelvis = Math.Clamp(desiredPelvis, -0.22f, 0);
        _pelvisOffset += (desiredPelvis - _pelvisOffset) * (1.0f - MathF.Exp(-14.0f * dt));
        if (MathF.Abs(_pelvisOffset) < 0.0001f)
            _pelvisOffset = 0;
        // Account for capsule padding with one visual translation, without solving
        // or flattening any joints on level ground.
        _clearanceOffset += (desiredClearance - _clearanceOffset) * (1.0f - MathF.Exp(-20.0f * dt));
        _visual.Position = _visualOrigin + up * (_pelvisOffset + _clearanceOffset);

        // Flat terrain contributes exactly zero. Idle/walk/run retain the original
        // animation instead of being re-solved against an assumed flat-foot pose.
        float influence = enabled ? Math.Clamp((MathF.Max(amount, MathF.Abs(_pelvisOffset)) - 0.004f) / 0.025f, 0, 1) : 0;
        float blend = 1.0f - MathF.Exp(-18.0f * dt);
        Vector3 soleForward = new(0, 0, 1);
        Vector3 actorLeft = Vector3.Cross(up, heading);
        float kneeAngle = KneeOutDegrees * (MathF.PI / 180.0f);

        for (int i = 0; i < 2; ++i)
        {
            _weights[i] += (_contacts[i] * influence - _weights[i]) * blend;
            if (_weights[i] < 0.0001f)
                _weights[i] = 0;
            float weight = _weights[i];
            Vector3 normal = _normals[i];
            Vector3 target = _feet[i] + up * (_terrainOffsets[i] + _clearanceOffset);
            Vector3 posedFoot = _feet[i] + up * (_pelvisOffset + _clearanceOffset);
            Vector3 solvedFoot = posedFoot + (target - posedFoot) * weight;
            // Mirror the bend direction about the character's heading. Both
            // Xbot knees still hinge about local -X as the ankle reaches the tread.
            float side = i == 0 ? 1.0f : -1.0f;
            Vector3 kneePole = heading * MathF.Cos(kneeAngle) + actorLeft * (side * MathF.Sin(kneeAngle));
            _animator.SetTwoBoneIK(i, _hips[i], _knees[i], _ankles[i], target, kneePole, weight,
                middleAxis: new Vector3(-1, 0, 0));

            // Turn around the tread normal so the sole stays flush even on a slope.
            // Xbot faces +Z and its left leg is on the model's +X side.
            Vector3 tangent = (heading - normal * Vector3.Dot(heading, normal)).Normalized();
            Vector3 left = Vector3.Cross(normal, tangent);
            float angle = side * PlantedToeOutDegrees * (MathF.PI / 180.0f);
            Vector3 footHeading = tangent * MathF.Cos(angle) + left * MathF.Sin(angle);
            // Aim IK takes a point. A distant target keeps small differences
            // between the requested and reachable ankle height from pitching the foot.
            _animator.SetAimIK(i, _ankles[i], solvedFoot + footHeading * 10.0f, normal, soleForward, up, weight);
        }
    }

    private bool ProbeSupport(Vector3 ankle, Vector3 forward, out float height, out Vector3 normal)
    {
        bool found = false;
        height = float.NegativeInfinity;
        normal = new Vector3(0, 1, 0);

        // Use the ankle and heel as support. A toe reaching past a riser must
        // not lift the entire foot to the next tread before it steps onto it.
        for (int probe = 0; probe < 2; ++probe)
        {
            Vector3 origin = ankle - forward * (probe == 0 ? 0 : 0.055f) + new Vector3(0, 0.45f, 0);
            if (!_entity.RaycastStatic(origin, new Vector3(0, -1.1f, 0), out Vector3 hit, out Vector3 surfaceNormal) || surfaceNormal.Y < 0.6f)
                continue;
            float support = hit.Y - (surfaceNormal.X * (ankle.X - hit.X) + surfaceNormal.Z * (ankle.Z - hit.Z)) / surfaceNormal.Y;
            if (support > height)
            {
                height = support;
                normal = surfaceNormal;
                found = true;
            }
        }

        return found;
    }
}

