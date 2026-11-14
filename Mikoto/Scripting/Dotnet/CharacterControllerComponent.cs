namespace Mikoto;

/// <summary>Script movement for either Jolt character implementation.</summary>
public sealed class CharacterControllerComponent : Component
{
    internal CharacterControllerComponent(Entity entity) : base(entity) { }

    /// <summary>Whether the most recent simulation step found walkable ground.</summary>
    public bool IsGrounded => Entity.IsCharacterGrounded();

    /// <summary>Sets world-space horizontal velocity (m/s) and optional one-step jump speed.</summary>
    /// <remarks>Call each update. Trigger jump on a key edge; zero velocity stops horizontal movement.</remarks>
    public void Move(Vector3 velocity, float jumpSpeed = 0.0f) => Entity.MoveCharacter(velocity, jumpSpeed);
}
