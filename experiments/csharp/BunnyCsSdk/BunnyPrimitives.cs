using System;
using System.Runtime.InteropServices;

namespace BunnyLoader.Source;

[StructLayout(LayoutKind.Sequential)]
public readonly struct BunnyVector2
{
    public readonly float X, Y;
    public BunnyVector2(float x, float y) { X=x; Y=y; }
    public float LengthSquared => X*X+Y*Y;
    public static BunnyVector2 operator +(BunnyVector2 a,BunnyVector2 b) => new(a.X+b.X,a.Y+b.Y);
    public static BunnyVector2 operator -(BunnyVector2 a,BunnyVector2 b) => new(a.X-b.X,a.Y-b.Y);
    public static BunnyVector2 operator *(BunnyVector2 a,float scale) => new(a.X*scale,a.Y*scale);
}

[StructLayout(LayoutKind.Sequential)]
public readonly struct BunnyRectangle
{
    public readonly int X,Y,Width,Height;
    public BunnyRectangle(int x,int y,int width,int height) { X=x;Y=y;Width=width;Height=height; }
    public bool Contains(int x,int y) => Width>0&&Height>0&&x>=X&&y>=Y&&(long)x<(long)X+Width&&(long)y<(long)Y+Height;
}

[StructLayout(LayoutKind.Sequential)]
public readonly struct BunnyColor
{
    public readonly byte R,G,B,A;
    public BunnyColor(byte red,byte green,byte blue,byte alpha=255) { R=red;G=green;B=blue;A=alpha; }
    public uint PackedRgba => (uint)R|((uint)G<<8)|((uint)B<<16)|((uint)A<<24);
    public static BunnyColor FromPackedRgba(uint value) => new((byte)value,(byte)(value>>8),(byte)(value>>16),(byte)(value>>24));
}

public enum BunnyValueKind { Void,Boolean,Int32,UInt32,Int64,Single,Double,Vector2,Rectangle,Color,UInt64 }

[StructLayout(LayoutKind.Sequential)]
public readonly struct BunnyValue
{
    public readonly BunnyValueKind Kind;
    private readonly int reserved;
    private readonly ulong a,b;
    private BunnyValue(BunnyValueKind kind,ulong first,ulong second=0) { Kind=kind;reserved=0;a=first;b=second; }
    public static BunnyValue FromBoolean(bool value) => new(BunnyValueKind.Boolean,value?1UL:0UL);
    public static BunnyValue FromInt32(int value) => new(BunnyValueKind.Int32,unchecked((uint)value));
    public static BunnyValue FromUInt32(uint value) => new(BunnyValueKind.UInt32,value);
    public static BunnyValue FromInt64(long value) => new(BunnyValueKind.Int64,unchecked((ulong)value));
    public static BunnyValue FromUInt64(ulong value) => new(BunnyValueKind.UInt64,value);
    public static BunnyValue FromSingle(float value) => new(BunnyValueKind.Single,unchecked((uint)BitConverter.SingleToInt32Bits(value)));
    public static BunnyValue FromDouble(double value) => new(BunnyValueKind.Double,unchecked((ulong)BitConverter.DoubleToInt64Bits(value)));
    public static BunnyValue FromVector2(BunnyVector2 value) => new(BunnyValueKind.Vector2,
        unchecked((uint)BitConverter.SingleToInt32Bits(value.X))|((ulong)unchecked((uint)BitConverter.SingleToInt32Bits(value.Y))<<32));
    public static BunnyValue FromRectangle(BunnyRectangle value) => new(BunnyValueKind.Rectangle,
        unchecked((uint)value.X)|((ulong)unchecked((uint)value.Y)<<32),unchecked((uint)value.Width)|((ulong)unchecked((uint)value.Height)<<32));
    public static BunnyValue FromColor(BunnyColor value) => new(BunnyValueKind.Color,value.PackedRgba);
    private void Require(BunnyValueKind kind) { if(Kind!=kind)throw new InvalidOperationException("Tipo de valor esperado: "+kind+"; recebido: "+Kind); }
    public bool AsBoolean() { Require(BunnyValueKind.Boolean);return a!=0; }
    public int AsInt32() { Require(BunnyValueKind.Int32);return unchecked((int)a); }
    public uint AsUInt32() { Require(BunnyValueKind.UInt32);return (uint)a; }
    public long AsInt64() { Require(BunnyValueKind.Int64);return unchecked((long)a); }
    public ulong AsUInt64() { Require(BunnyValueKind.UInt64);return a; }
    public float AsSingle() { Require(BunnyValueKind.Single);return BitConverter.Int32BitsToSingle(unchecked((int)a)); }
    public double AsDouble() { Require(BunnyValueKind.Double);return BitConverter.Int64BitsToDouble(unchecked((long)a)); }
    public BunnyVector2 AsVector2() { Require(BunnyValueKind.Vector2);return new(BitConverter.Int32BitsToSingle(unchecked((int)a)),BitConverter.Int32BitsToSingle(unchecked((int)(a>>32)))); }
    public BunnyRectangle AsRectangle() { Require(BunnyValueKind.Rectangle);return new(unchecked((int)a),unchecked((int)(a>>32)),unchecked((int)b),unchecked((int)(b>>32))); }
    public BunnyColor AsColor() { Require(BunnyValueKind.Color);return BunnyColor.FromPackedRgba((uint)a); }
}
