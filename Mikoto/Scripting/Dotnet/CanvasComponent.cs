namespace Mikoto;

/// <summary>Live access to named controls in the entity's retained canvas.</summary>
public sealed class CanvasComponent : Component
{
    internal CanvasComponent(Entity entity) : base(entity)
    {
    }

    /// <summary>Finds a named control and retains its stable canvas ID.</summary>
    /// <param name="name">Exact, unique control name authored in the canvas.</param>
    /// <returns>A live control wrapper, unaffected by reordering canvas elements.</returns>
    /// <exception cref="KeyNotFoundException">No control has this name.</exception>
    /// <remarks>Use during scene callbacks; the wrapper expires when its script is unbound.</remarks>
    public CanvasElement FindElement(string name) => new(Entity, Entity.FindCanvasElement(name));
}

/// <summary>A control identified by its canvas-local ID rather than its list index.</summary>
public sealed class CanvasElement
{
    private readonly Entity _entity;

    internal CanvasElement(Entity entity, ulong id)
    {
        _entity = entity;
        Id = id;
    }

    /// <summary>The control's persistent canvas-local identifier.</summary>
    public ulong Id { get; }

    /// <summary>The normalized slider/toggle value; writes clamp to [0, 1].</summary>
    /// <remarks>Access during scene callbacks. Writes use Canvas.SetValue and retain native change events.</remarks>
    public float Value
    {
        get => _entity.GetCanvasValue(Id);
        set => _entity.SetCanvasValue(Id, value);
    }

    /// <summary>Updates this control's displayed label during a live scene callback.</summary>
    public string Text
    {
        set => _entity.SetCanvasText(Id, value);
    }
}
