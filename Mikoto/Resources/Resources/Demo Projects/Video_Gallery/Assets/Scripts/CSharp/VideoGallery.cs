using System;
using Mikoto;

/// <summary>Moves the gallery's bunny and switches between orbit and free-flight cameras.</summary>
/// <remarks>
/// Attached to the bunny root. Press Play and focus the Game panel. P toggles
/// camera mode, RMB looks around, and WASD moves the bunny or camera. The wheel
/// adjusts orbit distance; Q/E moves vertically in free flight; Shift boosts speed.
/// Naming follows https://learn.microsoft.com/en-us/dotnet/csharp/fundamentals/coding-style/identifier-names.
/// </remarks>
public sealed class VideoGallery : EntityScript
{
    private TransformComponent _camera = null!;
    private CanvasElement _modeLabel = null!;
    private CanvasElement _controlsLabel = null!;
    private Vector2 _previousMousePosition;
    private bool _wasDragging;
    private bool _freeFlyEnabled;
    private bool _modeKeyWasPressed;
    private float _orbitYaw;
    private float _orbitPitch = -12;
    private float _orbitDistance = 7;
    private float _flyYaw;
    private float _flyPitch;

    public override void OnCreate()
    {
        // Resolve the independent game camera and the two authored canvas labels once.
        _camera = (Entity.Find("Orbit Camera") ?? throw new InvalidOperationException("Orbit Camera is missing")).GetComponent<TransformComponent>();
        CanvasComponent canvas = (Entity.Find("Video gallery controls") ?? throw new InvalidOperationException("Camera controls canvas is missing")).GetComponent<CanvasComponent>();
        _modeLabel = canvas.FindElement("Camera mode");
        _controlsLabel = canvas.FindElement("Camera controls");
        UpdateModeLabels();
        UpdateOrbitCamera();
    }

    public override void OnUpdate(float timeStep)
    {
        float deltaTime = Math.Clamp(timeStep, 0.0f, 0.1f);
        bool modeKeyPressed = Input.IsFocused && Input.IsKeyPressed(KeyCode.P);

        // Toggle once per press and enter free flight from the displayed camera pose.
        if (modeKeyPressed && !_modeKeyWasPressed)
        {
            _freeFlyEnabled = !_freeFlyEnabled;
            _flyYaw = _camera.Rotation.Y;
            _flyPitch = _camera.Rotation.X;
            _wasDragging = false;
            UpdateModeLabels();
        }

        _modeKeyWasPressed = modeKeyPressed;
        Vector2 mousePosition = Input.MousePosition;
        bool dragging = Input.IsFocused && Input.IsPointerOver && Input.IsMouseButtonPressed(MouseButton.Right);

        if (dragging && _wasDragging)
        {
            float yawDelta = (mousePosition.X - _previousMousePosition.X) * 0.22f;
            float pitchDelta = (mousePosition.Y - _previousMousePosition.Y) * 0.22f;

            if (_freeFlyEnabled)
            {
                _flyYaw = MathF.IEEERemainder(_flyYaw - yawDelta, 360);
                _flyPitch = Math.Clamp(_flyPitch - pitchDelta, -89, 89);
                _camera.Rotation = new Vector3(_flyPitch, _flyYaw, 0);
            }
            else
            {
                _orbitYaw = MathF.IEEERemainder(_orbitYaw - yawDelta, 360);
                _orbitPitch = Math.Clamp(_orbitPitch - pitchDelta, -70, 10);
            }
        }

        _wasDragging = dragging;
        _previousMousePosition = mousePosition;

        // Free flight leaves the bunny in place; orbit retains its own angles and distance.
        if (_freeFlyEnabled)
        {
            MoveFlyCamera(deltaTime);
            return;
        }

        if (Input.IsFocused && Input.IsPointerOver)
        {
            _orbitDistance = Math.Clamp(_orbitDistance - Input.MouseScrollDelta.Y * 0.6f, 2, 16);
        }

        float horizontalInput = Input.IsFocused ? ReadAxis(KeyCode.A, KeyCode.D) : 0;
        float forwardInput = Input.IsFocused ? ReadAxis(KeyCode.S, KeyCode.W) : 0;

        // Normalize diagonal input so movement speed is independent of key combinations.
        float length = MathF.Sqrt(horizontalInput * horizontalInput + forwardInput * forwardInput);

        if (length > 0)
        {
            float yaw = _orbitYaw * MathF.PI / 180;
            float movementX = (horizontalInput * MathF.Cos(yaw) - forwardInput * MathF.Sin(yaw)) / length;
            float movementZ = (-horizontalInput * MathF.Sin(yaw) - forwardInput * MathF.Cos(yaw)) / length;
            float speed = Input.IsKeyPressed(KeyCode.LeftShift) ? 6 : 3;
            Position += new Vector3(movementX, 0, movementZ) * (speed * deltaTime);
            Rotation = new Vector3(0, MathF.Atan2(movementX, movementZ) * 180 / MathF.PI, 0);
        }

        UpdateOrbitCamera();
    }

    private void MoveFlyCamera(float deltaTime)
    {
        if (!Input.IsFocused)
        {
            return;
        }

        float horizontalInput = ReadAxis(KeyCode.A, KeyCode.D);
        float verticalInput = ReadAxis(KeyCode.Q, KeyCode.E);
        float forwardInput = ReadAxis(KeyCode.S, KeyCode.W);

        if (Input.IsKeyPressed(KeyCode.Space))
        {
            verticalInput = 1;
        }

        float yaw = _flyYaw * MathF.PI / 180;
        float pitch = _flyPitch * MathF.PI / 180;
        Vector3 forward = new(-MathF.Sin(yaw) * MathF.Cos(pitch), MathF.Sin(pitch), -MathF.Cos(yaw) * MathF.Cos(pitch));
        Vector3 right = new(MathF.Cos(yaw), 0, -MathF.Sin(yaw));
        Vector3 movement = right * horizontalInput + forward * forwardInput + new Vector3(0, verticalInput, 0);
        float length = movement.Length();

        if (length > 0)
        {
            float speed = Input.IsKeyPressed(KeyCode.LeftShift) ? 18 : 6;
            _camera.Position += movement / length * (speed * deltaTime);
        }
    }

    private void UpdateModeLabels()
    {
        _modeLabel.Text = _freeFlyEnabled ? "FREE FLY CAMERA  |  P: return to bunny orbit" : "BUNNY ORBIT CAMERA  |  P: free fly";
        _controlsLabel.Text = _freeFlyEnabled
            ? "WASD: fly  |  Q/E: down/up  |  RMB: look  |  Shift: faster"
            : "WASD: move bunny  |  RMB: orbit  |  Wheel: distance  |  Shift: run";
    }

    private void UpdateOrbitCamera()
    {
        float yaw = _orbitYaw * MathF.PI / 180;
        float pitch = _orbitPitch * MathF.PI / 180;
        Vector3 backward = new(MathF.Sin(yaw) * MathF.Cos(pitch), -MathF.Sin(pitch), MathF.Cos(yaw) * MathF.Cos(pitch));
        _camera.Position = Position + new Vector3(0, 1.25f, 0) + backward * _orbitDistance;
        _camera.Rotation = new Vector3(_orbitPitch, _orbitYaw, 0);
    }

    private static float ReadAxis(KeyCode negative, KeyCode positive)
        => (Input.IsKeyPressed(positive) ? 1 : 0) - (Input.IsKeyPressed(negative) ? 1 : 0);
}
