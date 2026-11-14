using System.Diagnostics;
using System.Reflection;

namespace Mikoto.Hosting;

/// <summary>Builds C# script projects through the .NET SDK and MSBuild.</summary>
internal static class SourceCompiler
{
    private static readonly object _compileGate = new();
    private static readonly Dictionary<string, string> _compiledProjects =
        new(OperatingSystem.IsWindows() ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal);

    // Reflection and scene loading reuse the last successful build. Explicit
    // compile requests still run MSBuild, which owns incremental build decisions.
    internal static string GetAssemblyPath(string sourcePath)
    {
        lock (_compileGate)
        {
            var project = FindProject(sourcePath);
            return _compiledProjects.TryGetValue(project, out var output) && File.Exists(output)
                ? output : CompileSerialized(sourcePath);
        }
    }

    internal static string Compile(string sourcePath)
    {
        lock (_compileGate)
        {
            return CompileSerialized(sourcePath);
        }
    }

    // A project owns all scripts below its directory, unless a nearer project
    // takes ownership. Never silently choose between multiple sibling projects.
    internal static string FindProject(string sourcePath)
    {
        if (Path.GetExtension(sourcePath).Equals(".csproj", StringComparison.OrdinalIgnoreCase))
        {
            return Path.GetFullPath(sourcePath);
        }

        for (var directory = new DirectoryInfo(Path.GetDirectoryName(Path.GetFullPath(sourcePath))!);
             directory != null; directory = directory.Parent)
        {
            var projects = directory.GetFiles("*.csproj");
            if (projects.Length > 1)
            {
                throw new InvalidOperationException($"Multiple C# projects in {directory.FullName}; put each project in its own directory.");
            }

            if (projects.Length == 1)
            {
                return projects[0].FullName;
            }
        }

        throw new InvalidOperationException($"No C# project owns {sourcePath}. Place the script under your project's Assets directory or add a .csproj referencing Mikoto.ScriptCore.");
    }

    private static string RunDotnet(string project, params string[] arguments)
    {
        var start = new ProcessStartInfo("dotnet")
        {
            WorkingDirectory = Path.GetDirectoryName(project)!,
            UseShellExecute = false,
            CreateNoWindow = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true
        };

        foreach (var argument in arguments)
        {
            start.ArgumentList.Add(argument);
        }

        using var process = Process.Start(start) ?? throw new InvalidOperationException("Could not start the .NET SDK.");
        var output = process.StandardOutput.ReadToEndAsync();
        var errors = process.StandardError.ReadToEndAsync();
        if (!process.WaitForExit(120_000))
        {
            process.Kill(entireProcessTree: true);
            process.WaitForExit();
            throw new TimeoutException("C# project build exceeded two minutes.");
        }

        var text = output.GetAwaiter().GetResult();
        var errorText = errors.GetAwaiter().GetResult();
        if (process.ExitCode != 0)
        {
            throw new InvalidOperationException(text + "\n" + errorText);
        }

        return text;
    }

    private static string CompileProject(string project, Stopwatch timer)
    {
        // Let MSBuild handle incremental compilation, ProjectReference and
        // NuGet dependencies rather than maintaining a second dependency graph.
        var diagnostics = RunDotnet(project, "build", project, "--configuration", "Debug", "--nologo");
        var output = RunDotnet(project, "msbuild", project, "-nologo", "-property:Configuration=Debug",
            "-getProperty:TargetPath").Trim();
        if (!File.Exists(output))
        {
            throw new InvalidOperationException($"Project did not produce a loadable TargetPath: {output}");
        }

        NativeBridge.ReportCompilation(1, project, output, diagnostics, timer.Elapsed.TotalMilliseconds, 0, 0, 0);
        return output;
    }

    private static string CompileSerialized(string sourcePath)
    {
        var timer = Stopwatch.StartNew();
        NativeBridge.ReportCompilation(0, sourcePath, "", "Locating C# project", 0, 0, 0, 0, 1);
        try
        {
            var project = FindProject(sourcePath);
            var output = CompileProject(project, timer);
            _compiledProjects[project] = output;
            return output;
        }
        catch (Exception error)
        {
            NativeBridge.ReportCompilation(3, sourcePath, "", error.Message, timer.Elapsed.TotalMilliseconds, 0, 1, 0);
            throw;
        }
    }

    internal static Type FindScriptClass(Assembly assembly, string sourcePath, string requestedClass)
    {
        if (!string.IsNullOrWhiteSpace(requestedClass))
        {
            return assembly.GetType(requestedClass, throwOnError: true)!;
        }

        var candidates = assembly.GetTypes().Where(type => typeof(EntityScript).IsAssignableFrom(type) &&
            !type.IsAbstract && !type.ContainsGenericParameters).ToArray();

        var fileName = Path.GetFileNameWithoutExtension(sourcePath);
        var matching = candidates.Where(type => type.Name == fileName).ToArray();
        if (matching.Length == 1)
        {
            return matching[0];
        }

        if (matching.Length == 0 && candidates.Length == 1)
        {
            return candidates[0];
        }
        throw new InvalidOperationException($"{sourcePath}: expected one concrete EntityScript class named '{fileName}'. " +
            "Give each script the same name as its .cs file; namespaces are supported.");
    }
}
