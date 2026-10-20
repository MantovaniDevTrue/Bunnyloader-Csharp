using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;

namespace BunnyLoader.Source;

[Flags]
public enum BunnyQuestCatchRules
{
    None = 0,
    InvertedGravity = 1,
    Uncommon = 2,
    InvertedGravityAndUncommon = InvertedGravity | Uncommon
}

/// <summary>Item geral: os defaults sao aplicados ao Item real pelo host, sem callback Mono por frame.</summary>
public sealed class BunnyQuestContext
{
    public int FishType { get; internal set; }
    public int PlayerIndex { get; internal set; } = -1;
    public bool Hardmode { get; internal set; }
    public bool InvertedGravity { get; internal set; }
    public bool Uncommon { get; internal set; }
}

public abstract class BunnyItem : BunnyRecipeSource
{
    public BunnyNativeDefaults NativeDefaults { get; } = new();
    public BunnyHoldout? Holdout { get; set; }
    /// <summary>Som próprio do mod; tem precedência sobre o SoundID vanilla.</summary>
    public BunnySoundStyle? CustomUseSound { get; set; }
    public string NativeDefaultsDefinition => NativeDefaults.Definition;
    /// <summary>Valor interno equivalente a Item.buyPrice; as quantidades são convertidas em cobre.</summary>
    public static int BuyPrice(int platinum=0,int gold=0,int silver=0,int copper=0)
    {
        if(platinum<0)throw new ArgumentOutOfRangeException(nameof(platinum),platinum,"Moedas não podem ser negativas");
        if(gold<0)throw new ArgumentOutOfRangeException(nameof(gold),gold,"Moedas não podem ser negativas");
        if(silver<0)throw new ArgumentOutOfRangeException(nameof(silver),silver,"Moedas não podem ser negativas");
        if(copper<0)throw new ArgumentOutOfRangeException(nameof(copper),copper,"Moedas não podem ser negativas");
        return checked(platinum*1000000+gold*10000+silver*100+copper);
    }
    /// <summary>Item.value é o preço-base: sellPrice devolve 5x a venda desejada, como o Terraria.</summary>
    public static int SellPrice(int platinum=0,int gold=0,int silver=0,int copper=0)
        => checked(BuyPrice(platinum,gold,silver,copper)*5);
    internal override int RecipeStackLimit => MaxStack;
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
    public bool QuestFish { get; set; }
    public bool QuestAvailableInPreHardmode { get; set; } = true;
    public bool QuestAvailableInHardmode { get; set; } = true;
    public string QuestDescription { get; set; } = "";
    public string QuestCatchLocation { get; set; } = "";
    public Dictionary<string,string> QuestDescriptionTranslations { get; } = new(StringComparer.Ordinal);
    public Dictionary<string,string> QuestCatchLocationTranslations { get; } = new(StringComparer.Ordinal);
    public BunnyQuestCatchRules QuestCatchRules { get; set; }
    public Func<BunnyQuestContext,bool>? QuestAvailableWhen { get; set; }
    public Func<BunnyQuestContext,bool>? QuestCanCatchWhen { get; set; }
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
    public void DefaultToQuestFish()
    {
        QuestFish = true;
        MaxStack = 1;
        Value = 0;
    }

    public abstract void SetDefaults();
}

/// <summary>Receita declarada em C# e instalada pelo host na thread do Terraria.</summary>
public sealed class BunnyRecipe
{
    private readonly BunnyRecipeSource owner;
    private readonly int stack;
    private string station = "-1";
    private bool registered;
    private readonly List<string> ingredients = new();
    private readonly List<BunnyRecipeCondition> conditions=new();
    private readonly List<Func<BunnyCraftContext,int,int,int>> consume=new();
    private readonly List<Action<BunnyCraftContext>> crafted=new();
    public BunnyRecipe AddCondition(string description,Func<BunnyCraftContext,bool> predicate)
        => AddCondition(new BunnyRecipeCondition(description,predicate));
    public BunnyRecipe AddCondition(BunnyRecipeCondition condition)
    {
        Mutable();ArgumentNullException.ThrowIfNull(condition);
        if(conditions.Count>=32)throw new InvalidOperationException("Condition limit 32");
        conditions.Add(condition);return this;
    }
    public bool RemoveCondition(BunnyRecipeCondition condition)
    { Mutable();ArgumentNullException.ThrowIfNull(condition);return conditions.Remove(condition); }
    /// <summary>Binds actual virtual overrides to the same native hooks as delegate callbacks.</summary>
    public BunnyRecipe AddBehavior(BunnyRecipeBehavior behavior)
    {
        Mutable();ArgumentNullException.ThrowIfNull(behavior);int mask=behavior.CallbackMask();
        // Validate every part before changing this recipe; failed bindings leave no partial callbacks.
        BunnyRecipeCondition? condition=(mask&1)!=0?new(behavior.ConditionDescription,behavior.IsAvailable):null;
        if((condition!=null&&conditions.Count>=32)||((mask&2)!=0&&consume.Count>=16)||((mask&4)!=0&&crafted.Count>=16))
            throw new InvalidOperationException("[CSharp][ERROR][RecipeBehavior] Class="+behavior.GetType().FullName+" Stage=Bind Reason=CallbackCapacityExceeded");
        if(condition!=null)conditions.Add(condition);
        if((mask&2)!=0)consume.Add(behavior.ConsumeIngredient);
        if((mask&4)!=0)crafted.Add(behavior.OnCraft);
        return this;
    }
    public BunnyRecipe AddConsumeIngredientCallback(Func<BunnyCraftContext,int,int,int> callback)
    { Mutable();ArgumentNullException.ThrowIfNull(callback);if(consume.Count>=16)throw new InvalidOperationException("Callback limit 16");consume.Add(callback);return this; }
    public BunnyRecipe AddOnCraftCallback(Action<BunnyCraftContext> callback)
    { Mutable();ArgumentNullException.ThrowIfNull(callback);if(crafted.Count>=16)throw new InvalidOperationException("Callback limit 16");crafted.Add(callback);return this; }
    internal bool IsAvailable(BunnyCraftContext context)
    { foreach(var condition in conditions)if(!condition.Evaluate(context))return false;return true; }
    internal int Consume(BunnyCraftContext context,int type,int amount)
    { int limit=Math.Max(amount,context.AvailableIngredientCount);foreach(var callback in consume)amount=Math.Clamp(callback(context,type,amount),0,limit);return amount; }
    internal void Crafted(BunnyCraftContext context) { foreach(var callback in crafted)callback(context); }

    internal BunnyRecipe(BunnyRecipeSource owner, int stack)
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
    /// <summary>Item local identificado pela classe; UID/Nome continua disponivel para dependencias externas.</summary>
    public BunnyRecipe AddIngredient<TItem>(int count = 1) where TItem : class
        => AddIngredient(BunnyContentKey.TypeName(typeof(TItem),false,"BunnyRecipe.AddIngredient<TItem>"),count);
    /// <summary>Um ingrediente alternativo de Terraria.ID.RecipeGroups, como Wood ou IronBar.
    /// O nucleo resolve o grupo e chama Recipe.RequireGroup; nao escolhe um item fixo.</summary>
    public BunnyRecipe AddNativeRecipeGroup(string groupField, int count = 1)
    {
        string key=NativeGroupKey(groupField,"BunnyRecipe.AddNativeRecipeGroup");
        if(ingredients.Any(token=>token.StartsWith(key+":",StringComparison.Ordinal)))
            throw new InvalidOperationException("[CSharp][ERROR][Recipe] API=BunnyRecipe.AddNativeRecipeGroup Group="+groupField+
                " Stage=ValidateGroup Reason=DuplicateRecipeGroup Result=Rejected");
        return Ingredient(key,count);
    }
    /// <summary>Snapshot de um grupo personalizado; membros são resolvidos por identidade no núcleo.</summary>
    public BunnyRecipe AddRecipeGroup(BunnyRecipeGroup group, int count = 1)
    {
        ArgumentNullException.ThrowIfNull(group);
        string key=group.Definition();
        if(ingredients.Any(token=>token.StartsWith("h"+group.Name+"~",StringComparison.Ordinal)))
            throw new InvalidOperationException("Duplicate recipe group: "+group.Name);
        return Ingredient(key,count);
    }
    private static string NativeGroupKey(string groupField,string api)
    {
        if(string.IsNullOrEmpty(groupField)||groupField.Length>72||
            !char.IsAsciiLetter(groupField[0])||groupField.Any(c=>!(char.IsAsciiLetterOrDigit(c)||c=='_')))
            throw new ArgumentException("[CSharp][ERROR][Recipe] API="+api+" Stage=ValidateGroup Expected=RecipeGroupsField Actual="+
                groupField+" Reason=InvalidGroupField Result=Rejected",nameof(groupField));
        return "g"+groupField;
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
    /// <summary>Estacao local identificada pela classe BunnyTile, sem resolver IDs na compilacao.</summary>
    public BunnyRecipe AddTile<TTile>() where TTile : class
        => AddTile(BunnyContentKey.TypeName(typeof(TTile),true,"BunnyRecipe.AddTile<TTile>"));
    /// <summary>Copia independente dos descritores desta receita, inclusive apos Register.
    /// A copia preserva resultado/estacao e so sera instalada ao chamar seu Register.</summary>
    public BunnyRecipe Clone()
    {
        var copy=new BunnyRecipe(owner,stack){station=station};
        copy.ingredients.AddRange(ingredients);copy.conditions.AddRange(conditions);copy.consume.AddRange(consume);copy.crafted.AddRange(crafted);return copy;
    }
    public bool RemoveIngredient(int vanillaItemId)
    {
        if(vanillaItemId<=0||vanillaItemId>=6196)throw new ArgumentOutOfRangeException(nameof(vanillaItemId));
        return RemoveKey("v"+vanillaItemId.ToString(CultureInfo.InvariantCulture));
    }
    public bool RemoveIngredient(string modItemName)
    {
        BunnyContentKey.Validate(modItemName,nameof(modItemName),"BunnyRecipe.RemoveIngredient");
        return RemoveKey("m"+modItemName);
    }
    public bool RemoveIngredient<TItem>() where TItem : class
        => RemoveIngredient(BunnyContentKey.TypeName(typeof(TItem),false,"BunnyRecipe.RemoveIngredient<TItem>"));
    public bool RemoveRecipeGroup(string localGroupName)
    {
        Mutable();BunnyContentKey.Validate(localGroupName,nameof(localGroupName),"BunnyRecipe.RemoveRecipeGroup");
        if(localGroupName.Contains('/'))throw new ArgumentException("Expected a local group name",nameof(localGroupName));
        return ingredients.RemoveAll(token=>token.StartsWith("h"+localGroupName+"~",StringComparison.Ordinal))>0;
    }
    public bool RemoveNativeRecipeGroup(string groupField)
        => RemoveKey(NativeGroupKey(groupField,"BunnyRecipe.RemoveNativeRecipeGroup"));
    private bool RemoveKey(string key)
    { Mutable();return ingredients.RemoveAll(token=>token.StartsWith(key+":",StringComparison.Ordinal))>0; }
    public void Register()
    {
        Mutable();
        if (ingredients.Count==0) throw new InvalidOperationException("Receita sem ingredientes");
        if (stack>owner.RecipeStackLimit) throw new InvalidOperationException("Resultado excede MaxStack de "+owner.GetType().Name);
        // Nao e um sucesso simulado: o descritor entra nos defaults transferidos
        // ao host. O nucleo resolve IDs, registra Recipe e confirma seus lookups.
        int callbacks=(conditions.Count>0?1:0)|(consume.Count>0?2:0)|(crafted.Count>0?4:0);
        owner.RecordRecipe(stack.ToString(CultureInfo.InvariantCulture)+","+
            station+","+string.Join(",",ingredients)+(callbacks==0?"":"@"+callbacks.ToString(CultureInfo.InvariantCulture)));
        BunnyRecipeRuntime.Capture(this);
        registered=true;
    }
    private void Mutable() { if(registered) throw new InvalidOperationException("Receita ja registrada"); }
    private static int Amount(int n) => n>0 && n<=9999 ? n : throw new ArgumentOutOfRangeException(nameof(n));
}

public static class BunnyVanillaItems { public const int DirtBlock=2; }
public static class BunnyVanillaTiles { public const int WorkBenches=18; }

internal static class BunnyContentKey
{
    internal static string TypeName(Type type,bool tile,string api)
    {
        var declaration=Attribute.GetCustomAttribute(type,typeof(BunnyContentAttribute),false) as BunnyContentAttribute;
        bool supported=tile?typeof(BunnyTile).IsAssignableFrom(type)||declaration?.Kind==BunnyContentKind.Tile:
            typeof(BunnyItem).IsAssignableFrom(type)||typeof(BunnyWeapon).IsAssignableFrom(type)||
            typeof(BunnyAccessory).IsAssignableFrom(type)||typeof(BunnyBuffPotion).IsAssignableFrom(type)||
            declaration?.Kind is BunnyContentKind.Item or BunnyContentKind.Weapon or BunnyContentKind.Accessory or BunnyContentKind.BuffPotion;
        if(!supported||type.IsAbstract||type.IsGenericType)
            throw new ArgumentException("[CSharp][ERROR][ContentKey] API="+api+
                " Stage=ValidateType Expected="+(tile?"ConcreteTileContent":"ConcreteItemContent")+
                " Actual="+type.FullName+" Reason=UnsupportedContentType Result=Rejected");
        Validate(type.Name,"contentType",api);return type.Name;
    }
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
