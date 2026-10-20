using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.CompilerServices;

namespace BunnyLoader.Source;

public readonly struct BunnyPrefixItemSnapshot
{
    public int ItemType { get; }
    public int Damage { get; }
    public int Mana { get; }
    public int Stack { get; }
    public int CurrentPrefix { get; }
    public bool Accessory { get; }
    public bool Consumable { get; }
    public bool Ranged { get; }

    internal BunnyPrefixItemSnapshot(int type,int damage,int mana,int stack,
                                     int prefix,int flags)
    {
        ItemType=type;
        Damage=damage;
        Mana=mana;
        Stack=stack;
        CurrentPrefix=prefix;
        Accessory=(flags&1)!=0;
        Consumable=(flags&2)!=0;
        Ranged=(flags&4)!=0;
    }
}

public readonly struct BunnyPrefixPlayerSnapshot
{
    public int Life { get; }
    public int LifeMax { get; }

    internal BunnyPrefixPlayerSnapshot(int life, int lifeMax)
    {
        Life = life;
        LifeMax = lifeMax;
    }
}

public static class BunnyPrefixRuntime
{
    private sealed class Mod
    {
        public string Path="";
        public Assembly Assembly=null!;
        public readonly Dictionary<string,BunnyPrefix> Prefixes=new(StringComparer.Ordinal);
    }

    private static readonly Dictionary<string,Mod> mods=new(StringComparer.Ordinal);

    private static BunnyPrefixItemSnapshot ReadItem(string snapshot)
    {
        if(snapshot.Length==0||snapshot.Length>512)
            throw new ArgumentException("Invalid prefix snapshot size");
        var fields=snapshot.Split(',');
        if(fields.Length!=6)throw new ArgumentException("Expected six prefix item fields");
        Span<int> values=stackalloc int[6];
        for(int i=0;i<6;i++)
        {
            if(i==1&&fields[i]=="-1") { values[i]=-1;continue; }
            if(!int.TryParse(fields[i],NumberStyles.None,CultureInfo.InvariantCulture,out values[i]))
                throw new ArgumentException("Invalid prefix item numeric field");
        }
        if(values[0]<1||values[0]>1000000||values[1]>100000||
           (values[1]<0&&(values[5]&1)==0)||
           values[2]>10000||values[3]>999999||values[4]>255||values[5]>7)
            throw new ArgumentException("Prefix item snapshot out of bounds");
        return new(values[0],values[1],values[2],values[3],values[4],values[5]);
    }

    private static BunnyPrefixPlayerSnapshot ReadPlayer(string snapshot)
    {
        if(snapshot.Length==0||snapshot.Length>24)
            throw new ArgumentException("Invalid prefix player snapshot size");
        var fields=snapshot.Split(',');
        if(fields.Length!=2||
           !int.TryParse(fields[0],NumberStyles.None,CultureInfo.InvariantCulture,out int life)||
           !int.TryParse(fields[1],NumberStyles.None,CultureInfo.InvariantCulture,out int maximum)||
           maximum<1||maximum>1000000||life<0||life>maximum)
            throw new ArgumentException("Invalid prefix player snapshot");
        return new BunnyPrefixPlayerSnapshot(life,maximum);
    }

    public static string Dispatch(string modId,string assemblyPath,string owner,
                                  int index,int kind,string snapshot)
    {
        try
        {
            if(index!=0||kind<0||kind>6||modId.Length==0||modId.Length>128||
               owner.Length==0||owner.Length>128||assemblyPath.Length==0||
               assemblyPath.Length>4096||(kind==2?!string.IsNullOrEmpty(snapshot):string.IsNullOrEmpty(snapshot)))
                throw new ArgumentException("Invalid prefix transport arguments");

            if(!mods.TryGetValue(modId,out var mod)||mod.Path!=assemblyPath)
            {
                mod=new Mod{Path=assemblyPath,Assembly=Assembly.Load(File.ReadAllBytes(assemblyPath))};
                mods[modId]=mod;
            }
            if(!mod.Prefixes.TryGetValue(owner,out var prefix))
            {
                var matches=mod.Assembly.GetTypes().Where(type=>
                    type.Name==owner&&type.IsClass&&!type.IsAbstract&&
                    !type.ContainsGenericParameters&&typeof(BunnyPrefix).IsAssignableFrom(type)).ToArray();
                if(matches.Length!=1)
                    throw new InvalidOperationException("Prefix identity must resolve to one local class");
                var type=matches[0];
                prefix=(BunnyPrefix)Activator.CreateInstance(type)!;
                prefix.SetDefaults();
                foreach(string method in new[]{"CanRoll","RollChance","AllStatChangesHaveEffectOn","GetTooltipLines"})
                {
                    var callback=type.GetMethod(method,new[]{typeof(BunnyPrefixItemSnapshot)});
                    if(callback==null)throw new MissingMethodException(type.FullName,method);
                    RuntimeHelpers.PrepareMethod(callback.MethodHandle);
                }
                var apply=type.GetMethod("Apply",new[]{typeof(BunnyPrefixItemSnapshot),
                    typeof(BunnyPrefixApplyStats).MakeByRefType()});
                if(apply==null)throw new MissingMethodException(type.FullName,"Apply");
                RuntimeHelpers.PrepareMethod(apply.MethodHandle);
                var accessory=type.GetMethod("ApplyAccessoryEffects",new[]{
                    typeof(BunnyPrefixPlayerSnapshot),typeof(int).MakeByRefType()});
                if(accessory==null)throw new MissingMethodException(type.FullName,"ApplyAccessoryEffects");
                RuntimeHelpers.PrepareMethod(accessory.MethodHandle);
                mod.Prefixes.Add(owner,prefix);
            }

            if(kind==2)return "READY";
            if(kind==6)
            {
                if(prefix.Category!=PrefixCategory.Accessory)
                    throw new InvalidOperationException("Player bonus requires accessory prefix");
                var player=ReadPlayer(snapshot);
                int defenseBonus=0;
                prefix.ApplyAccessoryEffects(player,ref defenseBonus);
                if(defenseBonus < -100 || defenseBonus > 100)
                    throw new ArgumentOutOfRangeException(nameof(defenseBonus));
                return defenseBonus.ToString(CultureInfo.InvariantCulture);
            }
            var item=ReadItem(snapshot);
            if(kind==0)return prefix.CanRoll(item)?"1":"0";
            if(kind==3)return prefix.AllStatChangesHaveEffectOn(item)?"1":"0";
            if(kind==5)
            {
                var lines=prefix.GetTooltipLines(item);
                if(lines==null)return "";
                if(lines.Count>4)throw new ArgumentOutOfRangeException(nameof(lines));
                var encoded=new System.Collections.Generic.List<string>(lines.Count);
                foreach(var line in lines)
                {
                    string text=line.Text ?? "";
                    if(text.Length==0 || text.Any(char.IsControl))
                        throw new ArgumentException("Invalid tooltip line");
                    byte[] bytes=System.Text.Encoding.UTF8.GetBytes(text);
                    if(bytes.Length>120)
                        throw new ArgumentOutOfRangeException(nameof(lines));
                    encoded.Add((line.IsNegative?"1:":"0:")+Convert.ToHexString(bytes));
                }
                return string.Join(";",encoded);
            }
            if(kind==4)
            {
                var extra=new BunnyPrefixApplyStats();
                prefix.Apply(item,ref extra);
                if(extra.DamageBonus<0||extra.DamageBonus>1000||
                   extra.ValueBonus<0||extra.ValueBonus>100000)
                    throw new ArgumentOutOfRangeException(nameof(extra));
                return extra.DamageBonus.ToString(CultureInfo.InvariantCulture)+","+
                    extra.ValueBonus.ToString(CultureInfo.InvariantCulture);
            }
            int weight=prefix.RollChance(item);
            if(weight<0||weight>20)
                throw new ArgumentOutOfRangeException(nameof(weight));
            return weight.ToString(CultureInfo.InvariantCulture);
        }
        catch(Exception error)
        {
            return "ERROR|[CSharp][ERROR][PrefixCallback] Mod="+modId+
                " Class="+owner+" Kind="+kind+" Reason=ManagedException Result=Rejected Exception="+error;
        }
    }
}
