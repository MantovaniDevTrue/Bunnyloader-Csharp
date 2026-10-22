using System;
using System.Buffers;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Threading;

namespace BunnyLoader.Source;

internal static class BunnySurfaceRegistry
{
    private sealed record Entry(BunnySurfaceBackgroundStyle Style,BunnyHookScope Scope) { public int Failures; }
    private static readonly object Gate=new();
    private static readonly List<Entry> entries=new();
    private static Entry[] snapshot=Array.Empty<Entry>();
    private static BunnyHookScope? owner;
    private static BunnyMethodHook? hook;
    [DllImport("bunny",EntryPoint="bunny_surface_read",CallingConvention=CallingConvention.Cdecl)]
    private static extern int Read(int version,out long session,out int slot,out int operation,out double speed,out int fades);
    [DllImport("bunny",EntryPoint="bunny_surface_context",CallingConvention=CallingConvention.Cdecl)]
    private static extern unsafe int Context(long session,int slot,double* values,int length,out uint rgba);
    [DllImport("bunny",EntryPoint="bunny_surface_fades",CallingConvention=CallingConvention.Cdecl)]
    private static extern unsafe int Fades(long session,int slot,float* values,int length,int write);
    [DllImport("bunny",EntryPoint="bunny_surface_complete",CallingConvention=CallingConvention.Cdecl)]
    private static extern int Complete(long session,int slot,int texture,int defaultClose,double scale,double parallax,double a,double b);
    internal static BunnySurfaceBackgroundStyle? Get(int slot)
    {foreach(Entry entry in Volatile.Read(ref snapshot))if(entry.Style.Slot==slot)return entry.Style;return null;}
    internal static void Attach(BunnySurfaceBackgroundStyle style,BunnyHookScope scope)
    {
        lock(Gate) {
            entries.Add(new(style,scope));
            try { Rebuild(); }
            catch(Exception registration) {
                entries.RemoveAt(entries.Count-1);
                try { Rebuild(); }
                catch(Exception rollback) { throw new AggregateException("Surface callback registration and restoration both failed",registration,rollback); }
                throw;
            }
        }
    }
    internal static void Detach(BunnySurfaceBackgroundStyle style)
    {lock(Gate){entries.RemoveAll(e=>ReferenceEquals(e.Style,style));Rebuild();}}
    internal static void DetachScope(BunnyHookScope scope)
    {lock(Gate){entries.RemoveAll(e=>ReferenceEquals(e.Scope,scope));Rebuild();}}
    private static void Rebuild()
    {
        Volatile.Write(ref snapshot,entries.ToArray());
        BunnyHookScope? next=entries.Count==0?null:entries[0].Scope;
        if(!ReferenceEquals(owner,next) || hook is null || hook.IsDisposed) {
            hook?.Dispose();hook=null;owner=null;
            if(next is not null){hook=next.HookSystemStage("SurfaceBackground",Dispatch);owner=next;}
        }
    }
    private static unsafe void Dispatch(BunnyOriginalVoid original,nint self)
    {
        try {
            if(Read(1,out long session,out int slot,out int operation,out double speed,out int fadeCount)==0)return;
            foreach(Entry entry in Volatile.Read(ref snapshot)) {
                if(entry.Style.Slot!=slot)continue;
                string method=operation switch{0=>"ModifyFarFades",1=>"ChooseFarTexture",2=>"ChooseMiddleTexture",3=>"PreDrawCloseBackground/ChooseCloseTexture",_=>"UnknownOperation"};
                try {
                    int texture=-1;bool defaultClose=true;double scale=1.25,parallax=.37,a=1800,b=1750;
                    switch(operation) {
                        case 0:
                            if(fadeCount<=0 || fadeCount>4096)throw new InvalidOperationException("Invalid native fade count: "+fadeCount);
                            float[] rented=ArrayPool<float>.Shared.Rent(fadeCount);
                            try {fixed(float* data=rented) {
                                if(Fades(session,slot,data,fadeCount,0)==0)return;
                                entry.Style.ModifyFarFades(rented.AsSpan(0,fadeCount),speed);
                                if(Fades(session,slot,data,fadeCount,1)==0)return;
                            }}finally{ArrayPool<float>.Shared.Return(rented);}
                            break;
                        case 1:texture=entry.Style.ChooseFarTexture();break;
                        case 2:texture=entry.Style.ChooseMiddleTexture();break;
                        case 3:
                            double* values=stackalloc double[13];
                            if(Context(session,slot,values,13,out uint rgba)==0)return;
                            var context=new BunnySurfaceDrawContext(session,slot,new ReadOnlySpan<double>(values,13),rgba);
                            defaultClose=entry.Style.PreDrawCloseBackground(in context);
                            if(defaultClose)texture=entry.Style.ChooseCloseTexture(ref scale,ref parallax,ref a,ref b);
                            break;
                        default:throw new InvalidOperationException("Unsupported surface operation: "+operation);
                    }
                    // Expired replies are deliberately discarded; the native service owns fallback.
                    Complete(session,slot,texture,defaultClose?1:0,scale,parallax,a,b);
                }catch(Exception error) {
                    int failures=Interlocked.Increment(ref entry.Failures);
                    if((failures & (failures-1))==0)Console.Error.WriteLine("[CSharp][ERROR][SurfaceBackground] Mod="+entry.Style.ModId+
                        " Class="+entry.Style.GetType().FullName+" Method="+method+" Stage=Invoke ContentId="+slot+
                        " SuppressedTotal="+(failures-1)+" Result=CallbackRejected Consequence=NativeFallback Exception="+error);
                }
                break;
            }
        }finally{original.Invoke();}
    }
}
