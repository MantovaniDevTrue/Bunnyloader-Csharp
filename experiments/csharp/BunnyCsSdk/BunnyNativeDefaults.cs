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
    /// <summary>
    /// Campo inteiro com ID simbólico. ItemRarityID usa a tabela pública do tModLoader
    /// implementada pelo host nativo, pois essa classe não existe no Android.
    /// As demais famílias resolvem constantes reais do jogo com tipo validado.
    /// </summary>
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
    /// <summary>Flag da tabela nativa ItemID.Sets, indexada pelo tipo registrado do item.</summary>
    public BunnyNativeDefaults SetItemStaticBool(string name,bool value)
    {
        if(name!="GamepadWholeScreenUseRange"&&name!="LockOnIgnoresCollision")throw new ArgumentException("ItemID.Sets Boolean não suportado",nameof(name));
        return Set("ItemSet_"+name,"b:"+(value?"1":"0"));
    }
    /// <summary>Quantidade de slots exigidos pela arma de summon, na tabela real do jogo.</summary>
    public BunnyNativeDefaults SetStaffMinionSlotsRequired(int value)
    {
        if(value<0||value>100)throw new ArgumentOutOfRangeException(nameof(value));
        return Set("ItemSet_StaffMinionSlotsRequired","i:"+value.ToString(CultureInfo.InvariantCulture));
    }
    /// <summary>Flag de ProjectileID.Sets, instalada antes do SetDefaults nativo do projétil.</summary>
    public BunnyNativeDefaults SetProjectileStaticBool(string name,bool value)
    {
        if(name!="IsAGolfBall")throw new ArgumentException("ProjectileID.Sets Boolean não suportado",nameof(name));
        return Set("ProjectileSet_"+name,"b:"+(value?"1":"0"));
    }
    /// <summary>
    /// Política do rastro nativo. TrailCacheLength (0..128) controla o tamanho
    /// de oldPos/oldRot; TrailingMode (0..3) escolhe a atualização do histórico.
    /// A tabela é escrita antes da criação desses arrays pelo Terraria.
    /// </summary>
    public BunnyNativeDefaults SetProjectileStaticInt(string name,int value)
    {
        int maximum=name=="TrailCacheLength"?128:name=="TrailingMode"?3:
            throw new ArgumentException("ProjectileID.Sets Int32 não suportado",nameof(name));
        if(value<0||value>maximum)throw new ArgumentOutOfRangeException(nameof(value));
        return Set("ProjectileSet_"+name,"i:"+value.ToString(CultureInfo.InvariantCulture));
    }
    /// <summary>Usa o LegacySoundStyle real registrado em Terraria.ID.SoundID.</summary>
    public BunnyNativeDefaults SetVanillaUseSound(string name) {Name(name);return Set("UseSound","s:"+name);}
    public string Definition=>string.Join(";",System.Linq.Enumerable.Select(values,p=>p.Key+":"+p.Value));
}
