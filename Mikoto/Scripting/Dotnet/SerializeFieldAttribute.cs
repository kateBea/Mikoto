namespace Mikoto;

[AttributeUsage(
    AttributeTargets.Field | AttributeTargets.Property,
    AllowMultiple = false,
    Inherited = true
)]
public sealed class SerializeFieldAttribute : Attribute
{
}