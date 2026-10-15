using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;

namespace BunnyLoader.Source;

/// <summary>Item geral: os defaults sao aplicados ao Item real pelo host, sem callback Mono por frame.</summary>
public abstract class BunnyItem
{
    public string DisplayName { get; set; } = "";
    public string Texture { get; set; } = "";
    public string Tooltip { get; set; } = "";
    public Dictionary<string,string> TooltipTranslations { get; } = new(StringComparer.Ordinal);
    // Zero deixa o host inferir o tamanho pela textura, como o ModItem JS.
    public int Width { get; set; }
    public int Height { get; set; }
    public int MaxStack { get; set; } = 1;
    public int Value { get; set; }
    public int Rare { get; set; }
    public int Defense { get => Behavior.Defense; set => Behavior.Defense=value; }
    public int Damage { get; set; }
    public int Crit { get; set; }
    public float KnockBack { get; set; }
    public int UseTime { get; set; }
    public int UseAnimation { get; set; }
    public int UseStyle { get; set; }
    public bool Melee { get; set; }
    public bool NoMelee { get; set; }
    public bool AutoReuse { get; set; }
    public bool UseTurn { get; set; }
    public bool Consumable { get; set; }
    public int Pick { get; set; }
    // Unidade interna Terraria: Axe=30 mostra 150% no tooltip (30 * 5).
    public int Axe { get; set; }
    public int Hammer { get; set; }
    public bool Accessory { get; set; }
    public bool Vanity { get; set; }
    public BunnyItemBehavior Behavior { get; } = new();
    public string BehaviorDefinition => Behavior.ToJson();
    public string NativeAccessoryEffectsDefinition => Behavior.NativeAccessoryEffectsDefinition;
    /// <summary>Descritor nativo de atributos, imunidades e conjunto de armadura.</summary>
    public string NativeArmorDefinition => Behavior.NativeArmorDefinition;
    /// <summary>Nome de um campo SoundID, como Item1. Vazio deixa o item sem som de uso.</summary>
    public string UseSound { get; set; } = "";
    /// <summary>Descritor de colocacao resolvido pelo nucleo; vazio preserva itens antigos.</summary>
    public string PlacementDefinition { get; private set; } = "";
    /// <summary>Defaults do Bunny original: coloca um tile vanilla com o estilo indicado.</summary>
    public void DefaultToPlaceableTile(int vanillaTileId, int style = 0)
    {
        if(vanillaTileId<0 || vanillaTileId>=754)throw new ArgumentOutOfRangeException(nameof(vanillaTileId));
        Placeable("v"+vanillaTileId.ToString(CultureInfo.InvariantCulture),style);
    }
    /// <summary>Nome local ou UID/Nome de tile C# ou JS no registro nativo compartilhado.</summary>
    public void DefaultToPlaceableTile(string modTileName, int style = 0)
    {
        BunnyContentKey.Validate(modTileName,nameof(modTileName),"BunnyItem.DefaultToPlaceableTile");
        Placeable("m"+modTileName,style);
    }
    private void Placeable(string token,int style)
    {
        if(style<0||style>32767)throw new ArgumentOutOfRangeException(nameof(style));
        PlacementDefinition=token+":"+style.ToString(CultureInfo.InvariantCulture);
        UseStyle=1;UseAnimation=15;UseTime=10;MaxStack=9999;
        UseTurn=true;AutoReuse=true;Consumable=true;
    }
    public abstract void SetDefaults();
    public virtual void AddRecipes() { }
    private readonly List<string> recipes = new();
    public IReadOnlyList<string> RecipeDefinitions => recipes;
    public BunnyRecipe CreateRecipe(int stack = 1) => new(this, stack);
    internal void RecordRecipe(string definition)
    {
        if (recipes.Count >= 16) throw new InvalidOperationException(GetType().Name+": maximo 16 receitas por item");
        recipes.Add(definition);
    }
}

/// <summary>Receita declarada em C# e instalada pelo host na thread do Terraria.</summary>
public sealed class BunnyRecipe
{
    private readonly BunnyItem owner;
    private readonly int stack;
    private string station = "-1";
    private bool registered;
    private readonly List<string> ingredients = new();
    internal BunnyRecipe(BunnyItem owner, int stack)
    { this.owner=owner; this.stack=Amount(stack); }
    public BunnyRecipe AddIngredient(int vanillaItemId, int count = 1)
    {
        if (vanillaItemId<=0 || vanillaItemId>=6196) throw new ArgumentOutOfRangeException(nameof(vanillaItemId));
        return Ingredient("v"+vanillaItemId.ToString(CultureInfo.InvariantCulture),count);
    }
    /// <summary>Nome de classe local ou chave UID/Nome de um item de outro mod carregado.</summary>
    public BunnyRecipe AddIngredient(string modItemName, int count = 1)
    {
        BunnyContentKey.Validate(modItemName,nameof(modItemName),"BunnyRecipe.AddIngredient");
        return Ingredient("m"+modItemName,count);
    }
    private BunnyRecipe Ingredient(string key,int count)
    {
        Mutable();
        if (ingredients.Count>=15) throw new InvalidOperationException("Receita excede 15 ingredientes");
        ingredients.Add(key+":"+Amount(count).ToString(CultureInfo.InvariantCulture)); return this;
    }
    /// <summary>Uma estacao vanilla, como WorkBenches=18; omitir permite criar em qualquer lugar.</summary>
    public BunnyRecipe AddTile(int vanillaTileId)
    {
        Mutable();
        if (station!="-1") throw new InvalidOperationException("Receita ja possui estacao");
        if (vanillaTileId<0 || vanillaTileId>=754) throw new ArgumentOutOfRangeException(nameof(vanillaTileId));
        station=vanillaTileId.ToString(CultureInfo.InvariantCulture); return this;
    }
    /// <summary>Estacao de mod: nome local ou UID/Nome; resolvida no registro nativo compartilhado.</summary>
    public BunnyRecipe AddTile(string modTileName)
    {
        Mutable();
        if(station!="-1")throw new InvalidOperationException("Receita ja possui estacao");
        BunnyContentKey.Validate(modTileName,nameof(modTileName),"BunnyRecipe.AddTile");
        station="m"+modTileName;return this;
    }
    public void Register()
    {
        Mutable();
        if (ingredients.Count==0) throw new InvalidOperationException("Receita sem ingredientes");
        if (stack>owner.MaxStack) throw new InvalidOperationException("Resultado excede MaxStack de "+owner.GetType().Name);
        // Nao e um sucesso simulado: o descritor entra nos defaults transferidos
        // ao host. O nucleo resolve IDs, registra Recipe e confirma seus lookups.
        owner.RecordRecipe(stack.ToString(CultureInfo.InvariantCulture)+","+
            station+","+string.Join(",",ingredients));
        registered=true;
    }
    private void Mutable() { if(registered) throw new InvalidOperationException("Receita ja registrada"); }
    private static int Amount(int n) => n>0 && n<=9999 ? n : throw new ArgumentOutOfRangeException(nameof(n));
}

public static class BunnyVanillaItems { public const int DirtBlock=2; }
public static class BunnyVanillaTiles { public const int WorkBenches=18; }

internal static class BunnyContentKey
{
    internal static void Validate(string key,string parameter,string api)
    {
        string reason=string.IsNullOrWhiteSpace(key)?"EmptyIdentity":key.Length>160?"IdentityTooLong":
            key.Count(c=>c=='/')>1?"TooManySegments":key.Split('/').Any(part=>part.Length==0)?"EmptySegment":
            key.Any(c=>!(char.IsAsciiLetterOrDigit(c)||c=='_'||c=='-'||c=='/'))?"InvalidCharacter":"";
        if(reason.Length!=0)throw new ArgumentException("[CSharp][ERROR][ContentKey] API="+api+
            " Field="+parameter+" Stage=ValidateKey Expected=LocalNameOrModId/Name ActualLength="+(key?.Length??0)+
            " Reason="+reason+" Result=Rejected",parameter);
    }
}
