using System;
using Mikoto;

/// <summary>Switches between sandbox vehicles, characters and the overview camera.</summary>
/// <remarks>
/// Attached to the controls canvas. In Play, keys 1-4 select a vehicle, 5-6 select
/// a character, and 7 restores the overview. WASD moves, Space brakes or jumps,
/// Shift applies the handbrake, and arrow keys orbit the selected actor.
/// Naming follows https://learn.microsoft.com/en-us/dotnet/csharp/fundamentals/coding-style/identifier-names.
/// </remarks>
public sealed class SandboxDemo : EntityScript
{
    private static readonly string[] s_actorNames =
    [
        "Wheeled Car",
        "6DOF Car",
        "Motorcycle",
        "Tank",
        "Virtual Character",
        "Rigid Character"
    ];

    private readonly TransformComponent[] _actorTransforms = new TransformComponent[6];
    private readonly VehicleComponent[] _vehicles = new VehicleComponent[4];
    private readonly CharacterControllerComponent[] _characters = new CharacterControllerComponent[2];
    private readonly bool[] _selectionKeyWasPressed = new bool[7];
    private TransformComponent _camera = null!;
    private CanvasElement _statusLabel = null!;
    private Vector3 _overviewPosition;
    private Vector3 _overviewRotation;
    private int _selectedActorIndex = -1;
    private bool _jumpWasPressed;
    private float _smoothedSteering;
    private float _cameraYaw = 180.0f;
    private float _cameraPitch = -20.0f;

    public override void OnCreate()
    {
        // The first four named actors have vehicles; the final two have character controllers.
        // Cache wrappers once instead of searching the scene in every update.
        for (int i = 0; i < s_actorNames.Length; ++i)
        {
            Entity target = Entity.Find(s_actorNames[i]) ?? throw new InvalidOperationException($"Missing sandbox actor: {s_actorNames[i]}");
            _actorTransforms[i] = target.GetComponent<TransformComponent>();

            if (i < 4)
            {
                _vehicles[i] = target.GetComponent<VehicleComponent>();
            }
            else
            {
                _characters[i - 4] = target.GetComponent<CharacterControllerComponent>();
            }
        }

        _camera = (Entity.Find("Camera") ?? throw new InvalidOperationException("Sandbox camera is missing.")).GetComponent<TransformComponent>();
        _overviewPosition = _camera.Position;
        _overviewRotation = _camera.Rotation;
        _statusLabel = Entity.GetComponent<CanvasComponent>().FindElement("Control status");
    }

    public override void OnUpdate(float timeStep)
    {
        float deltaTime = Math.Clamp(timeStep, 0.0f, 0.1f);

        // Remember each key's previous state so holding a key selects only once.
        for (int i = 0; i < _selectionKeyWasPressed.Length; ++i)
        {
            bool keyPressed = Input.IsKeyPressed((KeyCode)((int)KeyCode.D1 + i));
            if (keyPressed && !_selectionKeyWasPressed[i])
            {
                SelectActor(i < 6 ? i : -1);
            }

            _selectionKeyWasPressed[i] = keyPressed;
        }

        float throttle = ReadAxis(KeyCode.S, KeyCode.W);
        float steering = ReadAxis(KeyCode.A, KeyCode.D);
        float brake = Input.IsKeyPressed(KeyCode.Space) ? 1.0f : 0.0f;
        float handBrake = Input.IsKeyPressed(KeyCode.LeftShift) ? 1.0f : 0.0f;
        _smoothedSteering += Math.Clamp(steering - _smoothedSteering, -4.0f * deltaTime, 4.0f * deltaTime);

        // Unselected vehicles remain parked while the selected controller receives input.
        for (int i = 0; i < _vehicles.Length; ++i)
        {
            if (i != _selectedActorIndex)
            {
                _vehicles[i].SetInputs(0, 0, 1);
                continue;
            }

            float driveInput = throttle;
            float brakePressure = brake;

            // Brake before reversing, following Jolt's vehicle samples:
            // https://github.com/jrouwe/JoltPhysics/blob/master/Samples/Tests/Vehicle/VehicleConstraintTest.cpp
            if (driveInput * _vehicles[i].ForwardSpeed < -0.1f)
            {
                driveInput = 0;
                brakePressure = 1;
            }

            if (handBrake > 0)
            {
                driveInput = 0;
            }

            _vehicles[i].SetInputs(driveInput, i == 2 ? _smoothedSteering : steering, brakePressure, handBrake);
        }

        // Jump is an edge-triggered request consumed by one physics step.
        bool jumpPressed = Input.IsKeyPressed(KeyCode.Space);

        for (int i = 0; i < _characters.Length; ++i)
        {
            Vector3 velocity = i + 4 == _selectedActorIndex ? new Vector3(steering * 4.0f, 0, throttle * 4.0f) : new Vector3();
            _characters[i].Move(velocity, i + 4 == _selectedActorIndex && jumpPressed && !_jumpWasPressed ? 4.0f : 0.0f);
        }

        _jumpWasPressed = jumpPressed;

        if (_selectedActorIndex < 0)
        {
            _statusLabel.Text = "Overview | 1 car  2 6DOF  3 bike  4 tank  5/6 characters";
            return;
        }

        _statusLabel.Text = _selectedActorIndex < 4
            ? $"{s_actorNames[_selectedActorIndex]} | {MathF.Abs(_vehicles[_selectedActorIndex].ForwardSpeed) * 3.6f:0} km/h"
            : $"{s_actorNames[_selectedActorIndex]} | {(_characters[_selectedActorIndex - 4].IsGrounded ? "grounded" : "airborne")}";

        UpdateFollowCamera(deltaTime);
    }

    private void SelectActor(int index)
    {
        _selectedActorIndex = index;
        _smoothedSteering = 0;

        if (index < 0)
        {
            _camera.Position = _overviewPosition;
            _camera.Rotation = _overviewRotation;
        }
    }

    private void UpdateFollowCamera(float deltaTime)
    {
        _cameraYaw -= ReadAxis(KeyCode.Left, KeyCode.Right) * 80.0f * deltaTime;
        _cameraPitch = Math.Clamp(_cameraPitch + ReadAxis(KeyCode.Down, KeyCode.Up) * 50.0f * deltaTime, -75.0f, -5.0f);

        float yaw = _cameraYaw * MathF.PI / 180.0f;
        float pitch = _cameraPitch * MathF.PI / 180.0f;
        float distance = _selectedActorIndex == 3 ? 15.0f : _selectedActorIndex < 4 ? 9.0f : 6.0f;
        Vector3 backward = new(MathF.Sin(yaw) * MathF.Cos(pitch), -MathF.Sin(pitch), MathF.Cos(yaw) * MathF.Cos(pitch));

        _camera.Position = _actorTransforms[_selectedActorIndex].Position + new Vector3(0, 1, 0) + backward * distance;
        _camera.Rotation = new Vector3(_cameraPitch, _cameraYaw, 0);
    }

    public override void OnDestroy()
    {
        foreach (VehicleComponent? vehicle in _vehicles)
        {
            vehicle?.SetInputs(0, 0, 0);
        }

        foreach (CharacterControllerComponent? character in _characters)
        {
            character?.Move(new Vector3());
        }
    }

    private static float ReadAxis(KeyCode negative, KeyCode positive)
        => (Input.IsKeyPressed(positive) ? 1.0f : 0.0f) - (Input.IsKeyPressed(negative) ? 1.0f : 0.0f);
}
