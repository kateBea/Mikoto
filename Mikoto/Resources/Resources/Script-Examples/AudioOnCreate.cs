using Mikoto;

/// <summary>Starts an assigned audio clip when this entity's Play session begins.</summary>
/// <remarks>
/// Add an AudioSourceComponent to the entity, attach this C# script, and assign
/// Clip in the script inspector. The nullable property allows an unassigned clip.
/// Naming follows https://learn.microsoft.com/en-us/dotnet/csharp/fundamentals/coding-style/identifier-names.
/// </remarks>
public sealed class AudioOnCreate : EntityScript
{
    [SerializeField]
    public AudioClip? Clip { get; set; }

    public override void OnCreate()
    {
        Clip?.Play(Entity);
    }
}
