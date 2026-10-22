using System;
using System.Collections.Generic;
using System.Threading;

namespace BunnyLoader.Source;

/// <summary>
/// A scene effect, as in tModLoader: while active for the local player it competes
/// with the other effects by Priority plus GetWeight, and the winner picks the
/// surface and underground backgrounds. The evaluation runs on the Mono worker
/// right after the game's Player.UpdateBiomes, only for the local player, and
/// activates the winner through the existing native background services. No
/// JavaScript runs. Music, water and map background are not available here yet.
/// </summary>
public abstract class BunnySceneEffect : BunnyHookModule
{
    private BunnyHookScope? scope;

    public sealed override void Load(BunnyHookScope hooks)
    {
        scope=hooks;
        try
        {
            SetStaticDefaults();
            BunnySceneRegistry.Attach(this,hooks);
        }
        catch
        {
            scope=null;
            throw;
        }
    }

    /// <summary>Initialize reusable managed state after ModId is assigned.</summary>
    public virtual void SetStaticDefaults() { }

    /// <summary>Priority of this effect against the others. Backgrounds only use the winner from BiomeLow up.</summary>
    public virtual BunnyBackgroundPriority Priority=>BunnyBackgroundPriority.None;

    /// <summary>0 to 1: breaks ties between effects of the same Priority. Other values are clamped.</summary>
    public virtual double GetWeight(BunnyNativeObject player)=>0.5;

    /// <summary>Called every evaluation; an exception counts as inactive for that frame.</summary>
    public abstract bool IsSceneEffectActive(BunnyNativeObject player);

    /// <summary>Called every evaluation, active or not. Use it to turn a screen filter on and off.</summary>
    public virtual void SpecialVisuals(BunnyNativeObject player,bool isActive) { }

    /// <summary>Class of a BunnySurfaceBackgroundStyle from this mod, or null. The type is resolved by mod UID and class name.</summary>
    public virtual Type? SurfaceBackgroundStyle=>null;

    /// <summary>Class of a BunnyUndergroundBackgroundStyle from this mod, or null. The type is resolved by mod UID and class name.</summary>
    public virtual Type? UndergroundBackgroundStyle=>null;

    /// <summary>Native field access bound to this module's scope, for reading player state inside the callbacks.</summary>
    protected BunnyNativeScope Native=>scope is null?throw new InvalidOperationException("The scene effect has not been loaded"):scope.Native;

    public sealed override void Unload()
    {
        BunnySceneRegistry.Detach(this);
        scope=null;
    }

    public override string DiagnosticStatus()=>"SceneEffect="+GetType().Name+" Priority="+Priority;
}

internal static class BunnySceneRegistry
{
    private const int RetryFrames=120;

    private sealed class Entry
    {
        public Entry(BunnySceneEffect effect,BunnyHookScope scope)
        {
            Effect=effect;
            Scope=scope;
            Key=effect.ModId+"/"+effect.GetType().FullName;
        }
        public BunnySceneEffect Effect { get; }
        public BunnyHookScope Scope { get; }
        public string Key { get; }
        public int Failures;
    }

    private sealed class Selection
    {
        public Selection(string mod,string className,BunnyBackgroundPriority priority,double weight)
        {
            Mod=mod;
            Class=className;
            Priority=priority;
            Weight=weight;
        }
        public string Mod { get; }
        public string Class { get; }
        public BunnyBackgroundPriority Priority { get; }
        public double Weight { get; }
        public bool SameStyle(Selection other)=>Mod==other.Mod&&Class==other.Class;
    }

    private static readonly object Gate=new();
    private static readonly List<Entry> entries=new();
    private static Entry[] snapshot=Array.Empty<Entry>();
    private static BunnyHookScope? owner;
    private static BunnyMethodHook? hook;
    private static BunnyNativeMember? localPlayer;
    private static BunnyNativeMember? playerIndex;

    // Evaluate runs on one worker thread; the lock also covers ReleaseSelections.
    private static readonly object EvaluateGate=new();
    private static readonly List<BunnySceneRanking.Entry> scratch=new();
    private static int[] priorities=new int[8];
    private static Selection? surface;
    private static Selection? underground;
    private static long frame;
    private static long surfaceRetry;
    private static long undergroundRetry;
    private static int channelFailures;

    internal static void Attach(BunnySceneEffect effect,BunnyHookScope scope)
    {
        lock(Gate)
        {
            entries.Add(new Entry(effect,scope));
            try { Rebuild(); }
            catch(Exception registration)
            {
                entries.RemoveAt(entries.Count-1);
                try { Rebuild(); }
                catch(Exception rollback) { throw new AggregateException("Scene effect hook registration and restoration both failed",registration,rollback); }
                throw;
            }
        }
    }

    internal static void Detach(BunnySceneEffect effect)
    {
        lock(Gate)
        {
            entries.RemoveAll(e=>ReferenceEquals(e.Effect,effect));
            Rebuild();
        }
    }

    private static void Rebuild()
    {
        BunnyHookScope? next=entries.Count==0?null:entries[0].Scope;
        if(!ReferenceEquals(owner,next)||hook is null||hook.IsDisposed)
        {
            hook?.Dispose();
            hook=null;
            owner=null;
            Volatile.Write(ref localPlayer,null);
            Volatile.Write(ref playerIndex,null);
            if(next is not null)
            {
                BunnyNativeScope access=next.Native;
                BunnyNativeMember mine=access.Type("Terraria.Main").Field("myPlayer",BunnyValueKind.Int32,isStatic:true);
                BunnyNativeMember index=access.Type("Terraria.Player").Field("whoAmI",BunnyValueKind.Int32);
                hook=next.HookVoid("Terraria.Player","UpdateBiomes",Dispatch);
                Volatile.Write(ref localPlayer,mine);
                Volatile.Write(ref playerIndex,index);
                owner=next;
            }
        }
        Volatile.Write(ref snapshot,entries.ToArray());
        if(entries.Count==0)ReleaseSelections();
    }

    private static void Dispatch(BunnyOriginalVoid original,nint self)
    {
        original.Invoke();
        Entry[] list=Volatile.Read(ref snapshot);
        BunnyNativeMember? mine=Volatile.Read(ref localPlayer);
        BunnyNativeMember? index=Volatile.Read(ref playerIndex);
        if(list.Length==0||mine is null||index is null)return;
        BunnyNativeObject player=BunnyNativeObject.FromHook(self);
        try
        {
            // Like the original loader, only the local player drives the scene.
            if(index.Read(player).AsInt32()!=mine.Read().AsInt32())return;
        }
        catch(Exception error)
        {
            Report("LocalPlayerCheck",null,error);
            return;
        }
        Evaluate(list,player);
    }

    private static void Evaluate(Entry[] list,BunnyNativeObject player)
    {
        lock(EvaluateGate)
        {
            long now=++frame;
            scratch.Clear();
            if(priorities.Length<list.Length)priorities=new int[list.Length];
            for(int i=0;i<list.Length;i++)
            {
                Entry entry=list[i];
                bool active=false;
                try { active=entry.Effect.IsSceneEffectActive(player); }
                catch(Exception error) { Report("IsSceneEffectActive",entry,error); }
                // SpecialVisuals runs for every effect, active or not.
                try { entry.Effect.SpecialVisuals(player,active); }
                catch(Exception error) { Report("SpecialVisuals",entry,error); }
                if(!active)continue;

                int priority=0;
                double weight=0.0;
                try { priority=(int)entry.Effect.Priority; }
                catch(Exception error) { Report("Priority",entry,error); }
                try { weight=entry.Effect.GetWeight(player); }
                catch(Exception error) { Report("GetWeight",entry,error); }
                priorities[i]=priority;
                scratch.Add(new BunnySceneRanking.Entry(i,entry.Key,BunnySceneRanking.RankOf(priority,weight)));
            }

            Selection? pickSurface=null;
            Selection? pickUnderground=null;
            if(scratch.Count>0)
            {
                BunnySceneRanking.Sort(scratch);
                foreach(BunnySceneRanking.Entry ranked in scratch)
                {
                    Entry entry=list[ranked.Index];
                    int priority=priorities[ranked.Index];
                    double weight=ranked.Rank-priority;
                    if(pickSurface is null)
                    {
                        Type? style=ReadStyle(entry,true);
                        if(style is not null)pickSurface=new Selection(entry.Effect.ModId,style.FullName!,(BunnyBackgroundPriority)priority,weight);
                    }
                    if(pickUnderground is null)
                    {
                        Type? style=ReadStyle(entry,false);
                        if(style is not null)pickUnderground=new Selection(entry.Effect.ModId,style.FullName!,(BunnyBackgroundPriority)priority,weight);
                    }
                    if(pickSurface is not null&&pickUnderground is not null)break;
                }
            }

            Apply(ref surface,ref surfaceRetry,pickSurface,now,SetSurface,"Surface");
            Apply(ref underground,ref undergroundRetry,pickUnderground,now,SetUnderground,"Underground");
        }
    }

    private static Type? ReadStyle(Entry entry,bool isSurface)
    {
        Type? type;
        try { type=isSurface?entry.Effect.SurfaceBackgroundStyle:entry.Effect.UndergroundBackgroundStyle; }
        catch(Exception error)
        {
            Report(isSurface?"SurfaceBackgroundStyle":"UndergroundBackgroundStyle",entry,error);
            return null;
        }
        if(type is null)return null;
        Type required=isSurface?typeof(BunnySurfaceBackgroundStyle):typeof(BunnyUndergroundBackgroundStyle);
        if(type.IsAbstract||type.FullName is null||!required.IsAssignableFrom(type))
        {
            Report(isSurface?"SurfaceBackgroundStyle":"UndergroundBackgroundStyle",entry,
                new ArgumentException("Expected a concrete class deriving from "+required.Name+", got "+type));
            return null;
        }
        return type;
    }

    private static void SetSurface(Selection selection,bool on)=>BunnySurfaceBackgrounds.Set(selection.Mod,selection.Class,
        on?selection.Priority:BunnyBackgroundPriority.None,on?selection.Weight:0.0,on);

    private static void SetUnderground(Selection selection,bool on)=>BunnyUndergroundBackgrounds.Set(selection.Mod,selection.Class,
        on?selection.Priority:BunnyBackgroundPriority.None,on?selection.Weight:0.0,on);

    // Activates the winner of one channel and turns the previous one off when
    // the style changes. A failing activation is retried after a pause, so a
    // style that is not registered cannot throw on every frame.
    private static void Apply(ref Selection? current,ref long retryAt,Selection? want,long now,Action<Selection,bool> set,string channel)
    {
        if(current is not null&&(want is null||!current.SameStyle(want)))
        {
            try { set(current,false); }
            catch(Exception error) { ReportChannel(channel,"Deactivate",error); }
            current=null;
        }
        if(want is null)return;
        if(current is not null&&current.Priority==want.Priority&&current.Weight==want.Weight)return;
        if(now<retryAt)return;
        try
        {
            set(want,true);
            current=want;
        }
        catch(Exception error)
        {
            ReportChannel(channel,"Activate",error);
            retryAt=now+RetryFrames;
        }
    }

    private static void ReleaseSelections()
    {
        lock(EvaluateGate)
        {
            if(surface is not null)
            {
                try { SetSurface(surface,false); }
                catch(Exception error) { ReportChannel("Surface","Release",error); }
                surface=null;
            }
            if(underground is not null)
            {
                try { SetUnderground(underground,false); }
                catch(Exception error) { ReportChannel("Underground","Release",error); }
                underground=null;
            }
        }
    }

    private static void Report(string method,Entry? entry,Exception error)
    {
        int failures=entry is null?1:Interlocked.Increment(ref entry.Failures);
        if((failures&(failures-1))!=0)return;
        Console.Error.WriteLine("[CSharp][ERROR][SceneEffect] Mod="+(entry is null?"":entry.Effect.ModId)+
            " Class="+(entry is null?"":entry.Effect.GetType().FullName)+" Method="+method+
            " SuppressedTotal="+(failures-1)+" Result=CallbackRejected Consequence=EffectIgnoredThisFrame Exception="+error);
    }

    private static void ReportChannel(string channel,string stage,Exception error)
    {
        int failures=Interlocked.Increment(ref channelFailures);
        if((failures&(failures-1))!=0)return;
        Console.Error.WriteLine("[CSharp][ERROR][SceneEffect] Channel="+channel+" Stage="+stage+
            " SuppressedTotal="+(failures-1)+" Result=Rejected Consequence=GameBackgroundKept Exception="+error);
    }
}
