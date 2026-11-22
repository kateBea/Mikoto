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

using System;
using Mikoto;

/// <summary>Flies between the particle museum exhibits while the simulation runs.</summary>
public sealed class ParticleMuseumCamera : EntityScript
{
    private const float MoveSpeed = 7.0f;
    private const float BoostMultiplier = 3.0f;
    private const float LookSensitivity = 0.22f;
    private const float PitchLimit = 89.0f;
    private const float DegreesToRadians = MathF.PI / 180.0f;

    private Vector2 _previousMousePosition;
    private bool _wasLooking;

    public override void OnUpdate(float timeStep)
    {
        if (!Input.IsFocused)
        {
            _wasLooking = false;
            return;
        }

        UpdateLook();

        if (timeStep > 0.0f)
        {
            Move(MathF.Min(timeStep, 0.1f));
        }
    }

    private void UpdateLook()
    {
        bool looking = Input.IsPointerOver && Input.IsMouseButtonPressed(MouseButton.Right);
        Vector2 mousePosition = Input.MousePosition;

        // Start a new drag from the current cursor position to avoid snapping the view.
        if (looking && _wasLooking)
        {
            float horizontal = (mousePosition.X - _previousMousePosition.X) * LookSensitivity;
            float vertical = (mousePosition.Y - _previousMousePosition.Y) * LookSensitivity;

            if (horizontal != 0.0f || vertical != 0.0f)
            {
                Vector3 rotation = Rotation;
                float pitch = Math.Clamp(rotation.X - vertical, -PitchLimit, PitchLimit);
                float yaw = MathF.IEEERemainder(rotation.Y - horizontal, 360.0f);
                Rotation = new Vector3(pitch, yaw, 0.0f);
            }
        }

        _wasLooking = looking;
        _previousMousePosition = mousePosition;
    }

    private void Move(float deltaTime)
    {
        float horizontal = ReadAxis(KeyCode.A, KeyCode.D);
        float forwardInput = ReadAxis(KeyCode.S, KeyCode.W);
        float vertical = Input.IsKeyPressed(KeyCode.Space) ? 1.0f : ReadAxis(KeyCode.Q, KeyCode.E);

        if (horizontal == 0.0f && forwardInput == 0.0f && vertical == 0.0f)
        {
            return;
        }

        // Camera forward is -Z in Mikoto. Up and down stay aligned with the room.
        Vector3 rotation = Rotation;
        float yaw = rotation.Y * DegreesToRadians;
        float pitch = rotation.X * DegreesToRadians;
        float sinYaw = MathF.Sin(yaw);
        float cosYaw = MathF.Cos(yaw);
        float cosPitch = MathF.Cos(pitch);
        Vector3 forward = new(-sinYaw * cosPitch, MathF.Sin(pitch), -cosYaw * cosPitch);
        Vector3 right = new(cosYaw, 0.0f, -sinYaw);
        Vector3 movement = forward * forwardInput + right * horizontal + new Vector3(0.0f, vertical, 0.0f);
        float length = movement.Length();

        if (length > 0.0f)
        {
            bool boost = Input.IsKeyPressed(KeyCode.LeftShift) || Input.IsKeyPressed(KeyCode.RightShift);
            float speed = boost ? MoveSpeed * BoostMultiplier : MoveSpeed;
            Position += movement * (speed * deltaTime / length);
        }
    }

    private static float ReadAxis(KeyCode negative, KeyCode positive)
        => (Input.IsKeyPressed(positive) ? 1.0f : 0.0f) - (Input.IsKeyPressed(negative) ? 1.0f : 0.0f);
}
