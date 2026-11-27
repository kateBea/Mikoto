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

/// <summary>Follows authored pedestrian routes through the native character controller.</summary>
public sealed class VideoTownPedestrian : EntityScript
{
    [SerializeField] public int Route = 0;
    [SerializeField] public int StartWaypoint = 0;
    [SerializeField] public float WalkSpeed = 1.35f;
    [SerializeField] public string VisualName = string.Empty;

    private static readonly List<VideoTownPedestrian> ActivePedestrians = [];

    private CharacterControllerComponent _character = null!;
    private AnimatorComponent _animator = null!;
    private VideoGalleryFeet _feet = null!;
    private TownRoutePoint[] _route = [];
    private Vector3 _spawn;
    private Vector3 _cachedPosition;
    private Vector3 _previousPosition;
    private int _waypoint;
    private int _routeDirection = 1;
    private float _waitRemaining;
    private float _blockedTime;
    private bool _registered;
    private string _clip = string.Empty;

    public override void OnCreate()
    {
        _character = Entity.GetComponent<CharacterControllerComponent>();
        Entity visual = Entity.Find(VisualName) ?? throw new InvalidOperationException("Pedestrian visual is missing: " + VisualName);
        _animator = visual.GetComponent<AnimatorComponent>();
        _feet = new VideoGalleryFeet(Entity, visual, _animator);
        _route = VideoTownRoutes.Get(Route);
        _waypoint = Wrap(StartWaypoint + 1);
        _spawn = Position;
        _cachedPosition = _spawn;
        _previousPosition = _spawn;
        _waitRemaining = (EntityId % 7) * 0.17f;
        _registered = true;
        ActivePedestrians.Add(this);
        SetClip("Idle");
    }

    public override void OnUpdate(float timeStep)
    {
        float dt = Math.Clamp(timeStep, 0.0f, 0.05f);
        if (dt <= 0.0f)
            return;

        _cachedPosition = Position;
        if (_cachedPosition.Y < -5.0f)
        {
            Position = _spawn;
            _cachedPosition = _spawn;
            _waitRemaining = 0.8f;
            _blockedTime = 0;
        }

        if (_waitRemaining > 0.0f)
        {
            _waitRemaining -= dt;
            LookAtNearbyDisplay(dt);
            Rest(dt);
            _previousPosition = _cachedPosition;
            return;
        }

        TownRoutePoint target = _route[_waypoint];
        float x = target.X - _cachedPosition.X;
        float z = target.Z - _cachedPosition.Z;
        float distance = MathF.Sqrt(x * x + z * z);

        if (distance < 0.24f)
        {
            _waitRemaining = target.Wait;
            _waypoint = Wrap(_waypoint + _routeDirection);
            Rest(dt);
            _previousPosition = _cachedPosition;
            return;
        }

        x /= distance;
        z /= distance;
        float speed = Math.Clamp(WalkSpeed, 0.5f, 2.0f);
        float separationX = 0;
        float separationZ = 0;

        // Peers expose their last position, avoiding scene searches on every steering update.
        foreach (VideoTownPedestrian peer in ActivePedestrians)
        {
            if (peer == this || !peer._registered)
                continue;

            float dx = _cachedPosition.X - peer._cachedPosition.X;
            float dz = _cachedPosition.Z - peer._cachedPosition.Z;
            float squaredDistance = dx * dx + dz * dz;
            if (squaredDistance < 0.0001f || squaredDistance > 1.0f)
                continue;

            float weight = (1.0f - MathF.Sqrt(squaredDistance)) * 0.7f / squaredDistance;
            separationX += dx * weight;
            separationZ += dz * weight;

            // Give the leading actor priority instead of letting two capsules push indefinitely.
            float ahead = -(dx * x + dz * z);
            if (ahead > 0 && squaredDistance < 0.65f * 0.65f && EntityId > peer.EntityId)
                speed *= 0.15f;
        }

        x += separationX;
        z += separationZ;
        float steeringLength = MathF.Sqrt(x * x + z * z);
        if (steeringLength > 0.001f)
        {
            x /= steeringLength;
            z /= steeringLength;
        }

        Vector3 probe = _cachedPosition + new Vector3(0, 0.85f, 0);
        if (Blocked(probe, x, z))
        {
            // The route supplies navigation; these short probes handle a nearby fixture or obstruction.
            float rightX = z;
            float rightZ = -x;
            if (!Blocked(probe, rightX, rightZ))
            {
                x = rightX;
                z = rightZ;
            }
            else if (!Blocked(probe, -rightX, -rightZ))
            {
                x = -rightX;
                z = -rightZ;
            }
            else
            {
                speed = 0;
            }
        }

        float movedX = _cachedPosition.X - _previousPosition.X;
        float movedZ = _cachedPosition.Z - _previousPosition.Z;
        if (movedX * movedX + movedZ * movedZ < 0.001f * 0.001f)
            _blockedTime += dt;
        else
            _blockedTime = 0;

        if (_blockedTime > 3.0f)
        {
            // Backtrack along the same safe pavement when a temporary obstruction does not clear.
            _routeDirection = -_routeDirection;
            _waypoint = Wrap(_waypoint + _routeDirection);
            _waitRemaining = 0.6f;
            _blockedTime = 0;
            Rest(dt);
        }
        else
        {
            _character.Move(new Vector3(x * speed, 0, z * speed));
            if (speed > 0.2f)
                TurnTowards(x, z, dt);
            SetClip(speed > 0.2f && _blockedTime < 0.7f ? "Walking" : "Idle");
            _feet.Update(dt, _character.IsGrounded);
        }

        _previousPosition = _cachedPosition;
    }

    public override void OnDestroy()
    {
        _registered = false;
        ActivePedestrians.Remove(this);
    }

    private bool Blocked(Vector3 origin, float x, float z)
    {
        return Entity.RaycastStatic(origin, new Vector3(x * 0.7f, 0, z * 0.7f), out _, out Vector3 normal)
            && normal.Y < 0.6f;
    }

    private void LookAtNearbyDisplay(float dt)
    {
        float x = 36.0f - _cachedPosition.X;
        float z = -9.0f - _cachedPosition.Z;
        if (x * x + z * z < 100)
            TurnTowards(x, z, dt);
        else if (_cachedPosition.Z > 15 && MathF.Abs(_cachedPosition.X) < 8)
            TurnTowards(-_cachedPosition.X, 25.0f - _cachedPosition.Z, dt);
    }

    private void TurnTowards(float x, float z, float dt)
    {
        if (x * x + z * z < 0.0001f)
            return;
        float yaw = MathF.Atan2(x, z) * 180.0f / MathF.PI;
        Vector3 rotation = Rotation;
        rotation.Y += MathF.IEEERemainder(yaw - rotation.Y, 360.0f) * (1.0f - MathF.Exp(-9.0f * dt));
        Rotation = rotation;
    }

    private void Rest(float dt)
    {
        _character.Move(new Vector3(0));
        SetClip("Idle");
        _feet.Update(dt, _character.IsGrounded);
    }

    private void SetClip(string name)
    {
        if (_clip == name)
            return;
        _animator.CrossFade(name, 0.24f);
        _clip = name;
    }

    private int Wrap(int waypoint) => (waypoint % _route.Length + _route.Length) % _route.Length;
}
