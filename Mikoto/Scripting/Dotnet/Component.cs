namespace Mikoto;

// Numeric values match ManagedComponentType in DotnetInterop.hh.
internal enum ComponentType : uint
{
    Transform = 0,
    Light = 1,
    Canvas = 2,
    Vehicle = 3,
    Character = 4,
    Animator = 5
}

/// <summary>A managed reference to a native component, rather than a copy of its data.</summary>
public abstract class Component
{
    /// <summary>The entity owning the native component.</summary>
    public Entity Entity { get; }

    internal Component(Entity entity)
    {
        Entity = entity;
    }
}
