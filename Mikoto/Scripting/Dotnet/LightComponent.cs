namespace Mikoto;

/// <summary>Live access to the entity's active directional, point, spot, or area light.</summary>
public sealed class LightComponent : Component
{
    internal LightComponent(Entity entity) : base(entity)
    {
    }

    /// <summary>Non-negative light intensity. Zero switches off its illumination.</summary>
    /// <remarks>Access during this scene's script callbacks. Writes update the native light immediately.</remarks>
    public float Intensity
    {
        get => Entity.GetLightIntensity();
        set => Entity.SetLightIntensity(value);
    }
}
