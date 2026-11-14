namespace Mikoto;

/// <summary>Script-driven inputs for a simulated vehicle whose forward direction is local +Z.</summary>
public sealed class VehicleComponent : Component
{
    internal VehicleComponent(Entity entity) : base(entity) { }

    /// <summary>Signed chassis speed along its forward axis, in meters per second.</summary>
    public float ForwardSpeed => Entity.GetVehicleSpeed();

    /// <summary>Updates the vehicle's transient controls without recreating its physics constraint.</summary>
    /// <param name="throttle">Reverse to forward drive, clamped to [-1, 1].</param>
    /// <param name="steering">Left to right steering, clamped to [-1, 1].</param>
    /// <param name="brake">Service-brake pressure, clamped to [0, 1].</param>
    /// <param name="handBrake">Handbrake pressure, clamped to [0, 1].</param>
    /// <remarks>Call every update, including zero inputs when released. Access requires a live scene callback.</remarks>
    public void SetInputs(float throttle, float steering, float brake, float handBrake = 0.0f)
        => Entity.SetVehicleInputs(throttle, steering, brake, handBrake);
}
