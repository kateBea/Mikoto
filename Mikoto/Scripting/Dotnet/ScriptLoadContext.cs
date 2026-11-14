using System.Reflection;
using System.Runtime.Loader;

namespace Mikoto.Hosting;

// Each scene gets isolated user assemblies/statics. ScriptCore stays shared so
// a loaded EntityScript is the same type as the bridge's EntityScript.
internal sealed class ScriptLoadContext(string assemblyPath) : AssemblyLoadContext(isCollectible: true)
{
    private readonly AssemblyDependencyResolver _resolver = new(assemblyPath);

    internal Assembly LoadScript(string path)
    {
        // Stream loading releases the original DLL immediately, so a rebuild
        // can overwrite it while the current Play session still uses old code.
        using var assembly = File.OpenRead(path);
        var symbols = Path.ChangeExtension(path, ".pdb");
        if (File.Exists(symbols))
        {
            using var pdb = File.OpenRead(symbols);
            return LoadFromStream(assembly, pdb);
        }

        return LoadFromStream(assembly);
    }

    protected override Assembly? Load(AssemblyName name)
    {
        var core = typeof(EntityScript).Assembly;
        if (name.Name == core.GetName().Name)
        {
            return core;
        }

        var path = _resolver.ResolveAssemblyToPath(name);
        return path == null ? null : LoadScript(path);
    }

    protected override nint LoadUnmanagedDll(string name)
    {
        var path = _resolver.ResolveUnmanagedDllToPath(name);
        return path == null ? 0 : LoadUnmanagedDllFromPath(path);
    }
}
