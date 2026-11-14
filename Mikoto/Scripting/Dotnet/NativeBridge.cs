using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;

namespace Mikoto.Hosting;

/// <summary>Dispatches native engine commands and calls the C++ binding table.</summary>
/// <remarks>
/// Execute is loaded by hostfxr from Mikoto.ScriptCore.dll. The sequential structs
/// below mirror Scripting/Dotnet/DotnetInterop.hh; field order and types define the ABI.
/// Renaming C# fields does not change their native layout.
/// </remarks>
public static unsafe class NativeBridge
{
    private const uint ApiVersion = 9;

    // Native callbacks and scene sessions are shared across gameplay/compiler services.
    private static readonly object _gate = new();
    private static readonly Dictionary<ulong, Session> _sessions = new();
    private static NativeApi _api;

    /// <summary>Commands received from the C++ scripting service.</summary>
    /// <remarks>
    /// Matches mikoto::scripting::ManagedOperation in Scripting/Dotnet/DotnetInterop.hh.
    /// Keep the uint underlying type and explicit values synchronized with that enum.
    /// </remarks>
    private enum ManagedOperation : uint
    {
        Bind = 0,
        Create = 1,
        Update = 2,
        Destroy = 3,
        StopScene = 4,
        Compile = 5,
        ReloadScene = 6,
        Inspect = 7,
        SetFields = 8
    }

    /// <summary>Matches the C++ ManagedNativeApi function table.</summary>
    [StructLayout(LayoutKind.Sequential)]
    private struct NativeApi
    {
        public uint Size;
        public uint Version;

        public delegate* unmanaged[Cdecl]<ulong, ulong, int, Vector3*, int> GetTransform;
        public delegate* unmanaged[Cdecl]<ulong, ulong, int, Vector3*, int> SetTransform;
        public delegate* unmanaged[Cdecl]<int, int> IsKeyPressed;
        public delegate* unmanaged[Cdecl]<int, byte*, void> Log;
        public delegate* unmanaged[Cdecl]<CompilationReport*, void> ReportCompilation;

        public delegate* unmanaged[Cdecl]<ScriptClassInfo*, void> RegisterScriptClass;
        public delegate* unmanaged[Cdecl]<ScriptFieldInfo*, void> RegisterScriptField;

        public delegate* unmanaged[Cdecl]<uint, void> UnregisterScriptClass;
        public delegate* unmanaged[Cdecl]<ulong, ulong, byte*, int> PlayAudio;
        public NativeMathApi* Math;

        public delegate* unmanaged[Cdecl]<ulong, ulong, ComponentType, int> HasComponent;
        public delegate* unmanaged[Cdecl]<ulong, ulong, float*, int> GetLightIntensity;
        public delegate* unmanaged[Cdecl]<ulong, ulong, float, int> SetLightIntensity;
        public delegate* unmanaged[Cdecl]<ulong, ulong, byte*, ulong*, int> FindCanvasElement;
        public delegate* unmanaged[Cdecl]<ulong, ulong, ulong, float*, int> GetCanvasValue;
        public delegate* unmanaged[Cdecl]<ulong, ulong, ulong, float, int> SetCanvasValue;
        public delegate* unmanaged[Cdecl]<ulong, ulong, byte*, ulong*, int> FindEntity;
        public delegate* unmanaged[Cdecl]<ulong, ulong, float, float, float, float, int> SetVehicleInputs;
        public delegate* unmanaged[Cdecl]<ulong, ulong, float*, int> GetVehicleSpeed;
        public delegate* unmanaged[Cdecl]<ulong, ulong, ulong, byte*, int> SetCanvasText;
        public delegate* unmanaged[Cdecl]<ulong, ulong, Vector3*, float, int> SetCharacterInputs;
        public delegate* unmanaged[Cdecl]<ulong, ulong, int> GetCharacterGrounded;
        public delegate* unmanaged[Cdecl]<int, int> IsMouseButtonPressed;
        public delegate* unmanaged[Cdecl]<float*, float*, int> GetMousePosition;
        public delegate* unmanaged[Cdecl]<int> IsInputFocused;
        public delegate* unmanaged[Cdecl]<int> IsPointerOverInput;
        public delegate* unmanaged[Cdecl]<float*, float*, int> GetMouseScrollDelta;
    }

    /// <summary>Matches the C++ ManagedMathApi function table.</summary>
    [StructLayout(LayoutKind.Sequential)]
    private struct NativeMathApi
    {
        public delegate* unmanaged[Cdecl]<VectorOperation, uint, float*, float*, float*, int> Vector;
        public delegate* unmanaged[Cdecl]<MatrixOperation, uint, float*, float*, float*, int> Matrix;
    }

    /// <summary>Matches C++ ManagedCompilationReport; strings are temporary UTF-8 buffers.</summary>
    [StructLayout(LayoutKind.Sequential)]
    private struct CompilationReport
    {
        public uint Size;
        public uint Status;

        public byte* Source;
        public byte* Output;
        public byte* Diagnostics;
        public byte* Runtime;

        public double Milliseconds;
        public uint Files;
        public uint Errors;
        public uint Warnings;
        public uint Reserved;
    }

    /// <summary>Matches the C++ ManagedCommand passed to Execute.</summary>
    [StructLayout(LayoutKind.Sequential)]
    private struct Command
    {
        public uint Size;
        public ManagedOperation Operation;

        public ulong Scene;
        public ulong Entity;
        public byte* Assembly;
        public byte* Class;

        public NativeApi* NativeApi;
        public float TimeStep;
        public uint Reserved;
        public uint* ClassId;
        public byte* Values;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct ScriptClassInfo
    {
        public uint Size;
        public uint ClassId;

        public byte* Name;
        public byte* Namespace;

        public uint FieldCount;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct ScriptFieldInfo
    {
        public uint Size;

        public uint ClassId;
        public uint FieldId;

        public ScriptValueType Type;

        public byte* Name;
        public byte* AssetKind;
        public byte* DefaultJson;
        public uint ReadOnly;
    }

    /// <summary>Owns one scene's managed objects and collectible assembly contexts.</summary>
    private sealed class Session
    {
        private readonly Dictionary<ulong, EntityScript> _instances = new();
        private readonly Dictionary<ulong, (string Path, string ClassName)> _definitions = new();

        private readonly Dictionary<Type, ManagedScriptClassMetadata> _scriptMetadata = new();

        private readonly Dictionary<string, (ScriptLoadContext Context, Assembly Assembly)> _assemblies =
            new(OperatingSystem.IsWindows() ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal);
        private readonly Dictionary<string, Lazy<string>> _sourceGroups =
            new(OperatingSystem.IsWindows() ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal);

        private Type GetScriptClass(string path, string className)
        {
            path = Path.GetFullPath(path);
            var assemblyPath = path;

            if (Path.GetExtension(path).Equals(".cs", StringComparison.OrdinalIgnoreCase))
            {
                var group = SourceCompiler.FindProject(path);
                if (!_sourceGroups.TryGetValue(group, out var compiled))
                {
                    // Cache failures too, so entities sharing a broken project do not rebuild it repeatedly.
                    compiled = new Lazy<string>(() => SourceCompiler.GetAssemblyPath(path));
                    _sourceGroups.Add(group, compiled);
                }

                assemblyPath = compiled.Value;
            }

            return SourceCompiler.FindScriptClass(GetAssembly(assemblyPath), path, className);
        }

        private ManagedScriptClassMetadata GetScriptMetadata(Type type)
        {
            if (_scriptMetadata.TryGetValue(type, out var metadata))
            {
                return metadata;
            }

            metadata = ScriptReflection.Reflect(type);

            RegisterScriptMetadata(metadata);
            _scriptMetadata.Add(type, metadata);

            return metadata;
        }

        private Assembly GetAssembly(string path)
        {
            path = Path.GetFullPath(path);
            if (_assemblies.TryGetValue(path, out var loaded))
            {
                return loaded.Assembly;
            }

            var context = new ScriptLoadContext(path);
            try
            {
                var assembly = context.LoadScript(path);
                _assemblies.Add(path, (context, assembly));
                return assembly;
            }
            catch
            {
                context.Unload();
                throw;
            }
        }

        internal uint Create(ulong scene, ulong entity, string path, string className, string values)
        {
            // Retain failed definitions so a later reload can retry after the source is fixed.
            _definitions[entity] = (path, className);
            var type = GetScriptClass(path, className);

            ValidateScriptClass(type);
            var metadata = GetScriptMetadata(type);
            DestroyInstance(entity);

            var script = (EntityScript)(Activator.CreateInstance(type) ??
                throw new InvalidOperationException($"Cannot construct {className}."));
            script.Bind(scene, entity);
            _instances.Add(entity, script);

            try
            {
                ScriptReflection.Apply(metadata, script, values, scene);
                script.OnCreate();
            }
            catch
            {
                DestroyInstance(entity);
                throw;
            }
            return metadata.ClassId;
        }

        internal uint Inspect(string path, string className)
        {
            var type = GetScriptClass(path, className);
            ValidateScriptClass(type);
            return GetScriptMetadata(type).ClassId;
        }

        internal void SetFields(ulong scene, ulong entity, string values)
        {
            var instance = _instances[entity];
            ScriptReflection.Apply(GetScriptMetadata(instance.GetType()), instance, values, scene);
        }

        internal void Update(ulong entity, float timeStep)
        {
            if (timeStep < 0)
            {
                throw new ArgumentOutOfRangeException(nameof(timeStep));
            }

            _instances[entity].OnUpdate(timeStep);
        }

        internal void Remove(ulong entity)
        {
            DestroyInstance(entity);
            _definitions.Remove(entity);
        }

        private void DestroyInstance(ulong entity)
        {
            if (!_instances.Remove(entity, out var script))
            {
                return;
            }

            try
            {
                script.OnDestroy();
            }
            catch (Exception error)
            {
                Log($"OnDestroy failed for {script.GetType().FullName} ({entity}): {error}", 2);
            }
            finally
            {
                script.Unbind();
            }
        }

        internal Session PrepareReload()
        {
            // Validate replacement assemblies first. A failed build leaves this session running.
            var replacement = new Session();
            try
            {
                foreach (var (id, definition) in _definitions)
                {
                    var type = replacement.GetScriptClass(definition.Path, definition.ClassName);
                    ValidateScriptClass(type);
                    replacement._definitions.Add(id, definition);
                }

                return replacement;
            }
            catch
            {
                replacement.Stop();
                throw;
            }
        }

        internal void Stop()
        {
            foreach (var id in _instances.Keys.ToArray())
            {
                DestroyInstance(id);
            }

            foreach (var loaded in _assemblies.Values)
            {
                loaded.Context.Unload();
            }

            // Unload is cooperative; user threads/event subscriptions must release their references.
            _assemblies.Clear();
            _sourceGroups.Clear();
            _definitions.Clear();
            foreach (var metadata in _scriptMetadata.Values) _api.UnregisterScriptClass(metadata.ClassId);
            _scriptMetadata.Clear();
        }
    }

    private static void ValidateScriptClass(Type type)
    {
        if (!typeof(EntityScript).IsAssignableFrom(type) || type.IsAbstract || type.ContainsGenericParameters ||
            type.GetConstructor(Type.EmptyTypes) == null)
        {
            throw new ArgumentException($"{type.FullName} must be a concrete EntityScript with a public parameterless constructor.");
        }
    }

    /// <summary>Receives a native command through the function pointer obtained from hostfxr.</summary>
    /// <param name="arguments">Pointer to a C++ ManagedCommand valid for the duration of this call.</param>
    /// <returns>Zero on success, or -1 for an invalid command or managed exception.</returns>
    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int Execute(void* arguments)
    {
        // Catch at the outermost native boundary. No managed exception may escape into C++.
        try
        {
            if (arguments == null || ((Command*)arguments)->Size != sizeof(Command))
            {
                return -1;
            }

            var command = *(Command*)arguments;
            if (command.Operation == ManagedOperation.Compile)
            {
                return CompileSource(command.Assembly);
            }

            lock (_gate)
            {
                if (command.Operation == ManagedOperation.Bind)
                {
                    return BindApi(command.NativeApi);
                }

                if (_api.Version != ApiVersion)
                {
                    return -1;
                }

                return ExecuteSceneCommand(command);
            }
        }
        catch (Exception error)
        {
            Log($"Script callback failed: {error}", 2);
            return -1;
        }
    }

    private static int BindApi(NativeApi* api)
    {
        if (api == null || api->Size != sizeof(NativeApi) || api->Version != ApiVersion ||
            api->GetTransform == null || api->SetTransform == null || api->IsKeyPressed == null ||
            api->RegisterScriptClass == null || api->RegisterScriptField == null || api->UnregisterScriptClass == null ||
            api->Log == null || api->Math == null || api->Math->Vector == null || api->Math->Matrix == null ||
            api->HasComponent == null || api->GetLightIntensity == null || api->SetLightIntensity == null ||
            api->FindCanvasElement == null || api->GetCanvasValue == null || api->SetCanvasValue == null ||
            api->FindEntity == null || api->SetVehicleInputs == null || api->GetVehicleSpeed == null || api->SetCanvasText == null ||
            api->SetCharacterInputs == null || api->GetCharacterGrounded == null ||
            api->IsMouseButtonPressed == null || api->GetMousePosition == null ||
            api->IsInputFocused == null || api->IsPointerOverInput == null || api->GetMouseScrollDelta == null ||
            sizeof(Vector2) != 8 || sizeof(Vector3) != 12 || sizeof(Vector4) != 16 ||
            sizeof(Matrix2) != 16 || sizeof(Matrix3) != 36 || sizeof(Matrix4) != 64)
        {
            return -1;
        }

        // Publish the process-lifetime callbacks once, under the same gate as scene dispatch.
        if (_api.Version == 0)
        {
            _api = *api;
        }

        return 0;
    }

    private static int CompileSource(byte* sourcePath)
    {
        lock (_gate)
        {
            if (_api.Version != ApiVersion)
            {
                return -1;
            }
        }

        // Compilation executes no user code; release the scene gate while building.
        var source = Marshal.PtrToStringUTF8((nint)sourcePath) ??
            throw new ArgumentException("Missing source path.");
        SourceCompiler.Compile(source);
        return 0;
    }

    private static int ExecuteSceneCommand(Command command)
    {
        if (command.Operation == ManagedOperation.StopScene)
        {
            if (_sessions.Remove(command.Scene, out var removed))
            {
                removed.Stop();
            }

            return 0;
        }

        if (!_sessions.TryGetValue(command.Scene, out var session))
        {
            if (command.Operation != ManagedOperation.Create && command.Operation != ManagedOperation.Inspect)
            {
                return command.Operation == ManagedOperation.Destroy ? 0 : -1;
            }

            session = new Session();
            _sessions.Add(command.Scene, session);
        }

        switch (command.Operation)
        {
            case ManagedOperation.Create:
            {
                var path = Marshal.PtrToStringUTF8((nint)command.Assembly) ??
                    throw new ArgumentException("Missing assembly path.");
                var className = Marshal.PtrToStringUTF8((nint)command.Class) ??
                    throw new ArgumentException("Missing class name.");
                var classId = session.Create(command.Scene, command.Entity, path, className,
                    Marshal.PtrToStringUTF8((nint)command.Values) ?? "{}");
                if (command.ClassId != null) *command.ClassId = classId;
                break;
            }

            case ManagedOperation.Inspect:
            {
                var classId = session.Inspect(Marshal.PtrToStringUTF8((nint)command.Assembly) ?? "",
                    Marshal.PtrToStringUTF8((nint)command.Class) ?? "");
                if (command.ClassId != null) *command.ClassId = classId;
                break;
            }
            case ManagedOperation.SetFields:
                session.SetFields(command.Scene, command.Entity, Marshal.PtrToStringUTF8((nint)command.Values) ?? "{}");
                break;

            case ManagedOperation.Update:
                session.Update(command.Entity, command.TimeStep);
                break;

            case ManagedOperation.Destroy:
                session.Remove(command.Entity);
                break;

            case ManagedOperation.ReloadScene:
            {
                var replacement = session.PrepareReload();
                session.Stop();
                _sessions[command.Scene] = replacement;
                // Native DotnetScript objects recreate themselves on their next Update.
                break;
            }

            default:
                return -1;
        }

        return 0;
    }

    private static void RegisterScriptMetadata(ManagedScriptClassMetadata metadata)
    {
        var nameBytes = Encoding.UTF8.GetBytes(metadata.Name + '\0');
        var namespaceBytes = Encoding.UTF8.GetBytes(metadata.Namespace + '\0');
        // Constructors supply defaults, but lifecycle callbacks are never run for editor inspection.
        var defaults = Activator.CreateInstance(metadata.Type)!;
        fixed (byte* name = nameBytes)
        fixed (byte* space = namespaceBytes)
        {
            var info = new ScriptClassInfo { Size = (uint)sizeof(ScriptClassInfo), ClassId = metadata.ClassId,
                Name = name, Namespace = space, FieldCount = (uint)metadata.Fields.Count };
            _api.RegisterScriptClass(&info);
        }
        try
        {
            foreach (var field in metadata.Fields.Values)
            {
                var fieldBytes = Encoding.UTF8.GetBytes(field.Name + '\0');
                var kindBytes = Encoding.UTF8.GetBytes(field.AssetKind + '\0');
                var defaultBytes = Encoding.UTF8.GetBytes(ScriptReflection.GetDefaultJson(field, defaults) + '\0');
                fixed (byte* name = fieldBytes)
                fixed (byte* kind = kindBytes)
                fixed (byte* value = defaultBytes)
                {
                    var info = new ScriptFieldInfo { Size = (uint)sizeof(ScriptFieldInfo), ClassId = metadata.ClassId,
                        FieldId = field.FieldId, Name = name, Type = field.Type, AssetKind = kind,
                        DefaultJson = value, ReadOnly = field.ReadOnly ? 1u : 0u };
                    _api.RegisterScriptField(&info);
                }
            }
        }
        catch { _api.UnregisterScriptClass(metadata.ClassId); throw; }
    }

    internal static void PlayAudio(ulong scene, ulong entity, string path)
    {
        var bytes = Encoding.UTF8.GetBytes(path + '\0');
        fixed (byte* text = bytes)
            if (_api.PlayAudio == null || _api.PlayAudio(scene, entity, text) == 0)
                throw new InvalidOperationException("Audio playback requires an AudioSourceComponent in the active scene.");
    }

    // Pass only packed scalar buffers to GLM. T is one of the public math value
    // types; its dimension and layout are fixed by the wrappers in MathTypes.cs.
    internal static T VectorMath<T>(VectorOperation operation, uint dimension, T left, T right) where T : unmanaged
    {
        T result = default;
        if (_api.Math == null || _api.Math->Vector(operation, dimension, (float*)&left, (float*)&right, (float*)&result) == 0)
        {
            throw new InvalidOperationException("GLM vector bindings are unavailable or the operation is invalid.");
        }

        return result;
    }

    internal static float VectorScalar<T>(VectorOperation operation, uint dimension, T left, T right) where T : unmanaged
    {
        T result = VectorMath(operation, dimension, left, right);
        return *(float*)&result;
    }

    internal static T MatrixMath<T>(MatrixOperation operation, uint dimension, T left, T right) where T : unmanaged
    {
        T result = default;
        if (_api.Math == null || _api.Math->Matrix(operation, dimension, (float*)&left, (float*)&right, (float*)&result) == 0)
        {
            throw new InvalidOperationException("GLM matrix bindings are unavailable or the operation is invalid.");
        }

        return result;
    }

    internal static bool MatrixEqual<T>(uint dimension, T left, T right) where T : unmanaged
    {
        T result = MatrixMath(MatrixOperation.Equal, dimension, left, right);
        return *(float*)&result != 0.0f;
    }

    internal static bool HasComponent(ulong scene, ulong entity, ComponentType type) =>
        _api.HasComponent != null && _api.HasComponent(scene, entity, type) != 0;

    internal static float GetLightIntensity(ulong scene, ulong entity)
    {
        float result = 0.0f;
        if (_api.GetLightIntensity == null || _api.GetLightIntensity(scene, entity, &result) == 0)
            throw new InvalidOperationException("Light access requires an active light in this scene's script callback.");
        return result;
    }

    internal static void SetLightIntensity(ulong scene, ulong entity, float value)
    {
        if (value < 0.0f)
            throw new ArgumentOutOfRangeException(nameof(value), "Light intensity must be non-negative.");
        if (_api.SetLightIntensity == null || _api.SetLightIntensity(scene, entity, value) == 0)
            throw new InvalidOperationException("Cannot change this light outside its scene's script callback.");
    }

    internal static ulong FindCanvasElement(ulong scene, ulong entity, string name)
    {
        ArgumentException.ThrowIfNullOrEmpty(name);
        if (name.Contains('\0'))
            throw new ArgumentException("Canvas element names cannot contain a null character.", nameof(name));
        var bytes = Encoding.UTF8.GetBytes(name + '\0');
        ulong result = 0;
        fixed (byte* text = bytes)
        {
            if (_api.FindCanvasElement == null || _api.FindCanvasElement(scene, entity, text, &result) == 0)
                throw new InvalidOperationException("Canvas access requires a CanvasComponent in this scene's script callback.");
        }
        if (result == 0)
            throw new KeyNotFoundException($"Canvas element '{name}' does not exist.");
        return result;
    }

    internal static float GetCanvasValue(ulong scene, ulong entity, ulong element)
    {
        float result = 0.0f;
        if (_api.GetCanvasValue == null || _api.GetCanvasValue(scene, entity, element, &result) == 0)
            throw new InvalidOperationException("Canvas element is missing or access is outside its scene's script callback.");
        return result;
    }

    internal static void SetCanvasValue(ulong scene, ulong entity, ulong element, float value)
    {
        if (_api.SetCanvasValue == null || _api.SetCanvasValue(scene, entity, element, value) == 0)
            throw new InvalidOperationException("Canvas element is missing or access is outside its scene's script callback.");
    }

    internal static ulong FindEntity(ulong scene, ulong owner, string name)
    {
        ArgumentException.ThrowIfNullOrEmpty(name);
        if (name.Contains('\0')) throw new ArgumentException("Entity names cannot contain a null character.", nameof(name));
        ulong result = 0;
        fixed (byte* text = Encoding.UTF8.GetBytes(name + '\0'))
        {
            if (_api.FindEntity == null || _api.FindEntity(scene, owner, text, &result) == 0)
                throw new InvalidOperationException("Entity lookup requires a unique name during its scene's script callback.");
        }
        return result;
    }

    internal static void SetVehicleInputs(ulong scene, ulong entity, float throttle, float steering, float brake, float handBrake)
    {
        if (_api.SetVehicleInputs == null || _api.SetVehicleInputs(scene, entity, throttle, steering, brake, handBrake) == 0)
            throw new InvalidOperationException("Vehicle access requires a VehicleComponent during its scene's script callback.");
    }

    internal static void SetCharacterInputs(ulong scene, ulong entity, Vector3 velocity, float jumpSpeed)
    {
        if (_api.SetCharacterInputs == null || _api.SetCharacterInputs(scene, entity, &velocity, jumpSpeed) == 0)
            throw new InvalidOperationException("Character movement requires a live character during its scene callback.");
    }

    internal static bool GetCharacterGrounded(ulong scene, ulong entity)
    {
        int state = _api.GetCharacterGrounded == null ? -1 : _api.GetCharacterGrounded(scene, entity);
        if (state < 0) throw new InvalidOperationException("Character state requires a live character during its scene callback.");
        return state != 0;
    }

    internal static float GetVehicleSpeed(ulong scene, ulong entity)
    {
        float result = 0;
        if (_api.GetVehicleSpeed == null || _api.GetVehicleSpeed(scene, entity, &result) == 0)
            throw new InvalidOperationException("Vehicle speed requires a vehicle and rigid body during its scene's script callback.");
        return result;
    }

    internal static void SetCanvasText(ulong scene, ulong entity, ulong element, string text)
    {
        ArgumentNullException.ThrowIfNull(text);
        if (text.Contains('\0')) throw new ArgumentException("Canvas text cannot contain a null character.", nameof(text));
        fixed (byte* value = Encoding.UTF8.GetBytes(text + '\0'))
        {
            if (_api.SetCanvasText == null || _api.SetCanvasText(scene, entity, element, value) == 0)
                throw new InvalidOperationException("Canvas text access requires a live control during its scene's script callback.");
        }
    }

    internal static Vector3 GetTransform(ulong scene, ulong entity, int kind)
    {
        Vector3 result = default;
        if (_api.GetTransform == null || _api.GetTransform(scene, entity, kind, &result) == 0)
        {
            throw new InvalidOperationException("Entity access requires a live entity during its scene's script callback.");
        }

        return result;
    }

    internal static void SetTransform(ulong scene, ulong entity, int kind, Vector3 value)
    {
        if (_api.SetTransform == null || _api.SetTransform(scene, entity, kind, &value) == 0)
        {
            throw new InvalidOperationException("Cannot change this entity transform outside a scene callback.");
        }
    }

    internal static bool IsKeyPressed(int key) => _api.IsKeyPressed != null && _api.IsKeyPressed(key) != 0;

    internal static bool IsMouseButtonPressed(int button)
        => _api.IsMouseButtonPressed != null && _api.IsMouseButtonPressed(button) != 0;

    internal static bool IsInputFocused => _api.IsInputFocused != null && _api.IsInputFocused() != 0;

    internal static bool IsPointerOverInput => _api.IsPointerOverInput != null && _api.IsPointerOverInput() != 0;

    internal static Vector2 GetMouseScrollDelta()
    {
        float x = 0.0f, y = 0.0f;
        if (_api.GetMouseScrollDelta == null || _api.GetMouseScrollDelta(&x, &y) == 0)
        {
            return default;
        }
        return new Vector2(x, y);
    }

    internal static Vector2 GetMousePosition()
    {
        float x = 0.0f, y = 0.0f;
        if (_api.GetMousePosition == null || _api.GetMousePosition(&x, &y) == 0)
            return default;
        return new Vector2 { X = x, Y = y };
    }

    internal static void ReportCompilation(uint status, string source, string output, string diagnostics,
        double milliseconds, uint files, uint errors, uint warnings, uint stage = 0)
    {
        if (_api.ReportCompilation == null)
        {
            return;
        }

        var sourceBytes = Encoding.UTF8.GetBytes(source + '\0');
        var outputBytes = Encoding.UTF8.GetBytes(output + '\0');
        var diagnosticBytes = Encoding.UTF8.GetBytes(diagnostics + '\0');
        var runtimeBytes = Encoding.UTF8.GetBytes($"{RuntimeInformation.FrameworkDescription}; MSBuild via .NET SDK; C# 14\0");
        fixed (byte* sourcePtr = sourceBytes)
        fixed (byte* outputPtr = outputBytes)
        fixed (byte* diagnosticPtr = diagnosticBytes)
        fixed (byte* runtimePtr = runtimeBytes)
        {
            var report = new CompilationReport
            {
                Size = (uint)sizeof(CompilationReport),
                Status = status,
                Source = sourcePtr,
                Output = outputPtr,
                Diagnostics = diagnosticPtr,
                Runtime = runtimePtr,
                Milliseconds = milliseconds,
                Files = files,
                Errors = errors,
                Warnings = warnings,
                Reserved = stage
            };
            _api.ReportCompilation(&report);
        }
    }

    internal static void Log(string message, int severity = 0)
    {
        try
        {
            if (_api.Log == null)
            {
                return;
            }

            var bytes = Encoding.UTF8.GetBytes(message + '\0');
            fixed (byte* text = bytes)
            {
                _api.Log(severity, text);
            }
        }
        catch
        {
            // Even exception reporting must not unwind across a native export.
        }
    }
}
