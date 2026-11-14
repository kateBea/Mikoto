using System;
using System.Runtime.InteropServices;
using Mikoto.Hosting;

namespace Mikoto;

/// <summary>Operations sent to the native GLM vector bindings.</summary>
/// <remarks>
/// Matches mikoto::scripting::ManagedVectorOperation in Scripting/Dotnet/DotnetInterop.hh.
/// Keep the uint underlying type and numeric values identical on both sides of the ABI.
/// </remarks>
internal enum VectorOperation : uint
{
    Construct = 0,
    Add = 1,
    Subtract = 2,
    Multiply = 3,
    Divide = 4,
    Negate = 5,
    Equal = 6,
    Length = 7,
    Dot = 8,
    Normalize = 9,
    Cross = 10
}

/// <summary>Operations sent to the native GLM matrix bindings.</summary>
/// <remarks>
/// Matches mikoto::scripting::ManagedMatrixOperation in Scripting/Dotnet/DotnetInterop.hh.
/// Keep the uint underlying type and numeric values identical on both sides of the ABI.
/// </remarks>
internal enum MatrixOperation : uint
{
    Construct = 0,
    Multiply = 1,
    Equal = 2
}

/// <summary>GLM-backed 2D vector. Constructors and arithmetic require an initialized scripting host.</summary>
/// <remarks>Fields and default(Vector2) are plain value storage; operations execute in native GLM.</remarks>
[StructLayout(LayoutKind.Sequential)]
public struct Vector2 : IEquatable<Vector2>
{
    public float X;
    public float Y;

    public Vector2() : this(0.0f)
    {
    }

    public Vector2(float value) : this(value, value)
    {
    }

    public Vector2(float x, float y)
    {
        this = default;
        X = x;
        Y = y;
        this = NativeBridge.VectorMath(VectorOperation.Construct, 2, this, default);
    }

    public static Vector2 operator +(Vector2 a, Vector2 b) => NativeBridge.VectorMath(VectorOperation.Add, 2, a, b);

    public static Vector2 operator +(Vector2 a, float b) => a + new Vector2(b);

    public static Vector2 operator +(float a, Vector2 b) => new Vector2(a) + b;

    public static Vector2 operator -(Vector2 a, Vector2 b) => NativeBridge.VectorMath(VectorOperation.Subtract, 2, a, b);

    public static Vector2 operator -(Vector2 a, float b) => a - new Vector2(b);

    public static Vector2 operator -(float a, Vector2 b) => new Vector2(a) - b;

    public static Vector2 operator *(Vector2 a, Vector2 b) => NativeBridge.VectorMath(VectorOperation.Multiply, 2, a, b);

    public static Vector2 operator *(Vector2 a, float b) => a * new Vector2(b);

    public static Vector2 operator *(float a, Vector2 b) => new Vector2(a) * b;

    public static Vector2 operator /(Vector2 a, float b) => NativeBridge.VectorMath(VectorOperation.Divide, 2, a, new Vector2(b));

    public static Vector2 operator -(Vector2 value) => NativeBridge.VectorMath(VectorOperation.Negate, 2, value, default);

    public static bool operator ==(Vector2 a, Vector2 b) => NativeBridge.VectorScalar(VectorOperation.Equal, 2, a, b) != 0.0f;

    public static bool operator !=(Vector2 a, Vector2 b) => !(a == b);

    public readonly bool Equals(Vector2 other) => this == other;

    public override readonly bool Equals(object? value) => value is Vector2 other && this == other;

    public override readonly int GetHashCode() => HashCode.Combine(X, Y);

    public readonly float Length() => NativeBridge.VectorScalar(VectorOperation.Length, 2, this, default);

    public static float Dot(Vector2 a, Vector2 b) => NativeBridge.VectorScalar(VectorOperation.Dot, 2, a, b);

    /// <summary>Uses glm::normalize; the input must have nonzero length.</summary>
    public readonly Vector2 Normalized() => NativeBridge.VectorMath(VectorOperation.Normalize, 2, this, default);

    public override readonly string ToString() => $"Vector2({X}, {Y})";
}

/// <summary>GLM-backed 3D vector. Constructors and arithmetic require an initialized scripting host.</summary>
/// <remarks>Fields and default(Vector3) are plain value storage; operations execute in native GLM.</remarks>
[StructLayout(LayoutKind.Sequential)]
public struct Vector3 : IEquatable<Vector3>
{
    public float X;
    public float Y;
    public float Z;

    public Vector3() : this(0.0f)
    {
    }

    public Vector3(float value) : this(value, value, value)
    {
    }

    public Vector3(float x, float y, float z)
    {
        this = default;
        X = x;
        Y = y;
        Z = z;
        this = NativeBridge.VectorMath(VectorOperation.Construct, 3, this, default);
    }

    public static Vector3 operator +(Vector3 a, Vector3 b) => NativeBridge.VectorMath(VectorOperation.Add, 3, a, b);

    public static Vector3 operator +(Vector3 a, float b) => a + new Vector3(b);

    public static Vector3 operator +(float a, Vector3 b) => new Vector3(a) + b;

    public static Vector3 operator -(Vector3 a, Vector3 b) => NativeBridge.VectorMath(VectorOperation.Subtract, 3, a, b);

    public static Vector3 operator -(Vector3 a, float b) => a - new Vector3(b);

    public static Vector3 operator -(float a, Vector3 b) => new Vector3(a) - b;

    public static Vector3 operator *(Vector3 a, Vector3 b) => NativeBridge.VectorMath(VectorOperation.Multiply, 3, a, b);

    public static Vector3 operator *(Vector3 a, float b) => a * new Vector3(b);

    public static Vector3 operator *(float a, Vector3 b) => new Vector3(a) * b;

    public static Vector3 operator /(Vector3 a, float b) => NativeBridge.VectorMath(VectorOperation.Divide, 3, a, new Vector3(b));

    public static Vector3 operator -(Vector3 value) => NativeBridge.VectorMath(VectorOperation.Negate, 3, value, default);

    public static bool operator ==(Vector3 a, Vector3 b) => NativeBridge.VectorScalar(VectorOperation.Equal, 3, a, b) != 0.0f;

    public static bool operator !=(Vector3 a, Vector3 b) => !(a == b);

    public readonly bool Equals(Vector3 other) => this == other;

    public override readonly bool Equals(object? value) => value is Vector3 other && this == other;

    public override readonly int GetHashCode() => HashCode.Combine(X, Y, Z);

    public readonly float Length() => NativeBridge.VectorScalar(VectorOperation.Length, 3, this, default);

    public static float Dot(Vector3 a, Vector3 b) => NativeBridge.VectorScalar(VectorOperation.Dot, 3, a, b);

    /// <summary>Uses glm::normalize; the input must have nonzero length.</summary>
    public readonly Vector3 Normalized() => NativeBridge.VectorMath(VectorOperation.Normalize, 3, this, default);

    public static Vector3 Cross(Vector3 a, Vector3 b) => NativeBridge.VectorMath(VectorOperation.Cross, 3, a, b);

    public override readonly string ToString() => $"Vector3({X}, {Y}, {Z})";
}

/// <summary>GLM-backed 4D vector. Constructors and arithmetic require an initialized scripting host.</summary>
/// <remarks>Fields and default(Vector4) are plain value storage; operations execute in native GLM.</remarks>
[StructLayout(LayoutKind.Sequential)]
public struct Vector4 : IEquatable<Vector4>
{
    public float X;
    public float Y;
    public float Z;
    public float W;

    public Vector4() : this(0.0f)
    {
    }

    public Vector4(float value) : this(value, value, value, value)
    {
    }

    public Vector4(float x, float y, float z, float w)
    {
        this = default;
        X = x;
        Y = y;
        Z = z;
        W = w;
        this = NativeBridge.VectorMath(VectorOperation.Construct, 4, this, default);
    }

    public static Vector4 operator +(Vector4 a, Vector4 b) => NativeBridge.VectorMath(VectorOperation.Add, 4, a, b);

    public static Vector4 operator +(Vector4 a, float b) => a + new Vector4(b);

    public static Vector4 operator +(float a, Vector4 b) => new Vector4(a) + b;

    public static Vector4 operator -(Vector4 a, Vector4 b) => NativeBridge.VectorMath(VectorOperation.Subtract, 4, a, b);

    public static Vector4 operator -(Vector4 a, float b) => a - new Vector4(b);

    public static Vector4 operator -(float a, Vector4 b) => new Vector4(a) - b;

    public static Vector4 operator *(Vector4 a, Vector4 b) => NativeBridge.VectorMath(VectorOperation.Multiply, 4, a, b);

    public static Vector4 operator *(Vector4 a, float b) => a * new Vector4(b);

    public static Vector4 operator *(float a, Vector4 b) => new Vector4(a) * b;

    public static Vector4 operator /(Vector4 a, float b) => NativeBridge.VectorMath(VectorOperation.Divide, 4, a, new Vector4(b));

    public static Vector4 operator -(Vector4 value) => NativeBridge.VectorMath(VectorOperation.Negate, 4, value, default);

    public static bool operator ==(Vector4 a, Vector4 b) => NativeBridge.VectorScalar(VectorOperation.Equal, 4, a, b) != 0.0f;

    public static bool operator !=(Vector4 a, Vector4 b) => !(a == b);

    public readonly bool Equals(Vector4 other) => this == other;

    public override readonly bool Equals(object? value) => value is Vector4 other && this == other;

    public override readonly int GetHashCode() => HashCode.Combine(X, Y, Z, W);

    public readonly float Length() => NativeBridge.VectorScalar(VectorOperation.Length, 4, this, default);

    public static float Dot(Vector4 a, Vector4 b) => NativeBridge.VectorScalar(VectorOperation.Dot, 4, a, b);

    /// <summary>Uses glm::normalize; the input must have nonzero length.</summary>
    public readonly Vector4 Normalized() => NativeBridge.VectorMath(VectorOperation.Normalize, 4, this, default);

    public override readonly string ToString() => $"Vector4({X}, {Y}, {Z}, {W})";
}

/// <summary>GLM-backed 2x2 matrix stored as columns, matching GLM's column-major convention.</summary>
/// <remarks>new Matrix2() is identity; default(Matrix2) is zero-filled storage.</remarks>
[StructLayout(LayoutKind.Sequential)]
public struct Matrix2 : IEquatable<Matrix2>
{
    public Vector2 Column0;
    public Vector2 Column1;

    public Matrix2() : this(1.0f)
    {
    }

    public Matrix2(float diagonal)
    {
        this = default;
        Column0.X = diagonal;
        Column1.Y = diagonal;
        this = NativeBridge.MatrixMath(MatrixOperation.Construct, 2, this, default);
    }

    public Matrix2(Vector2 column0, Vector2 column1)
    {
        this = default;
        Column0 = column0;
        Column1 = column1;
        this = NativeBridge.MatrixMath(MatrixOperation.Construct, 2, this, default);
    }

    public static Matrix2 operator *(Matrix2 a, Matrix2 b) => NativeBridge.MatrixMath(MatrixOperation.Multiply, 2, a, b);

    public static bool operator ==(Matrix2 a, Matrix2 b) => NativeBridge.MatrixEqual(2, a, b);

    public static bool operator !=(Matrix2 a, Matrix2 b) => !(a == b);

    public readonly bool Equals(Matrix2 other) => this == other;

    public override readonly bool Equals(object? value) => value is Matrix2 other && this == other;

    public override readonly int GetHashCode() => HashCode.Combine(Column0, Column1);
}

/// <summary>GLM-backed 3x3 matrix stored as columns, matching GLM's column-major convention.</summary>
/// <remarks>new Matrix3() is identity; default(Matrix3) is zero-filled storage.</remarks>
[StructLayout(LayoutKind.Sequential)]
public struct Matrix3 : IEquatable<Matrix3>
{
    public Vector3 Column0;
    public Vector3 Column1;
    public Vector3 Column2;

    public Matrix3() : this(1.0f)
    {
    }

    public Matrix3(float diagonal)
    {
        this = default;
        Column0.X = diagonal;
        Column1.Y = diagonal;
        Column2.Z = diagonal;
        this = NativeBridge.MatrixMath(MatrixOperation.Construct, 3, this, default);
    }

    public Matrix3(Vector3 column0, Vector3 column1, Vector3 column2)
    {
        this = default;
        Column0 = column0;
        Column1 = column1;
        Column2 = column2;
        this = NativeBridge.MatrixMath(MatrixOperation.Construct, 3, this, default);
    }

    public static Matrix3 operator *(Matrix3 a, Matrix3 b) => NativeBridge.MatrixMath(MatrixOperation.Multiply, 3, a, b);

    public static bool operator ==(Matrix3 a, Matrix3 b) => NativeBridge.MatrixEqual(3, a, b);

    public static bool operator !=(Matrix3 a, Matrix3 b) => !(a == b);

    public readonly bool Equals(Matrix3 other) => this == other;

    public override readonly bool Equals(object? value) => value is Matrix3 other && this == other;

    public override readonly int GetHashCode() => HashCode.Combine(Column0, Column1, Column2);
}

/// <summary>GLM-backed 4x4 matrix stored as columns, matching GLM's column-major convention.</summary>
/// <remarks>new Matrix4() is identity; default(Matrix4) is zero-filled storage.</remarks>
[StructLayout(LayoutKind.Sequential)]
public struct Matrix4 : IEquatable<Matrix4>
{
    public Vector4 Column0;
    public Vector4 Column1;
    public Vector4 Column2;
    public Vector4 Column3;

    public Matrix4() : this(1.0f)
    {
    }

    public Matrix4(float diagonal)
    {
        this = default;
        Column0.X = diagonal;
        Column1.Y = diagonal;
        Column2.Z = diagonal;
        Column3.W = diagonal;
        this = NativeBridge.MatrixMath(MatrixOperation.Construct, 4, this, default);
    }

    public Matrix4(Vector4 column0, Vector4 column1, Vector4 column2, Vector4 column3)
    {
        this = default;
        Column0 = column0;
        Column1 = column1;
        Column2 = column2;
        Column3 = column3;
        this = NativeBridge.MatrixMath(MatrixOperation.Construct, 4, this, default);
    }

    public static Matrix4 operator *(Matrix4 a, Matrix4 b) => NativeBridge.MatrixMath(MatrixOperation.Multiply, 4, a, b);

    public static bool operator ==(Matrix4 a, Matrix4 b) => NativeBridge.MatrixEqual(4, a, b);

    public static bool operator !=(Matrix4 a, Matrix4 b) => !(a == b);

    public readonly bool Equals(Matrix4 other) => this == other;

    public override readonly bool Equals(object? value) => value is Matrix4 other && this == other;

    public override readonly int GetHashCode() => HashCode.Combine(Column0, Column1, Column2, Column3);
}
