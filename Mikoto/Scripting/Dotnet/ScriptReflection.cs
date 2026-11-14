using System.Reflection;
using System.Text.Json;

namespace Mikoto.Hosting;

// Matches ScriptValueType in Scripting/Script.hh. Append values to preserve the ABI.
internal enum ScriptValueType : uint
{
    Unknown, Bool, Int32, UInt32, Float32, Float64, String, Vec2, Vec3, Vec4, Entity, Asset
}

internal sealed class ManagedScriptFieldMetadata
{
    public required uint FieldId { get; init; }
    public required string Name { get; init; }
    public required ScriptValueType Type { get; init; }
    public required Type ManagedType { get; init; }
    public required MemberInfo Member { get; init; }
    public bool ReadOnly { get; init; }
    public string AssetKind { get; init; } = string.Empty;

    public object? Read(object instance) => Member switch
    {
        FieldInfo field => field.GetValue(instance),
        PropertyInfo property => property.GetValue(instance),
        _ => null
    };

    public void Write(object instance, object? value)
    {
        if (ReadOnly) return;
        if (Member is FieldInfo field) field.SetValue(instance, value);
        else if (Member is PropertyInfo property) property.SetValue(instance, value);
    }
}

internal sealed class ManagedScriptClassMetadata
{
    public required uint ClassId { get; init; }
    public required string Name { get; init; }
    public required string Namespace { get; init; }
    public required Type Type { get; init; }
    public Dictionary<uint, ManagedScriptFieldMetadata> Fields { get; } = new();
}

internal static class ScriptReflection
{
    private static uint _nextClassId;
    private static readonly JsonSerializerOptions JsonOptions = new() { IncludeFields = true };

    internal static ManagedScriptClassMetadata Reflect(Type type)
    {
        var metadata = new ManagedScriptClassMetadata
        {
            ClassId = checked(++_nextClassId), Name = type.Name,
            Namespace = type.Namespace ?? string.Empty, Type = type
        };
        var names = new HashSet<string>(StringComparer.Ordinal);
        uint fieldId = 0;

        // DeclaredOnly plus an explicit walk includes private base fields without duplicating overrides.
        for (var current = type; current != null && current != typeof(EntityScript); current = current.BaseType)
        {
            foreach (var member in current.GetMembers(BindingFlags.Instance | BindingFlags.Public |
                         BindingFlags.NonPublic | BindingFlags.DeclaredOnly).OrderBy(member => member.Name, StringComparer.Ordinal))
            {
                if (member is not FieldInfo && member is not PropertyInfo ||
                    !Attribute.IsDefined(member, typeof(SerializeFieldAttribute), true) || !names.Add(member.Name)) continue;

                var memberType = member is FieldInfo field ? field.FieldType : ((PropertyInfo)member).PropertyType;
                if (member is PropertyInfo property && (property.GetIndexParameters().Length != 0 || property.GetMethod == null))
                    throw new InvalidOperationException($"Serialized property {type.FullName}.{member.Name} needs a getter and cannot be an indexer.");

                var valueType = GetValueType(memberType);
                if (valueType == ScriptValueType.Unknown)
                    throw new NotSupportedException($"Serialized member {type.FullName}.{member.Name} has unsupported type {memberType}.");

                var readOnly = member is FieldInfo f ? f.IsInitOnly || f.IsLiteral :
                    ((PropertyInfo)member).SetMethod == null ||
                    ((PropertyInfo)member).SetMethod!.ReturnParameter.GetRequiredCustomModifiers()
                        .Contains(typeof(System.Runtime.CompilerServices.IsExternalInit));
                metadata.Fields.Add(++fieldId, new ManagedScriptFieldMetadata
                {
                    FieldId = fieldId, Name = member.Name, Type = valueType, ManagedType = memberType,
                    Member = member, ReadOnly = readOnly,
                    AssetKind = memberType == typeof(AudioClip) ? "Audio" : memberType == typeof(Texture) ? "Texture" :
                        memberType == typeof(Material) ? "Material" : string.Empty
                });
            }
        }
        return metadata;
    }

    internal static string GetDefaultJson(ManagedScriptFieldMetadata field, object instance)
    {
        var value = field.Read(instance);
        return value switch
        {
            Asset asset => JsonSerializer.Serialize(asset.Path),
            Entity entity => JsonSerializer.Serialize(entity.Id),
            Vector2 v => JsonSerializer.Serialize(new[] { v.X, v.Y }),
            Vector3 v => JsonSerializer.Serialize(new[] { v.X, v.Y, v.Z }),
            Vector4 v => JsonSerializer.Serialize(new[] { v.X, v.Y, v.Z, v.W }),
            _ => JsonSerializer.Serialize(value, field.ManagedType, JsonOptions)
        };
    }

    internal static void Apply(ManagedScriptClassMetadata metadata, EntityScript instance, string json, ulong scene)
    {
        using var document = JsonDocument.Parse(json);
        if (document.RootElement.ValueKind != JsonValueKind.Object)
            throw new ArgumentException("Script properties must be an object.");
        foreach (var field in metadata.Fields.Values)
        {
            if (field.ReadOnly || !document.RootElement.TryGetProperty(field.Name, out var value)) continue;
            object? converted;
            if (field.Type == ScriptValueType.Asset)
                converted = value.ValueKind == JsonValueKind.Null ? null : Activator.CreateInstance(field.ManagedType, value.GetString()!);
            else if (field.Type == ScriptValueType.Entity)
                converted = value.ValueKind == JsonValueKind.Null || value.GetUInt64() == 0 ? null : new Entity(scene, value.GetUInt64());
            else if (field.Type == ScriptValueType.Vec2)
                converted = new Vector2 { X = value[0].GetSingle(), Y = value[1].GetSingle() };
            else if (field.Type == ScriptValueType.Vec3)
                converted = new Vector3 { X = value[0].GetSingle(), Y = value[1].GetSingle(), Z = value[2].GetSingle() };
            else if (field.Type == ScriptValueType.Vec4)
                converted = new Vector4 { X = value[0].GetSingle(), Y = value[1].GetSingle(), Z = value[2].GetSingle(), W = value[3].GetSingle() };
            else converted = value.Deserialize(field.ManagedType, JsonOptions);
            field.Write(instance, converted);
        }
    }

    private static ScriptValueType GetValueType(Type type)
    {
        if (type == typeof(bool)) return ScriptValueType.Bool;
        if (type == typeof(int)) return ScriptValueType.Int32;
        if (type == typeof(uint)) return ScriptValueType.UInt32;
        if (type == typeof(float)) return ScriptValueType.Float32;
        if (type == typeof(double)) return ScriptValueType.Float64;
        if (type == typeof(string)) return ScriptValueType.String;
        if (type == typeof(Vector2)) return ScriptValueType.Vec2;
        if (type == typeof(Vector3)) return ScriptValueType.Vec3;
        if (type == typeof(Vector4)) return ScriptValueType.Vec4;
        if (type == typeof(Entity)) return ScriptValueType.Entity;
        if (type == typeof(AudioClip) || type == typeof(Texture) || type == typeof(Material)) return ScriptValueType.Asset;
        return ScriptValueType.Unknown;
    }
}
