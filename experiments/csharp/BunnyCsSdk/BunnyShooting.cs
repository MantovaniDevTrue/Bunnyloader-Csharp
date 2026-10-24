using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;

namespace BunnyLoader.Source;

/// <summary>Stats after Terraria selected ammunition and applied weapon modifiers. No native pointers.</summary>
public struct BunnyShot
{
    public float X, Y, VelocityX, VelocityY, KnockBack, Ai0, Ai1, Ai2;
    public int Type, Damage;
    /// <summary>Optional local or UID/name identity; resolved by the native projectile registry.</summary>
    public string? Projectile;
    internal string Encode()
    {
        if(Projectile!=null)BunnyContentKey.Validate(Projectile,nameof(Projectile),"BunnyShot");
        if(Type<0||Type>1000000||Damage<0||Damage>1000000||
           !Finite(X,100000000)||!Finite(Y,100000000)||!Finite(VelocityX,4096)||!Finite(VelocityY,4096)||
           !Finite(KnockBack,10000)||KnockBack<0||!Finite(Ai0,100000000)||!Finite(Ai1,100000000)||!Finite(Ai2,100000000))
            throw new ArgumentException("[CSharp][ERROR][Shoot] Stage=ValidateOutput Reason=ShotOutsideNativeContract");
        return string.Join(",",Projectile==null?"v"+Type.ToString(CultureInfo.InvariantCulture):"m"+Projectile,
            F(X),F(Y),F(VelocityX),F(VelocityY),Damage.ToString(CultureInfo.InvariantCulture),F(KnockBack),F(Ai0),F(Ai1),F(Ai2));
    }
    private static bool Finite(float n,float bound)=>float.IsFinite(n)&&MathF.Abs(n)<=bound;
    private static string F(float n)=>n.ToString("R",CultureInfo.InvariantCulture);
}

/// <summary>One weapon use on the Mono worker. Spawn requests apply on the game thread only after validation.</summary>
public sealed class BunnyShootContext
{
    private readonly int threadId=Environment.CurrentManagedThreadId;
    internal BunnyShootContext() { }
    public int PlayerIndex { get; internal set; }
    public int ItemType { get; internal set; }
    public int WeaponDamage { get; internal set; }
    public Random Random { get; internal set; }=null!;
    private readonly List<BunnyShot> shots=new();
    internal bool CanSpawn;
    internal IReadOnlyList<BunnyShot> Shots=>shots;
    /// <summary>Uses the original owner/source/modifier; maximum 64 extra projectiles per native shot.</summary>
    public void Spawn(BunnyShot shot)
    {
        if(Environment.CurrentManagedThreadId!=threadId)throw new InvalidOperationException("[CSharp][ERROR][Shoot] API=BunnyShootContext.Spawn Stage=QueueSpawn ExpectedThread="+threadId+" ActualThread="+Environment.CurrentManagedThreadId+" Reason=WrongCallbackThread");
        if(!CanSpawn)throw new InvalidOperationException("Spawn is only available during OnShoot, not CanShoot or ModifyShootStats");
        if(shots.Count>=64)throw new InvalidOperationException("[CSharp][ERROR][Shoot] Stage=QueueSpawn Expected=AtMost64 Reason=SpawnCapacityExceeded");
        _=shot.Encode();shots.Add(shot);
    }
}

/// <summary>Common item shooting lifecycle. Arbitrary C# algorithms execute on MonoVM, independently of QuickJS.</summary>
public abstract class BunnyShootSource : BunnyRecipeSource
{
    /// <summary>Posiciona o disparo na mira local, limitada por Player.LimitPointToPlayerReachableArea nativo.</summary>
    public bool SpawnAtCursor { get; set; }
    public virtual bool CanShoot(BunnyShootContext context)=>true;
    public virtual void ModifyShootStats(BunnyShootContext context,ref BunnyShot shot) { }
    /// <summary>Return false to replace the original shot with the requests queued through context.Spawn.</summary>
    public virtual bool OnShoot(BunnyShootContext context,BunnyShot shot)=>true;
    public int ShootCallbackMask
    {
        get
        {
            int mask=0;var type=GetType();
            foreach(var pair in new[]{("CanShoot",1), ("ModifyShootStats",2), ("OnShoot",4)})
            {
                var method=type.GetMethod(pair.Item1,BindingFlags.Instance|BindingFlags.Public);
                if(method==null||method.GetBaseDefinition().DeclaringType!=typeof(BunnyShootSource))
                    throw new InvalidOperationException("[CSharp][ERROR][Shoot] Class="+type.FullName+" Method="+pair.Item1+" Stage=Bind Reason=HiddenOrAmbiguousCallback");
                if(method.DeclaringType!=typeof(BunnyShootSource))mask|=pair.Item2;
            }
            return mask;
        }
    }
}

/// <summary>Native transport endpoint. Instances/defaults and callback JIT are prepared before gameplay.</summary>
public static class BunnyShootRuntime
{
    private sealed class Mod
    {
        public string Path="";
        public Assembly Assembly=null!;
        public readonly Dictionary<string,BunnyShootSource> Items=new(StringComparer.Ordinal);
    }
    private static readonly Dictionary<string,Mod> mods=new(StringComparer.Ordinal);
    private static bool coreReady;
    private static (BunnyShootContext Context,BunnyShot Shot) ReadSnapshot(string snapshot)
    {
        var f=snapshot.Split(',');if(f.Length!=14)throw new ArgumentException("Expected 14 shooting snapshot fields");
        int I(int i)=>int.Parse(f[i],NumberStyles.Integer,CultureInfo.InvariantCulture);
        float F(int i)=>float.Parse(f[i],NumberStyles.Float,CultureInfo.InvariantCulture);
        var context=new BunnyShootContext{PlayerIndex=I(0),ItemType=I(1),WeaponDamage=I(2),Random=new Random(I(3))};
        if(context.PlayerIndex<0||context.PlayerIndex>255||context.ItemType<1||context.ItemType>1000000||context.WeaponDamage<0||context.WeaponDamage>1000000)
            throw new ArgumentException("Invalid player/item/damage snapshot");
        var shot=new BunnyShot{Type=I(4),X=F(5),Y=F(6),VelocityX=F(7),VelocityY=F(8),Damage=I(9),KnockBack=F(10),Ai0=F(11),Ai1=F(12),Ai2=F(13)};
        _=shot.Encode();return (context,shot);
    }
    private static string EncodeResponse(bool original,BunnyShot shot,BunnyShootContext context)
        => "SHOT1|"+(original?"1":"0")+"|"+shot.Encode()+"|"+string.Join(";",context.Shots.Select(s=>s.Encode()));
    private static void PrepareCore()
    {
        if(coreReady)return;
        // Warm only SDK-owned parsing/commands/math, with no native calls and no user gameplay callbacks.
        var (context,shot)=ReadSnapshot("0,1,1,1,0,100,200,1,0,1,1,0,0,0");
        context.CanSpawn=true;
        for(int i=0;i<64;i++){
            float angle=context.Random.NextSingle();shot.VelocityX=MathF.Cos(angle);shot.VelocityY=MathF.Sin(angle);
            context.Spawn(shot);
        }
        _=EncodeResponse(true,shot,context);coreReady=true;
    }
    public static string Dispatch(string modId,string assemblyPath,string owner,int index,int kind,string snapshot)
    {
        string callback="Bind",className=owner;
        try
        {
            if(index!=0||kind<0||kind>2||snapshot.Length>1024||(kind==2&&snapshot.Length!=0))
                throw new ArgumentException("Invalid shooting transport arguments: Index="+index+" Kind="+kind+" SnapshotLength="+snapshot.Length);
            if(!mods.TryGetValue(modId,out var mod)||mod.Path!=assemblyPath)
            {
                mod=new Mod{Path=assemblyPath,Assembly=Assembly.Load(File.ReadAllBytes(assemblyPath))};mods[modId]=mod;
            }
            if(!mod.Items.TryGetValue(owner,out var item))
            {
                var types=mod.Assembly.GetTypes().Where(t=>t.Name==owner&&!t.IsAbstract&&!t.ContainsGenericParameters&&typeof(BunnyShootSource).IsAssignableFrom(t)).ToArray();
                if(types.Length!=1)throw new InvalidOperationException("Expected one registered shooting item; found "+types.Length);
                className=types[0].FullName??owner;callback="SetDefaults";
                item=(BunnyShootSource)Activator.CreateInstance(types[0])!;
                var declaration=types[0].GetCustomAttribute<BunnyContentAttribute>(false);
                if(declaration!=null){types[0].GetProperty("DisplayName")!.SetValue(item,declaration.DisplayName);types[0].GetProperty("Texture")!.SetValue(item,declaration.Texture);}
                types[0].GetMethod("SetDefaults",Type.EmptyTypes)!.Invoke(item,null);
                if(item.ShootCallbackMask==0)throw new InvalidOperationException("No shooting callbacks declared");
                // Compile methods without running mod gameplay or consuming its state during warmup.
                callback="PrepareMethod";
                foreach(var name in new[]{"CanShoot","ModifyShootStats","OnShoot"})
                    System.Runtime.CompilerServices.RuntimeHelpers.PrepareMethod(types[0].GetMethod(name)!.MethodHandle);
                mod.Items.Add(owner,item);
            }
            className=item.GetType().FullName??owner;
            callback="PrepareCore";PrepareCore();
            if(kind==2)return "READY";
            callback="ReadSnapshot";var (context,shot)=ReadSnapshot(snapshot);
            if(kind==0){callback="CanShoot";return item.CanShoot(context)?"1":"0";}
            callback="ModifyShootStats";item.ModifyShootStats(context,ref shot);_=shot.Encode();
            callback="OnShoot";bool original;context.CanSpawn=true;
            try {original=item.OnShoot(context,shot);}finally {context.CanSpawn=false;}
            // Validate every queued command before returning any of them to the native bridge.
            return EncodeResponse(original,shot,context);
        }
        catch(Exception error)
        {
            return "ERROR|[CSharp][ERROR][Shoot] Mod="+modId+" Class="+className+" Method="+callback+" Assembly="+assemblyPath+
                " Stage=Invoke Kind="+kind+" Reason=ManagedException Result=ShotRejected Exception="+error;
        }
    }
}
