using System;
using System.Runtime.InteropServices;

namespace BunnyLoader.Source;

/// <summary>
/// Surface style with far/middle/close layers, transitions and custom close drawing.
/// Callbacks execute on the Mono worker; the native renderer owns the open SpriteBatch.
/// Slots belong to this session and must never be persisted in a world or character.
/// </summary>
public abstract class BunnySurfaceBackgroundStyle : BunnyHookModule
{
    public int Slot { get; private set; }=-1;
    public sealed override void Load(BunnyHookScope hooks)
    {
        Slot=BunnySurfaceBackgrounds.Reserve(ModId,GetType().FullName!);
        try { SetStaticDefaults();BunnySurfaceRegistry.Attach(this,hooks); }
        catch { BunnySurfaceBackgrounds.Retire(Slot);Slot=-1;throw; }
    }
    /// <summary>Resolve reusable texture slots here, after ModId and Slot are assigned.</summary>
    public virtual void SetStaticDefaults() { }
    /// <summary>Changes the full native front-layer fade table atomically. Keep Double arithmetic until the final Single store.</summary>
    public virtual void ModifyFarFades(Span<float> fades,double transitionSpeed)
    {
        if((uint)Slot>=(uint)fades.Length || !double.IsFinite(transitionSpeed))
            throw new ArgumentException("Expected the complete fade table and a finite transition speed");
        for(int i=0;i<fades.Length;i++) {
            double value=i==Slot?Math.Min(1,(double)fades[i]+transitionSpeed):Math.Max(0,(double)fades[i]-transitionSpeed);
            if(!double.IsFinite(value) || value>float.MaxValue || value< -float.MaxValue)
                throw new ArgumentException("Non-finite or unrepresentable fade at index "+i);
            fades[i]=(float)value;
        }
    }
    /// <summary>A background texture slot, or -1 to omit this layer.</summary>
    public virtual int ChooseFarTexture()=>-1;
    public virtual int ChooseMiddleTexture()=>-1;
    /// <summary>Queue sprites on the existing batch. Return false to replace the default close layer.</summary>
    public virtual bool PreDrawCloseBackground(in BunnySurfaceDrawContext context)=>true;
    /// <summary>Initial values: scale=1.25, parallax=.37, a=1800, b=1750. Native close scale is multiplied by two.</summary>
    public virtual int ChooseCloseTexture(ref double scale,ref double parallax,ref double a,ref double b)=>-1;
    public sealed override void Unload()
    {
        try { BunnySurfaceRegistry.Detach(this); }
        finally { if(Slot>=0)BunnySurfaceBackgrounds.Retire(Slot);Slot=-1; }
    }
    public override string DiagnosticStatus()=>"SurfaceBackground="+GetType().Name+" Slot="+Slot;
}

/// <summary>UID/class lookup and native scene/menu selection, shared with JavaScript-owned styles.</summary>
public static class BunnySurfaceBackgrounds
{
    [DllImport("bunny",EntryPoint="bunny_surface_reserve",CallingConvention=CallingConvention.Cdecl)]
    private static extern int NativeReserve([MarshalAs(UnmanagedType.LPUTF8Str)]string mod,[MarshalAs(UnmanagedType.LPUTF8Str)]string name);
    [DllImport("bunny",EntryPoint="bunny_surface_find",CallingConvention=CallingConvention.Cdecl)]
    private static extern int NativeFind([MarshalAs(UnmanagedType.LPUTF8Str)]string mod,[MarshalAs(UnmanagedType.LPUTF8Str)]string name);
    [DllImport("bunny",EntryPoint="bunny_surface_vanilla_count",CallingConvention=CallingConvention.Cdecl)]
    private static extern int NativeCount();
    [DllImport("bunny",EntryPoint="bunny_surface_select",CallingConvention=CallingConvention.Cdecl)]
    private static extern int NativeSelect(int slot,int priority,double weight,int active,int menu);
    [DllImport("bunny",EntryPoint="bunny_surface_retire",CallingConvention=CallingConvention.Cdecl)]
    internal static extern void Retire(int slot);
    [DllImport("bunny",EntryPoint="bunny_surface_last_error",CallingConvention=CallingConvention.Cdecl)]
    private static extern nint NativeError();
    public static int VanillaCount { get { int count=NativeCount();return count>=0?count:throw Failure("","","ReadVanillaCount"); } }
    public static BunnySurfaceBackgroundStyle? Get(int slot)=>BunnySurfaceRegistry.Get(slot);
    internal static int Reserve(string mod,string name)
    {Validate(mod,name);int slot=NativeReserve(mod,name);return slot>=0?slot:throw Failure(mod,name,"Reserve");}
    private static InvalidOperationException Failure(string mod,string name,string stage)=>new(
        "[CSharp][ERROR][SurfaceBackground] Mod="+mod+" Class="+name+" Stage="+stage+
        " Result=Rejected Detail="+(Marshal.PtrToStringUTF8(NativeError())??"NativeDiagnosticMissing"));
    public static int GetSlot<T>(string modId) where T:BunnySurfaceBackgroundStyle=>GetSlot(modId,typeof(T).FullName!);
    public static int GetSlot(string modId,string className)
    {Validate(modId,className);int slot=NativeFind(modId,className);return slot>=0?slot:throw Failure(modId,className,"ResolveStyle");}
    public static void Activate<T>(string modId,BunnyBackgroundPriority priority=BunnyBackgroundPriority.BiomeLow,double weight=.5)
        where T:BunnySurfaceBackgroundStyle=>Set(modId,typeof(T).FullName!,priority,weight,true);
    public static void Deactivate<T>(string modId) where T:BunnySurfaceBackgroundStyle=>Set(modId,typeof(T).FullName!,BunnyBackgroundPriority.None,0,false);
    /// <summary>menu=true selects the title background independently from scene activation and world resets.</summary>
    public static void Set(string modId,string className,BunnyBackgroundPriority priority,double weight,bool active,bool menu=false)
    {
        Validate(modId,className);
        if((uint)priority>8)throw new ArgumentOutOfRangeException(nameof(priority));
        if(!double.IsFinite(weight) || weight<0 || weight>1)throw new ArgumentOutOfRangeException(nameof(weight));
        int slot=GetSlot(modId,className);
        if(NativeSelect(slot,(int)priority,weight,active?1:0,menu?1:0)==0)throw Failure(modId,className,"Select");
    }
    private static void Validate(string mod,string name)
    {
        if(string.IsNullOrWhiteSpace(mod) || mod.Length>128 || string.IsNullOrWhiteSpace(name) || name.Length>240)
            throw new ArgumentException("Expected mod UID and fully qualified registered class name");
        foreach(char c in mod)if(c<32 || c==127 || c=='/' || c=='\\' || c=='|')throw new ArgumentException("Invalid mod identity");
        foreach(char c in name)if(c<32 || c==127 || c=='/' || c=='\\' || c=='|')throw new ArgumentException("Invalid class identity");
    }
}
