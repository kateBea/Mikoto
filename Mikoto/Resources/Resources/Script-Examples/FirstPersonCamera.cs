using System;
using Mikoto;

/// <summary>
/// First-person camera: WASD walks on the horizontal plane, arrow keys look
/// around, Shift sprints and R restores the starting position and view.
/// Looking up or down does not change movement speed or height.
/// </summary>
/// <remarks>
/// Attach to an unparented entity with a Camera component marked as the main
/// camera. Set its starting Y position to the desired eye height, choose C# and
/// this file, then press Play. Keep the class and filename the same.
/// Tune the constants below and restart Play to apply changes.
/// This example keeps eye height fixed and uses arrow keys for keyboard look.
/// Use CharacterControllerComponent for movement with collisions and jumping,
/// as demonstrated by the Sandbox project's character controls.
/// Naming follows https://learn.microsoft.com/en-us/dotnet/csharp/fundamentals/coding-style/identifier-names.
/// </remarks>
public sealed class FirstPersonCamera : EntityScript
{
    private const float WalkSpeed = 3.5f;         // World units per second.
    private const float SprintMultiplier = 1.8f;
    private const float LookSpeed = 90.0f;        // Degrees per second.
    private const float PitchLimit = 89.0f;
    private const float DegreesToRadians = MathF.PI / 180.0f;

    private Vector3 _startPosition;
    private Vector3 _startRotation;
    private float _pitch;
    private float _yaw;
    private bool _resetWasPressed;

    public override void OnCreate()
    {
        _startPosition = Position;
        _startRotation = Rotation;
        ResetView();

        Log.Info("First-person camera: WASD walk, arrows look, Shift sprint, R reset.");
    }

    public override void OnUpdate(float timeStep)
    {
        if (timeStep <= 0.0f)
        {
            return;
        }

        // Avoid a large camera jump after a debugger pause or a stalled frame.
        float deltaTime = MathF.Min(timeStep, 0.1f);
        bool resetPressed = Input.IsKeyPressed(KeyCode.R);

        if (resetPressed && !_resetWasPressed)
        {
            ResetView();
        }

        _resetWasPressed = resetPressed;

        float lookHorizontal = ReadAxis(KeyCode.Left, KeyCode.Right);
        float lookVertical = ReadAxis(KeyCode.Down, KeyCode.Up);

        // Match Mikoto's Y * X * Z rotation, in degrees, with forward along -Z.
        _yaw = WrapAngle(_yaw - lookHorizontal * LookSpeed * deltaTime);
        _pitch = Math.Clamp(_pitch + lookVertical * LookSpeed * deltaTime, -PitchLimit, PitchLimit);
        Rotation = new Vector3(_pitch, _yaw, 0.0f);

        float yaw = _yaw * DegreesToRadians;
        float sinYaw = MathF.Sin(yaw);
        float cosYaw = MathF.Cos(yaw);

        // Walking uses yaw only. Including pitch would make looking down slow
        // the player or move the camera into the ground.
        Vector3 forward = new(-sinYaw, 0.0f, -cosYaw);
        Vector3 right = new(cosYaw, 0.0f, -sinYaw);
        float forwardInput = ReadAxis(KeyCode.S, KeyCode.W);
        float rightInput = ReadAxis(KeyCode.A, KeyCode.D);
        float length = new Vector2(forwardInput, rightInput).Length();

        if (length > 0.0f)
        {
            float speed = WalkSpeed;
            if (Input.IsKeyPressed(KeyCode.LeftShift) || Input.IsKeyPressed(KeyCode.RightShift))
            {
                speed *= SprintMultiplier;
            }

            Vector3 movement = forward * forwardInput + right * rightInput;
            Vector3 position = Position + movement * (speed * deltaTime / length);
            position.Y = _startPosition.Y;
            Position = position;
        }
    }

    private void ResetView()
    {
        Position = _startPosition;
        _pitch = Math.Clamp(WrapAngle(_startRotation.X), -PitchLimit, PitchLimit);
        _yaw = WrapAngle(_startRotation.Y);
        Rotation = new Vector3(_pitch, _yaw, 0.0f);
    }

    private static float ReadAxis(KeyCode negative, KeyCode positive)
    {
        return (Input.IsKeyPressed(positive) ? 1.0f : 0.0f)
             - (Input.IsKeyPressed(negative) ? 1.0f : 0.0f);
    }

    private static float WrapAngle(float degrees)
    {
        return MathF.IEEERemainder(degrees, 360.0f);
    }
}
