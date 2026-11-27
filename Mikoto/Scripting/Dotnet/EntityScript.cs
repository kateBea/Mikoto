namespace Mikoto;

/// <summary>One instance per entity per Play session. Use OnCreate, not a
/// constructor, to access the entity. Transforms are local; rotation is degrees.
/// </summary>
public abstract class EntityScript
{
    private Entity? _entity;

    /// <summary>The owning native entity. Available from OnCreate through OnDestroy.</summary>
    public Entity Entity => _entity ?? throw new InvalidOperationException("The script is not bound to an entity.");

    public ulong EntityId => _entity?.Id ?? 0;

    internal void Bind(ulong sceneId, ulong entityId)
    {
        Unbind();
        _entity = new Entity(sceneId, entityId);
    }

    internal void Unbind()
    {
        _entity?.Unbind();
        _entity = null;
    }

    // Keep the existing convenience properties on the same live binding path.
    public Vector3 Position
    {
        get => Entity.GetTransform(0);
        set => Entity.SetTransform(0, value);
    }

    public Vector3 Rotation
    {
        get => Entity.GetTransform(1);
        set => Entity.SetTransform(1, value);
    }

    public Vector3 Scale
    {
        get => Entity.GetTransform(2);
        set => Entity.SetTransform(2, value);
    }

    public virtual void OnCreate()
    {
    }

    public virtual void OnUpdate(float timeStep)
    {
    }

    // Physics notifications run after simulation; other is the entity on the opposite side.
    // Stay is delivered once per simulation update, including persistent sleeping contacts.
    public virtual void OnCollisionEnter(Entity other)
    {
    }

    public virtual void OnCollisionStay(Entity other)
    {
    }

    public virtual void OnCollisionExit(Entity other)
    {
    }

    public virtual void OnTriggerEnter(Entity other)
    {
    }

    public virtual void OnTriggerStay(Entity other)
    {
    }

    public virtual void OnTriggerExit(Entity other)
    {
    }

    public virtual void OnDestroy()
    {
    }
}

// Values match Mikoto's stable input codes, independent of native SDL scancodes.
public enum KeyCode
{
    Space = 32,
    D1 = 49,
    D2, D3, D4, D5, D6, D7, D8, D9,
    A = 65,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,

    Escape = 256,
    Enter,
    Tab,
    Backspace,
    Insert,
    Delete,
    Right,
    Left,
    Down,
    Up,

    LeftShift = 340,
    LeftControl,
    LeftAlt,
    LeftSuper,
    RightShift,
    RightControl,
    RightAlt,
    RightSuper
}

public enum MouseButton
{
    Left = 0,
    Right = 1,
    Middle = 2
}

public static class Input
{
    public static bool IsKeyPressed(KeyCode key) => Hosting.NativeBridge.IsKeyPressed((int)key);

    /// <summary>True when the Game panel or standalone game owns gameplay input.</summary>
    public static bool IsFocused => Hosting.NativeBridge.IsInputFocused;

    /// <summary>True over the focused game image, excluding letterbox bars and editor panels.</summary>
    public static bool IsPointerOver => Hosting.NativeBridge.IsPointerOverInput;

    public static bool IsMouseButtonPressed(MouseButton button) => Hosting.NativeBridge.IsMouseButtonPressed((int)button);

    /// <summary>Mouse position in window pixels. Compute drag deltas in the script, without multiplying by frame time.</summary>
    public static Vector2 MousePosition => Hosting.NativeBridge.GetMousePosition();

    /// <summary>Wheel movement this frame over the focused game image; positive Y scrolls up.</summary>
    public static Vector2 MouseScrollDelta => Hosting.NativeBridge.GetMouseScrollDelta();
}

public static class Log
{
    public static void Info(string message) => Hosting.NativeBridge.Log(message);

    public static void Warning(string message) => Hosting.NativeBridge.Log(message, 1);

    public static void Error(string message) => Hosting.NativeBridge.Log(message, 2);

    public static void Debug(string message) => Hosting.NativeBridge.Log(message, 3);
}
