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
    // Zero deixa o host inferir o tamanho pela textura, como o ModItem JS.
    public int Width { get; set; }
    public int Height { get; set; }
    public int MaxStack { get; set; } = 1;
    public int Value { get; set; }
    public int Rare { get; set; }
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
    /// <summary>Nome de um campo SoundID, como Item1. Vazio deixa o item sem som de uso.</summary>
    public string UseSound { get; set; } = "";
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
    private int station = -1;
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
        if (string.IsNullOrWhiteSpace(modItemName) || modItemName.Length>160 ||
            modItemName.Any(c=>!(char.IsAsciiLetterOrDigit(c)||c=='_'||c=='-'||c=='/')))
            throw new ArgumentException("Nome/chave de ingrediente invalido",nameof(modItemName));
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
        if (station!=-1) throw new InvalidOperationException("Receita ja possui estacao");
        if (vanillaTileId<0 || vanillaTileId>=2195) throw new ArgumentOutOfRangeException(nameof(vanillaTileId));
        station=vanillaTileId; return this;
    }
    public void Register()
    {
        Mutable();
        if (ingredients.Count==0) throw new InvalidOperationException("Receita sem ingredientes");
        if (stack>owner.MaxStack) throw new InvalidOperationException("Resultado excede MaxStack de "+owner.GetType().Name);
        // Nao e um sucesso simulado: o descritor entra nos defaults transferidos
        // ao host. O host resolve IDs, registra ModRecipe e confirma o indice.
        owner.RecordRecipe(stack.ToString(CultureInfo.InvariantCulture)+","+
            station.ToString(CultureInfo.InvariantCulture)+","+string.Join(",",ingredients));
        registered=true;
    }
    private void Mutable() { if(registered) throw new InvalidOperationException("Receita ja registrada"); }
    private static int Amount(int n) => n>0 && n<=9999 ? n : throw new ArgumentOutOfRangeException(nameof(n));
}

public static class BunnyVanillaItems { public const int DirtBlock=2; }
public static class BunnyVanillaTiles { public const int WorkBenches=18; }
