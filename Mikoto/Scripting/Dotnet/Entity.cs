using Mikoto.Hosting;

namespace Mikoto;

/// <summary>A script's live reference to its native scene entity.</summary>
/// <remarks>Access components during scene callbacks. Cached wrappers are invalidated when the script is unbound.</remarks>
public sealed class Entity
{
    private readonly ulong _sceneId;
    private TransformComponent? _transform;
    private LightComponent? _light;
    private CanvasComponent? _canvas;
    private VehicleComponent? _vehicle;
    private CharacterControllerComponent? _character;
    private AnimatorComponent? _animator;
    private bool _bound = true;

    /// <summary>The native entity identifier.</summary>
    public ulong Id { get; }

    internal Entity(ulong sceneId, ulong entityId)
    {
        _sceneId = sceneId;
        Id = entityId;
    }

    /// <summary>Gets a live component wrapper. Property writes modify the native component.</summary>
    /// <typeparam name="T">TransformComponent, LightComponent, CanvasComponent, or VehicleComponent.</typeparam>
    /// <returns>A cached wrapper for this entity's component.</returns>
    /// <exception cref="NotSupportedException">The component type has no managed binding.</exception>
    /// <exception cref="InvalidOperationException">The component is unavailable or access is outside a scene callback.</exception>
    public T GetComponent<T>() where T : Component
    {
        EnsureBound();
        ComponentType kind = typeof(T) == typeof(TransformComponent) ? ComponentType.Transform
            : typeof(T) == typeof(LightComponent) ? ComponentType.Light
            : typeof(T) == typeof(CanvasComponent) ? ComponentType.Canvas
            : typeof(T) == typeof(VehicleComponent) ? ComponentType.Vehicle
            : typeof(T) == typeof(AnimatorComponent) ? ComponentType.Animator
            : typeof(T) == typeof(CharacterControllerComponent) ? ComponentType.Character
            : throw new NotSupportedException($"No C# component binding exists for {typeof(T).FullName}.");

        if (!NativeBridge.HasComponent(_sceneId, Id, kind))
            throw new InvalidOperationException($"The entity has no {typeof(T).Name}, or access is outside its scene's script callback.");

        Component component = kind switch
        {
            ComponentType.Transform => _transform ??= new TransformComponent(this),
            ComponentType.Light => _light ??= new LightComponent(this),
            ComponentType.Canvas => _canvas ??= new CanvasComponent(this),
            ComponentType.Vehicle => _vehicle ??= new VehicleComponent(this),
            ComponentType.Animator => _animator ??= new AnimatorComponent(this),
            ComponentType.Character => _character ??= new CharacterControllerComponent(this),
            _ => throw new NotSupportedException()
        };
        return (T)component;
    }

    /// <summary>Finds an entity by its unique, exact name in this entity's scene.</summary>
    /// <param name="name">The authored entity name. Names must be unique for this lookup.</param>
    /// <returns>A live scene reference, or null if no entity has this name.</returns>
    /// <exception cref="InvalidOperationException">The name is ambiguous or access is outside a live scene callback.</exception>
    public Entity? Find(string name)
    {
        EnsureBound();
        ulong id = NativeBridge.FindEntity(_sceneId, Id, name);
        return id == 0 ? null : new Entity(_sceneId, id);
    }

    /// <summary>Queries static, non-sensor colliders. Direction includes the ray length in world units.</summary>
    public bool RaycastStatic(Vector3 origin, Vector3 direction, out Vector3 position, out Vector3 normal)
    {
        EnsureBound();
        return NativeBridge.RaycastStatic(_sceneId, Id, origin, direction, out position, out normal);
    }

    internal void AnimatorCommand(AnimatorOperation operation, string name, ref AnimatorRequest request)
    {
        EnsureBound();
        NativeBridge.AnimatorCommand(_sceneId, Id, operation, name, ref request);
    }

    internal void SetVehicleInputs(float throttle, float steering, float brake, float handBrake)
    {
        EnsureBound();
        NativeBridge.SetVehicleInputs(_sceneId, Id, throttle, steering, brake, handBrake);
    }

    internal void MoveCharacter(Vector3 velocity, float jumpSpeed)
    {
        EnsureBound();
        NativeBridge.SetCharacterInputs(_sceneId, Id, velocity, jumpSpeed);
    }

    internal bool IsCharacterGrounded()
    {
        EnsureBound();
        return NativeBridge.GetCharacterGrounded(_sceneId, Id);
    }

    internal float GetVehicleSpeed()
    {
        EnsureBound();
        return NativeBridge.GetVehicleSpeed(_sceneId, Id);
    }

    internal void SetCanvasText(ulong element, string text)
    {
        EnsureBound();
        NativeBridge.SetCanvasText(_sceneId, Id, element, text);
    }

    internal Vector3 GetTransform(int kind)
    {
        EnsureBound();
        return NativeBridge.GetTransform(_sceneId, Id, kind);
    }

    internal void SetTransform(int kind, Vector3 value)
    {
        EnsureBound();
        NativeBridge.SetTransform(_sceneId, Id, kind, value);
    }

    internal float GetLightIntensity()
    {
        EnsureBound();
        return NativeBridge.GetLightIntensity(_sceneId, Id);
    }

    internal void SetLightIntensity(float value)
    {
        EnsureBound();
        NativeBridge.SetLightIntensity(_sceneId, Id, value);
    }

    internal ulong FindCanvasElement(string name)
    {
        EnsureBound();
        return NativeBridge.FindCanvasElement(_sceneId, Id, name);
    }

    internal float GetCanvasValue(ulong element)
    {
        EnsureBound();
        return NativeBridge.GetCanvasValue(_sceneId, Id, element);
    }

    internal void SetCanvasValue(ulong element, float value)
    {
        EnsureBound();
        NativeBridge.SetCanvasValue(_sceneId, Id, element, value);
    }

    internal void PlayAudio(AudioClip clip)
    {
        EnsureBound();
        NativeBridge.PlayAudio(_sceneId, Id, clip.Path);
    }

    internal void Unbind() => _bound = false;

    private void EnsureBound()
    {
        if (!_bound)
        {
            throw new InvalidOperationException("This entity reference belongs to a script that has been unbound.");
        }
    }
}
