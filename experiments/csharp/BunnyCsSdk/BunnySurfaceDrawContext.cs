using System;
using System.Runtime.InteropServices;

namespace BunnyLoader.Source;

/// <summary>
/// Immutable snapshot valid only inside PreDrawCloseBackground. A ref struct cannot
/// escape into a field/task. Draw queues at most 128 native sprites transactionally;
/// no IL2CPP pointers, Begin/End, or JavaScript VM calls cross this API.
/// </summary>
public readonly ref struct BunnySurfaceDrawContext
{
    private readonly long session;
    private readonly int slot;
    private readonly ReadOnlySpan<double> values;
    internal BunnySurfaceDrawContext(long session,int slot,ReadOnlySpan<double> values,uint rgba)
    {this.session=session;this.slot=slot;this.values=values;BaseColor=BunnyColor.FromPackedRgba(rgba);}
    public double Alpha=>values[0];
    public double ScreenX=>values[1];
    public double ScreenY=>values[2];
    public double WorldSurface=>values[3];
    public int ScreenWidth=>(int)values[4];
    public double BackgroundTop=>values[5];
    public double VerticalAdjustment=>values[6];
    public int PushTop=>(int)values[7];
    public BunnyColor BaseColor { get; }
    /// <summary>Original background color multiplied by the current layer fade, including alpha.</summary>
    public BunnyColor FadedColor=>new(Fade(BaseColor.R),Fade(BaseColor.G),Fade(BaseColor.B),Fade(BaseColor.A));
    private byte Fade(byte value)=>(byte)Math.Clamp(value*Alpha,0,255);
    [DllImport("bunny",EntryPoint="bunny_surface_draw",CallingConvention=CallingConvention.Cdecl)]
    private static extern unsafe int NativeDraw(long session,int slot,int texture,double* values,int length,uint rgba);
    [DllImport("bunny",EntryPoint="bunny_surface_last_error",CallingConvention=CallingConvention.Cdecl)]
    private static extern nint NativeError();
    /// <summary>Draw one sprite on the engine's open batch. Source=null uses the complete texture; effects uses horizontal=1/vertical=2.</summary>
    public unsafe void Draw(int textureSlot,BunnyVector2 position,BunnyColor color,BunnyRectangle? source=null,
        float rotation=0,BunnyVector2 origin=default,float scale=1,int effects=0,float layerDepth=0)
    {
        if(textureSlot<0)throw new ArgumentOutOfRangeException(nameof(textureSlot));
        if(!float.IsFinite(position.X) || !float.IsFinite(position.Y) || !float.IsFinite(rotation) ||
            !float.IsFinite(origin.X) || !float.IsFinite(origin.Y) || !float.IsFinite(scale) || scale<=0 ||
            (uint)effects>3 || !float.IsFinite(layerDepth) || layerDepth<0 || layerDepth>1)
            throw new ArgumentException("Expected finite sprite coordinates/scale, effects 0..3 and depth 0..1");
        if(source is BunnyRectangle rect && (rect.X<0 || rect.Y<0 || rect.Width<=0 || rect.Height<=0))
            throw new ArgumentException("Expected a positive source rectangle inside the texture",nameof(source));
        double* command=stackalloc double[13]{position.X,position.Y,source?.X??0,source?.Y??0,source?.Width??0,source?.Height??0,
            source.HasValue?1:0,rotation,origin.X,origin.Y,scale,effects,layerDepth};
        if(NativeDraw(session,slot,textureSlot,command,13,color.PackedRgba)==0)
            throw new InvalidOperationException("[CSharp][ERROR][SurfaceBackground] ContentId="+slot+
                " Method=PreDrawCloseBackground Stage=QueueDraw Result=Rejected Detail="+
                (Marshal.PtrToStringUTF8(NativeError())??"NativeDiagnosticMissing"));
    }
}
