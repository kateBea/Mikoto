using System;
using Mikoto;

/// <summary>Drives the car and updates its follow camera and canvas controls.</summary>
/// <remarks>
/// Attached to the controls canvas. Press Play to run: WASD drives, Space brakes,
/// Shift applies the handbrake, and arrow keys orbit the camera. The canvas adjusts
/// follow, distance and power. Car and Driving Camera are unparented entities.
/// Naming follows https://learn.microsoft.com/en-us/dotnet/csharp/fundamentals/coding-style/identifier-names.
/// </remarks>
public sealed class CarDemo : EntityScript
{
    private VehicleComponent _vehicle = null!;
    private TransformComponent _car = null!;
    private TransformComponent _camera = null!;
    private CanvasElement _followControl = null!;
    private CanvasElement _distanceControl = null!;
    private CanvasElement _powerControl = null!;
    private CanvasElement _speedLabel = null!;
    private float _yaw = 180.0f;
    private float _pitch = -18.9246f;

    public override void OnCreate()
    {
        // Resolve scene names once; component wrappers belong to this Play session.
        Entity car = Entity.Find("Car") ?? throw new InvalidOperationException("Car is missing from the demo scene.");
        Entity camera = Entity.Find("Driving Camera") ?? throw new InvalidOperationException("Driving Camera is missing from the demo scene.");
        _car = car.GetComponent<TransformComponent>();
        _vehicle = car.GetComponent<VehicleComponent>();
        _camera = camera.GetComponent<TransformComponent>();

        // Element names are the authored bindings in the canvas inspector.
        CanvasComponent canvas = Entity.GetComponent<CanvasComponent>();
        _followControl = canvas.FindElement("Follow car");
        _distanceControl = canvas.FindElement("Camera distance");
        _powerControl = canvas.FindElement("Drive power");
        _speedLabel = canvas.FindElement("Speed");
        UpdateCamera(0.0f);
    }

    public override void OnUpdate(float timeStep)
    {
        float deltaTime = Math.Clamp(timeStep, 0.0f, 0.1f);
        float throttle = ReadAxis(KeyCode.S, KeyCode.W) * _powerControl.Value;
        float steering = ReadAxis(KeyCode.A, KeyCode.D);
        float brake = Input.IsKeyPressed(KeyCode.Space) ? 1.0f : 0.0f;
        float handBrake = Input.IsKeyPressed(KeyCode.LeftShift) || Input.IsKeyPressed(KeyCode.RightShift) ? 1.0f : 0.0f;
        float speed = _vehicle.ForwardSpeed;

        // Opposite throttle first stops the chassis, then engages the other direction.
        // Reference: https://github.com/jrouwe/JoltPhysics/blob/master/Samples/Tests/Vehicle/VehicleConstraintTest.cpp
        if (throttle * speed < -0.1f)
        {
            throttle = 0.0f;
            brake = 1.0f;
        }
        if (handBrake > 0.0f)
        {
            throttle = 0.0f;
        }

        _vehicle.SetInputs(throttle, steering, brake, handBrake);
        _speedLabel.Text = $"Speed: {MathF.Abs(speed) * 3.6f:0} km/h";
        UpdateCamera(deltaTime);
    }

    private void UpdateCamera(float deltaTime)
    {
        // Disabling follow freezes the game camera; editor navigation remains independent.
        if (_followControl.Value < 0.5f)
        {
            return;
        }

        _yaw = MathF.IEEERemainder(_yaw - ReadAxis(KeyCode.Left, KeyCode.Right) * 80.0f * deltaTime, 360.0f);
        _pitch = Math.Clamp(_pitch + ReadAxis(KeyCode.Down, KeyCode.Up) * 55.0f * deltaTime, -75.0f, -5.0f);

        // Transform rotations use degrees; trigonometry expects radians.
        float yaw = _yaw * MathF.PI / 180.0f;
        float pitch = _pitch * MathF.PI / 180.0f;
        float distance = 4.0f + _distanceControl.Value * 14.0f;
        Vector3 backward = new(MathF.Sin(yaw) * MathF.Cos(pitch), -MathF.Sin(pitch), MathF.Cos(yaw) * MathF.Cos(pitch));
        _camera.Position = _car.Position + new Vector3(0.0f, 0.6f, 0.0f) + backward * distance;
        _camera.Rotation = new Vector3(_pitch, _yaw, 0.0f);
    }

    public override void OnDestroy()
    {
        // Release held input before the runtime scene is torn down.
        if (_vehicle != null)
        {
            _vehicle.SetInputs(0.0f, 0.0f, 0.0f);
        }
    }

    private static float ReadAxis(KeyCode negative, KeyCode positive)
        => (Input.IsKeyPressed(positive) ? 1.0f : 0.0f) - (Input.IsKeyPressed(negative) ? 1.0f : 0.0f);
}
