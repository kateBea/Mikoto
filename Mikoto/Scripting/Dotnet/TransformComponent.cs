namespace Mikoto;

/// <summary>Live access to the native transform through its getters and setters.</summary>
/// <remarks>
/// Transforms are local and rotation is expressed in degrees. Vector properties return values;
/// assign an edited vector back to its property to update the native component.
/// </remarks>
public sealed class TransformComponent : Component
{
    internal TransformComponent(Entity entity) : base(entity)
    {
    }

    /// <summary>Local translation. Assigning updates the native transform and its dirty state.</summary>
    public Vector3 Position
    {
        get => Entity.GetTransform(0);
        set => Entity.SetTransform(0, value);
    }

    /// <summary>Local Euler rotation in degrees.</summary>
    public Vector3 Rotation
    {
        get => Entity.GetTransform(1);
        set => Entity.SetTransform(1, value);
    }

    /// <summary>Local scale.</summary>
    public Vector3 Scale
    {
        get => Entity.GetTransform(2);
        set => Entity.SetTransform(2, value);
    }
}
