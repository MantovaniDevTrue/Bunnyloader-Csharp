using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;


namespace BunnyLoader.Source;

public enum BunnyArmorStat { MeleeDamage, RangedDamage, MagicDamage, MinionDamage, MoveSpeed }
public enum BunnyArmorInteger { StatManaMax2, MaxMinions, StatDefense, LifeRegen }

/// <summary>Efeitos do UpdateEquip real. O Terraria reinicia os atributos em cada ciclo.</summary>
public sealed class BunnyArmorEffects
{
    private readonly List<object[]> effects = new();
    private readonly HashSet<string> immune = new(StringComparer.Ordinal);
    public void Add(BunnyArmorStat field, float value) => Float(field,"add",value,-10,10);
    public void Multiply(BunnyArmorStat field, float value) => Float(field,"mul",value,0,10);
    public void Add(BunnyArmorInteger field, int value)
    {
        if(!Enum.IsDefined(field)||value < -1000||value > 1000)throw new ArgumentOutOfRangeException(nameof(value));
        Record(new object[]{Field(field.ToString()),"int",value});
    }
    public void ImmuneTo(string vanillaBuffName)
    {
        Name(vanillaBuffName);
        if(immune.Count>=32&&!immune.Contains(vanillaBuffName))throw new InvalidOperationException("Mais de 32 imunidades");
        immune.Add(vanillaBuffName);
    }
    private void Float(BunnyArmorStat field,string op,float value,float min,float max)
    {
        if(!Enum.IsDefined(field)||!float.IsFinite(value)||value<min||value>max)throw new ArgumentOutOfRangeException(nameof(value));
        Record(new object[]{Field(field.ToString()),op,value});
    }
    private void Record(object[] effect)
    { if(effects.Count>=16)throw new InvalidOperationException("Mais de 16 efeitos de armadura");effects.Add(effect); }
    internal bool Empty => effects.Count==0&&immune.Count==0;
    internal string WireEffects => effects.Count==0?"-":string.Join(";",effects.Select(e=>(string)e[0]+":"+(string)e[1]+":"+Convert.ToString(e[2],CultureInfo.InvariantCulture)));
    internal string WireImmunity => immune.Count==0?"-":string.Join(",",immune.OrderBy(x=>x,StringComparer.Ordinal));
    internal string ToJson() => "{\"effects\":["+string.Join(",",effects.Select(e=>"["+Quote((string)e[0])+","+Quote((string)e[1])+","+Convert.ToString(e[2],CultureInfo.InvariantCulture)+"]"))+"],\"immune\":["+string.Join(",",immune.OrderBy(x=>x,StringComparer.Ordinal).Select(Quote))+"]}";
    internal static string Quote(string value) => "\""+value.Replace("\\","\\\\").Replace("\"","\\\"")+"\"";
    internal static string Field(string name) => char.ToLowerInvariant(name[0])+name.Substring(1);
    internal static void Name(string name)
    { if(string.IsNullOrEmpty(name)||name.Length>72||name.Any(c=>!(char.IsAsciiLetterOrDigit(c)||c=='_')))throw new ArgumentException("Nome nativo invalido",nameof(name)); }
}

/// <summary>Conjunto inserido em ArmorSetBonuses: bonus e tooltip nativos do Terraria 1.4.5.</summary>
public sealed class BunnyArmorSet
{
    internal string Head { get; }
    internal string Body { get; }
    internal string Legs { get; }
    internal string Text { get; }
    internal string LocalizedTextKey { get; private set; } = "";
    public BunnyArmorEffects Effects { get; } = new();
    public BunnyArmorSet(string head,string body,string legs,string text)
    {
        foreach(var key in new[]{head,body,legs}) {
            BunnyContentKey.Validate(key,nameof(key),"BunnyArmorSet");
        }
        if(string.IsNullOrWhiteSpace(text)||text.Length>512||text.Any(char.IsControl))throw new ArgumentException("Texto do conjunto invalido",nameof(text));
        Head=head;Body=body;Legs=legs;Text=text;
    }
    /// <summary>
    /// Usa uma chave relativa registrada no catálogo do próprio mod. O Terraria
    /// guarda LocalizedText, que acompanha a troca de idioma sem recriar o conjunto.
    /// O registro aguarda a chave existir; não captura um texto provisório sem dono.
    /// </summary>
    public BunnyArmorSet UseLocalizedText(string key)
    {
        if (string.IsNullOrEmpty(key) || key.Length > 160 ||
            key.Split('.').Any(part => part.Length == 0 || part.Length > 72 ||
                part.Any(c => !(char.IsAsciiLetterOrDigit(c) || c == '_'))))
            throw new ArgumentException("Chave relativa de localização inválida", nameof(key));
        LocalizedTextKey = key;
        return this;
    }
    internal string ToWire() => string.Join("|",Head,Body,Legs,Convert.ToHexString(Encoding.UTF8.GetBytes(Text)),Effects.WireEffects,Effects.WireImmunity)+
        (LocalizedTextKey.Length == 0 ? "" : "|" + LocalizedTextKey);
    internal string ToJson() => "{\"head\":"+BunnyArmorEffects.Quote(Head)+",\"body\":"+BunnyArmorEffects.Quote(Body)+",\"legs\":"+BunnyArmorEffects.Quote(Legs)+",\"text\":"+BunnyArmorEffects.Quote(Text)+",\"textKey\":"+BunnyArmorEffects.Quote(LocalizedTextKey)+",\"effects\":"+Effects.ToJson()+"}";
}
