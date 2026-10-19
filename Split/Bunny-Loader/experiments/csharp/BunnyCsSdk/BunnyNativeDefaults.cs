using System;
using System.Collections.Generic;
using System.Globalization;

namespace BunnyLoader.Source;

/// <summary>
/// Defaults primitivos do Item/Projectile real. O host resolve e valida cada
/// campo antes de publicar o lote; nunca são offsets fornecidos pelo mod.
/// Use em SetDefaults. Não é uma API para alterar entidades a cada frame.
/// </summary>
public sealed class BunnyNativeDefaults
{
    private readonly SortedDictionary<string,string> values=new(StringComparer.Ordinal);
    private BunnyNativeDefaults Set(string field,string value)
    {
        Name(field);
        if(!values.ContainsKey(field)&&values.Count>=64)throw new InvalidOperationException("NativeDefaults: limite de 64 campos por conteúdo");
        values[field]=value;return this;
    }
    private static void Name(string value)
    {
        if(string.IsNullOrEmpty(value)||value.Length>72)throw new ArgumentException("Nome de campo/ID nativo inválido",nameof(value));
        foreach(char c in value)if(!char.IsAsciiLetterOrDigit(c)&&c!='_')throw new ArgumentException("Nome de campo/ID nativo inválido: "+value);
    }
    public BunnyNativeDefaults SetInt(string field,int value)=>Set(field,"i:"+value.ToString(CultureInfo.InvariantCulture));
    public BunnyNativeDefaults SetBool(string field,bool value)=>Set(field,"b:"+(value?"1":"0"));
    public BunnyNativeDefaults SetFloat(string field,float value)
    {
        if(!float.IsFinite(value))throw new ArgumentOutOfRangeException(nameof(value),"NativeDefaults requer Single finito");
        return Set(field,"f:"+value.ToString("R",CultureInfo.InvariantCulture));
    }
    /// <summary>Campo inteiro com ID vanilla resolvido pelo nome e pelo tipo real da constante.</summary>
    public BunnyNativeDefaults SetId(string field,string idClass,string name)
    {
        if(idClass!="AmmoID"&&idClass!="ProjectileID"&&idClass!="ItemID"&&idClass!="ItemUseStyleID"&&idClass!="ItemRarityID")
            throw new ArgumentException("Família de IDs não suportada: "+idClass,nameof(idClass));
        Name(name);return Set(field,"d:"+idClass+":"+name);
    }
    /// <summary>Referência local ou UID/Nome, resolvida no registro nativo comum a C# e JS.</summary>
    public BunnyNativeDefaults SetProjectile(string field,string key)
    {
        BunnyContentKey.Validate(key,nameof(key),"BunnyNativeDefaults.SetProjectile");return Set(field,"p:"+key);
    }
    public string Definition=>string.Join(";",System.Linq.Enumerable.Select(values,p=>p.Key+":"+p.Value));
}
