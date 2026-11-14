namespace Mikoto;

/// <summary>A serialized reference to a project asset. Native resources stay owned by the engine.</summary>
public abstract class Asset
{
    public string Path { get; }
    protected Asset(string path) => Path = path ?? throw new ArgumentNullException(nameof(path));
    public override string ToString() => Path;
}

/// <summary>An audio asset that can be assigned to an entity's audio source.</summary>
public sealed class AudioClip(string path) : Asset(path)
{
    /// <summary>Assigns this clip to the entity's AudioSourceComponent and starts playback.</summary>
    /// <param name="entity">Live entity in the current scene callback.</param>
    public void Play(Entity entity) => entity.PlayAudio(this);
}

/// <summary>A texture asset reference for serialized script properties.</summary>
public sealed class Texture(string path) : Asset(path);

/// <summary>A material asset reference for serialized script properties.</summary>
public sealed class Material(string path) : Asset(path);
