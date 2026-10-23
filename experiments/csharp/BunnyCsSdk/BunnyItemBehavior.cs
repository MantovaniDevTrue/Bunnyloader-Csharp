using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;

namespace BunnyLoader.Source;

public enum BunnyEquipKind { Head, Body, Legs, Beard, Back, Wings }
public enum BunnyEquipDrawFlags { None=0, UseHairColor=1, ShowHands=2 }
public enum BunnyEquipFloat { MeleeDamage, RangedDamage, MagicDamage, MinionDamage, MoveSpeed }
public enum BunnyEquipFlag { StrongBees }

/// <summary>Politicas de item executadas pelos hooks reais do host; sem ida ao Mono por quadro.</summary>
public sealed class BunnyItemBehavior
{
    private readonly List<string> equips = new();
    private readonly List<string> effects = new();
    private readonly List<string> nativeEffects = new();
    public string NativeAccessoryEffectsDefinition => string.Join(";",nativeEffects);
    private string clone = "";
    private string animation = "null", light = "null", alpha = "null";
    public bool ColorFromPlayerHair { get; set; }
    public bool Robe { get; set; }
    public BunnyArmorEffects Armor { get; } = new();
    private BunnyArmorSet? armorSet;
    /// <summary>Efeitos de peca e conjunto instalados pelo host nativo.</summary>
    public string NativeArmorDefinition => Armor.Empty&&armorSet is null?"":
        (armorSet is not null && armorSet.LocalizedTextKey.Length != 0 ? "A2|" : "A1|")+
        Armor.WireEffects+"|"+Armor.WireImmunity+"|"+(armorSet?.ToWire()??"-|-|-|-|-|-");
    private int defense;
    private string wings="null";
    /// <summary>Distingue defesa explicitamente zero dos defaults herdados do item vanilla.</summary>
    public bool HasDefenseOverride { get; private set; }
    public int Defense
    {
        get => defense;
        set { if(value<0||value>1000)throw new ArgumentOutOfRangeException(nameof(value));defense=value;HasDefenseOverride=true; }
    }
    public void DefineArmorSet(BunnyArmorSet set)
    { if(set is null)throw new ArgumentNullException(nameof(set));if(armorSet is not null)throw new InvalidOperationException("Conjunto ja definido");armorSet=set; }

    /// <summary>WingStats e os cinco refs do WingMovement original, sem callback Mono por quadro.</summary>
    public void SetWingStats(int flyTime,float flySpeed,float acceleration,
        float ascentWhenFalling,float ascentWhenRising,float maxCanAscendMultiplier,
        float maxAscentMultiplier,float constantAscend)
    {
        if(flyTime<1||flyTime>6000)throw new ArgumentOutOfRangeException(nameof(flyTime));
        foreach(var n in new[]{flySpeed,acceleration})if(!float.IsFinite(n)||n<0||n>100)throw new ArgumentOutOfRangeException(nameof(flySpeed));
        foreach(var n in new[]{ascentWhenFalling,ascentWhenRising,maxCanAscendMultiplier,maxAscentMultiplier,constantAscend})
            if(!float.IsFinite(n)||n<0||n>10)throw new ArgumentOutOfRangeException(nameof(ascentWhenFalling));
        wings="{\"stats\":["+flyTime+","+Number(flySpeed)+","+Number(acceleration)+"],\"vertical\":["+
            string.Join(",",new[]{ascentWhenFalling,ascentWhenRising,maxCanAscendMultiplier,maxAscentMultiplier,constantAscend}.Select(Number))+"]}";
    }
    public void CloneVanillaDefaults(string itemIdName)
    { Name(itemIdName); clone=itemIdName; }
    public void EquipTexture(BunnyEquipKind kind,string texture,BunnyEquipDrawFlags flags=BunnyEquipDrawFlags.None)
    {
        if(!Enum.IsDefined(typeof(BunnyEquipKind),kind)||(flags & ~(BunnyEquipDrawFlags.UseHairColor|BunnyEquipDrawFlags.ShowHands))!=0)
            throw new ArgumentOutOfRangeException(nameof(kind));
        if(string.IsNullOrWhiteSpace(texture)||texture.Length>200||texture.Contains("..")||texture.StartsWith("/")||
            texture.Any(c=>!(char.IsAsciiLetterOrDigit(c)||c=='_'||c=='/'||c=='-')))
            throw new ArgumentException("Textura deve ser caminho relativo sem extensao",nameof(texture));
        if(equips.Count>=4||equips.Any(e=>e.StartsWith("[\""+kind+"\",")))throw new InvalidOperationException("Slot repetido ou mais de quatro texturas");
        equips.Add("[\""+kind+"\",\""+texture+"\","+((int)flags).ToString(CultureInfo.InvariantCulture)+"]");
    }
    public void MultiplyOnEquip(BunnyEquipFloat field,float multiplier) => Effect(field,"mul",multiplier,0,10);
    public void AddOnEquip(BunnyEquipFloat field,float value) => Effect(field,"add",value,-10,10);
    public void SetOnEquip(BunnyEquipFlag flag)
    {
        if(!Enum.IsDefined(typeof(BunnyEquipFlag),flag))throw new ArgumentOutOfRangeException(nameof(flag));
        Record("[\"strongBees\",\"set\",true]","strongBees:set:1");
    }
    private void Effect(BunnyEquipFloat field,string op,float value,float min,float max)
    {
        if(!Enum.IsDefined(typeof(BunnyEquipFloat),field)||!float.IsFinite(value)||value<min||value>max)
            throw new ArgumentOutOfRangeException(nameof(value));
        string name=field.ToString();name=char.ToLowerInvariant(name[0])+name.Substring(1);
        Record("[\""+name+"\",\""+op+"\","+Number(value)+"]",name+":"+op+":"+Number(value));
    }
    private void Record(string effect,string nativeEffect) {
        if(effects.Count>=16)throw new InvalidOperationException("Mais de 16 efeitos por item");
        effects.Add(effect);nativeEffects.Add(nativeEffect);
    }
    public void Animate(int frames,int ticksPerFrame,bool animatesAsSoul=false,bool pulse=false,bool noGravity=false)
    {
        if(frames<1||frames>128||ticksPerFrame<1||ticksPerFrame>600)throw new ArgumentOutOfRangeException(nameof(frames));
        animation="["+frames+","+ticksPerFrame+","+Bool(animatesAsSoul)+","+Bool(pulse)+","+Bool(noGravity)+"]";
    }
    public void WorldLight(float red,float green,float blue,bool scaleByEssence=false)
    {
        foreach(float value in new[]{red,green,blue})if(!float.IsFinite(value)||value<0||value>2)throw new ArgumentOutOfRangeException(nameof(red));
        light="["+Number(red)+","+Number(green)+","+Number(blue)+","+Bool(scaleByEssence)+"]";
    }
    public void WorldAlpha(byte red,byte green,byte blue,byte opacity) => alpha="["+red+","+green+","+blue+","+opacity+"]";
    internal string ToJson()
    {
        if(equips.Count==0&&clone.Length==0&&animation=="null"&&light=="null"&&alpha=="null"&&!ColorFromPlayerHair&&!Robe&&wings=="null")return "";
        return "{\"v\":1,\"clone\":\""+clone+"\",\"equips\":["+string.Join(",",equips)+"],\"effects\":["+string.Join(",",effects)+
            "],\"nativeAccessory\":"+Bool(effects.Count>0)+",\"nativeArmor\":"+Bool(!Armor.Empty||armorSet is not null)+",\"animation\":"+animation+",\"light\":"+light+",\"alpha\":"+alpha+",\"hair\":"+Bool(ColorFromPlayerHair)+",\"robe\":"+Bool(Robe)+",\"defense\":"+defense+",\"armor\":"+Armor.ToJson()+",\"armorSet\":"+(armorSet?.ToJson()??"null")+",\"wings\":"+wings+"}";
    }
    private static string Number(float n) => n.ToString("R",CultureInfo.InvariantCulture);
    private static string Bool(bool b) => b?"true":"false";
    private static void Name(string name) { if(string.IsNullOrEmpty(name)||name.Length>72||name.Any(c=>!(char.IsAsciiLetterOrDigit(c)||c=='_')))throw new ArgumentException("Nome de ItemID invalido"); }
}
