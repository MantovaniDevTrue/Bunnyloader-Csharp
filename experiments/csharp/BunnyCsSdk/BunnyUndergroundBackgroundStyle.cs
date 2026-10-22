using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Threading;

namespace BunnyLoader.Source;

/// <summary>Priority used when replacing Terraria's underground background.</summary>
public enum BunnyBackgroundPriority
{
    None=0, BiomeLow=1, BiomeMedium=2, BiomeHigh=3,
    Environment=4, Event=5, BossLow=6, BossMedium=7, BossHigh=8
}

/// <summary>
/// Five-layer underground style. Fill runs once per draw for each style needed
/// by the current/previous transition. Managed code always runs on the Mono worker;
/// render threads exchange only bounded slot messages and native texture references.
/// Resolve assets through BunnyBackgroundTextures; numeric slots are session-local.
/// </summary>
public abstract class BunnyUndergroundBackgroundStyle : BunnyHookModule
{
    public int Slot { get; private set; }=-1;
    public sealed override void Load(BunnyHookScope hooks)
    {
        Slot=BunnyUndergroundBackgrounds.Reserve(ModId,GetType().FullName!);
        try { SetStaticDefaults();BunnyUndergroundRegistry.Attach(this,hooks); }
        catch { BunnyUndergroundBackgrounds.Retire(Slot);Slot=-1;throw; }
    }
    /// <summary>Initialize reusable managed state after Slot and ModId are assigned.</summary>
    public virtual void SetStaticDefaults() { }
    /// <summary>Slots 0..3 are borders/dirt/rock; slot 4 defaults to vanilla hell transition 6. -1 preserves the game's layer.</summary>
    public abstract void FillTextureArray(Span<int> textureSlots);
    public sealed override void Unload()
    {
        BunnyUndergroundRegistry.Detach(this);
        if(Slot>=0)BunnyUndergroundBackgrounds.Retire(Slot);
        Slot=-1;
    }
    public override string DiagnosticStatus()=>"UndergroundBackground="+GetType().Name+" Slot="+Slot;
}

/// <summary>
/// Scene controllers can activate any registered style by mod UID/class. The
/// native loader resolves priority and weight without invoking a JavaScript API.
/// Activation ends at a world boundary; no numeric ID should be saved.
/// </summary>
public static class BunnyUndergroundBackgrounds
{
    public static int VanillaCount=>22;
    /// <summary>Returns a registered C# style; JavaScript-owned styles are referenced by UID/name/slot.</summary>
    public static BunnyUndergroundBackgroundStyle? Get(int slot)=>BunnyUndergroundRegistry.Get(slot);
    [DllImport("bunny",EntryPoint="bunny_underground_reserve",CallingConvention=CallingConvention.Cdecl)]
    private static extern int NativeReserve([MarshalAs(UnmanagedType.LPUTF8Str)]string mod,[MarshalAs(UnmanagedType.LPUTF8Str)]string name);
    [DllImport("bunny",EntryPoint="bunny_underground_find",CallingConvention=CallingConvention.Cdecl)]
    private static extern int NativeFind([MarshalAs(UnmanagedType.LPUTF8Str)]string mod,[MarshalAs(UnmanagedType.LPUTF8Str)]string name);
    [DllImport("bunny",EntryPoint="bunny_underground_select",CallingConvention=CallingConvention.Cdecl)]
    private static extern int NativeSelect(int slot,int priority,double weight,int active);
    [DllImport("bunny",EntryPoint="bunny_underground_retire",CallingConvention=CallingConvention.Cdecl)]
    internal static extern void Retire(int slot);
    [DllImport("bunny",EntryPoint="bunny_underground_last_error",CallingConvention=CallingConvention.Cdecl)]
    private static extern nint NativeError();
    internal static int Reserve(string mod,string name)
    {
        Validate(mod,name);int slot=NativeReserve(mod,name);
        if(slot<0)throw Failure(mod,name,"Reserve");return slot;
    }
    private static InvalidOperationException Failure(string mod,string name,string stage)=>new(
        "[CSharp][ERROR][UndergroundBackground] Mod="+mod+" Class="+name+" Stage="+stage+
        " Result=Rejected Detail="+(Marshal.PtrToStringUTF8(NativeError())??"NativeDiagnosticMissing"));
    public static int GetSlot<T>(string modId) where T:BunnyUndergroundBackgroundStyle=>GetSlot(modId,typeof(T).FullName!);
    public static int GetSlot(string modId,string className)
    {
        Validate(modId,className);int slot=NativeFind(modId,className);
        if(slot<0)throw Failure(modId,className,"ResolveStyle");return slot;
    }
    public static void Activate<T>(string modId,BunnyBackgroundPriority priority=BunnyBackgroundPriority.BiomeLow,double weight=.5)
        where T:BunnyUndergroundBackgroundStyle=>Set(modId,typeof(T).FullName!,priority,weight,true);
    public static void Deactivate<T>(string modId) where T:BunnyUndergroundBackgroundStyle=>Set(modId,typeof(T).FullName!,BunnyBackgroundPriority.None,0,false);
    public static void Set(string modId,string className,BunnyBackgroundPriority priority,double weight,bool active)
    {
        Validate(modId,className);
        if((uint)priority>8)throw new ArgumentOutOfRangeException(nameof(priority));
        if(!double.IsFinite(weight) || weight<0 || weight>1)throw new ArgumentOutOfRangeException(nameof(weight));
        int slot=GetSlot(modId,className);
        if(NativeSelect(slot,(int)priority,weight,active?1:0)==0)throw Failure(modId,className,"Select");
    }
    private static void Validate(string mod,string name)
    {
        if(string.IsNullOrWhiteSpace(mod) || mod.Length>128 || string.IsNullOrWhiteSpace(name) || name.Length>240)
            throw new ArgumentException("Expected mod UID and fully qualified registered class name");
        foreach(string text in new[]{mod,name})foreach(char c in text)
            if(c<32 || c==127 || c=='/' || c=='\\' || c=='|')throw new ArgumentException("Invalid content identity");
    }
}

internal static class BunnyUndergroundRegistry
{
    private sealed record Entry(BunnyUndergroundBackgroundStyle Style,BunnyHookScope Scope) { public int Failures; }
    private static readonly object Gate=new();
    private static readonly List<Entry> entries=new();
    private static Entry[] snapshot=Array.Empty<Entry>();
    private static BunnyHookScope? owner;
    private static BunnyMethodHook? hook;
    [DllImport("bunny",EntryPoint="bunny_underground_fill_read",CallingConvention=CallingConvention.Cdecl)]
    private static extern int Read(out long session,out int slot);
    [DllImport("bunny",EntryPoint="bunny_underground_fill_write",CallingConvention=CallingConvention.Cdecl)]
    private static extern int Write(long session,int slot,int a,int b,int c,int d,int e);
    internal static BunnyUndergroundBackgroundStyle? Get(int slot)
    {foreach(Entry entry in Volatile.Read(ref snapshot))if(entry.Style.Slot==slot)return entry.Style;return null;}
    internal static void Attach(BunnyUndergroundBackgroundStyle style,BunnyHookScope scope)
    {
        lock(Gate) {
            entries.Add(new(style,scope));
            try { Rebuild(); }catch { entries.RemoveAt(entries.Count-1);Rebuild();throw; }
        }
    }
    internal static void Detach(BunnyUndergroundBackgroundStyle style)
    {
        lock(Gate){entries.RemoveAll(e=>ReferenceEquals(e.Style,style));Rebuild();}
    }
    internal static void DetachScope(BunnyHookScope scope)
    {
        lock(Gate){entries.RemoveAll(e=>ReferenceEquals(e.Scope,scope));Rebuild();}
    }
    private static void Rebuild()
    {
        BunnyHookScope? next=entries.Count==0?null:entries[0].Scope;
        if(!ReferenceEquals(owner,next) || hook is null || hook.IsDisposed) {
            hook?.Dispose();hook=null;owner=null;
            if(next is not null){hook=next.HookSystemStage("UndergroundBackground",Dispatch);owner=next;}
        }
        Volatile.Write(ref snapshot,entries.ToArray());
    }
    private static void Dispatch(BunnyOriginalVoid original,nint self)
    {
        if(Read(out long session,out int slot)!=0)foreach(Entry entry in Volatile.Read(ref snapshot)) {
            if(entry.Style.Slot!=slot)continue;
            Span<int> values=stackalloc int[]{-1,-1,-1,-1,6};
            try {
                entry.Style.FillTextureArray(values);
                // A response arriving after the native deadline is discarded.
                Write(session,slot,values[0],values[1],values[2],values[3],values[4]);
            }catch(Exception error) {
                int failures=Interlocked.Increment(ref entry.Failures);
                if((failures & (failures-1))==0)Console.Error.WriteLine("[CSharp][ERROR][UndergroundBackground] Mod="+entry.Style.ModId+" Class="+entry.Style.GetType().FullName+
                    " Method=FillTextureArray Stage=Invoke ContentId="+slot+" SuppressedTotal="+(failures-1)+" Result=CallbackRejected Consequence=NativeFallback Exception="+error);
            }
            break;
        }
        original.Invoke();
    }
}
