using System;
using Mikoto;

/// <summary>
/// Free-flight camera: WASD moves relative to the view, Q/E descends/ascends,
/// arrow keys look around, Shift boosts speed and Control slows movement.
/// R restores the starting position and view.
/// </summary>
/// <remarks>
/// Attach to an unparented entity with a Camera component marked as the main
/// camera, choose C# and this file, then press Play. Keep the class and filename
/// the same. Tune the constants below and restart Play to apply changes.
/// Arrow keys provide keyboard look control in this example.
/// This moves the transform directly and does not perform collision detection.
/// Naming follows https://learn.microsoft.com/en-us/dotnet/csharp/fundamentals/coding-style/identifier-names.
/// </remarks>
public sealed class FlyCamera : EntityScript
{
    private const float MoveSpeed = 5.0f;          // World units per second.
    private const float LookSpeed = 90.0f;         // Degrees per second.
    private const float BoostMultiplier = 3.0f;
    private const float SlowMultiplier = 0.25f;
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

        Log.Info("Fly camera: WASD move, Q/E down/up, arrows look, Shift boost, Control slow, R reset.");
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

        // Mikoto uses degrees, Y * X * Z rotation and camera forward along -Z.
        // Negative yaw turns right; positive pitch looks up. Keep the horizon level.
        _yaw = WrapAngle(_yaw - lookHorizontal * LookSpeed * deltaTime);
        _pitch = Math.Clamp(_pitch + lookVertical * LookSpeed * deltaTime, -PitchLimit, PitchLimit);
        Rotation = new Vector3(_pitch, _yaw, 0.0f);

        float yaw = _yaw * DegreesToRadians;
        float pitch = _pitch * DegreesToRadians;
        float sinYaw = MathF.Sin(yaw);
        float cosYaw = MathF.Cos(yaw);
        float sinPitch = MathF.Sin(pitch);
        float cosPitch = MathF.Cos(pitch);

        Vector3 forward = new(-sinYaw * cosPitch, sinPitch, -cosYaw * cosPitch);
        Vector3 right = new(cosYaw, 0.0f, -sinYaw);
        Vector3 up = new(0.0f, 1.0f, 0.0f);

        float forwardInput = ReadAxis(KeyCode.S, KeyCode.W);
        float rightInput = ReadAxis(KeyCode.A, KeyCode.D);
        float upInput = ReadAxis(KeyCode.Q, KeyCode.E);
        Vector3 movement = forward * forwardInput + right * rightInput + up * upInput;
        float length = movement.Length();

        if (length > 0.0001f)
        {
            float speed = MoveSpeed;
            if (Input.IsKeyPressed(KeyCode.LeftShift) || Input.IsKeyPressed(KeyCode.RightShift))
            {
                speed *= BoostMultiplier;
            }
            if (Input.IsKeyPressed(KeyCode.LeftControl) || Input.IsKeyPressed(KeyCode.RightControl))
            {
                speed *= SlowMultiplier;
            }

            // Normalize the combined direction so diagonal travel is not faster.
            Position = Position + movement * (speed * deltaTime / length);
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
