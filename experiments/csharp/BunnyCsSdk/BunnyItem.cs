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

public abstract class BunnyItem : BunnyShootSource
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
    /// <summary>
    /// Mesmo contrato de Item.DefaultToGolfBall(projid) no Android 301720.
    /// Use em SetDefaults, antes de sobrescrever os valores particulares do item.
    /// A referência ao projétil é resolvida pelo host, sem ID fixo de mod.
    /// </summary>
    public void DefaultToGolfBall(string projectile)
    {
        // Estes valores acompanham o helper real do Terraria: bola equipável,
        // sem gráfico de uso nem acerto direto; o projétil faz a simulação.
        Width=18; Height=20; UseStyle=1; UseTime=15; UseAnimation=15;
        Accessory=true; NoMelee=true; Rare=ItemRarityID.Green; Value=10000;
        UseSound="Item1";
        NativeDefaults.SetProjectile("shoot",projectile).SetFloat("shootSpeed",12)
            .SetBool("noUseGraphic",true).SetBool("hasVanityEffects",true);
    }
    public string DisplayName { get; set; } = "";
    public string Texture { get; set; } = "";
    public string Tooltip { get; set; } = "";
    public Dictionary<string,string> TooltipTranslations { get; } = new(StringComparer.Ordinal);
    // Zero deixa o host inferir o tamanho pela textura, como o ModItem JS.
    private bool nativeWallDefaults;
    private int wallIntegerOverrides,wallFlagOverrides;
    private int cloneIntegerOverrides,cloneFlagOverrides;
    private bool cloneKnockBackOverride,cloneUseSoundOverride;
    private string placement="";
    private void OverrideInteger(int index) { cloneIntegerOverrides|=1<<index;if(nativeWallDefaults)wallIntegerOverrides|=1<<index; }
    private void OverrideFlag(int index) { cloneFlagOverrides|=1<<index;if(nativeWallDefaults)wallFlagOverrides|=1<<index; }
    public int Width { get => width; set { width=value; OverrideInteger(0); } }
    private int width;
    private int wallHeight=0;
    public int Height { get => wallHeight; set { wallHeight=value; OverrideInteger(1); } }
    private int wallMaxStack=1;
    public int MaxStack { get => wallMaxStack; set { wallMaxStack=value; OverrideInteger(2); } }
    private int wallValue=0;
    public int Value { get => wallValue; set { wallValue=value; OverrideInteger(3); } }
    private int wallRare=0;
    public int Rare { get => wallRare; set { wallRare=value; OverrideInteger(4); } }
    public string ModRarityClassName { get; private set; } = "";
    public void SetRarity<T>() where T : BunnyRarity
        { ModRarityClassName = BunnyRarityReference.Resolve(GetType(), typeof(T)); OverrideInteger(4); }
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
    /// <summary>True quando SetDefaults atribuiu Defense, inclusive zero.</summary>
    public bool HasDefenseOverride => Behavior.HasDefenseOverride;
    public int Defense { get => Behavior.Defense; set => Behavior.Defense=value; }
    private int wallDamage=0;
    public int Damage { get => wallDamage; set { wallDamage=value; OverrideInteger(5); } }
    private int wallCrit=0;
    public int Crit { get => wallCrit; set { wallCrit=value; OverrideInteger(6); } }
    private float knockBack;
    public float KnockBack { get => knockBack; set { knockBack=value;cloneKnockBackOverride=true; } }
    private int wallUseTime=0;
    public int UseTime { get => wallUseTime; set { wallUseTime=value; OverrideInteger(7); } }
    private int wallUseAnimation=0;
    public int UseAnimation { get => wallUseAnimation; set { wallUseAnimation=value; OverrideInteger(8); } }
    private int wallUseStyle=0;
    public int UseStyle { get => wallUseStyle; set { wallUseStyle=value; OverrideInteger(9); } }
    private bool wallMelee;
    public bool Melee { get => wallMelee; set { wallMelee=value; OverrideFlag(0); } }
    private bool wallNoMelee;
    public bool NoMelee { get => wallNoMelee; set { wallNoMelee=value; OverrideFlag(1); } }
    private bool wallAutoReuse;
    public bool AutoReuse { get => wallAutoReuse; set { wallAutoReuse=value; OverrideFlag(2); } }
    private bool wallUseTurn;
    public bool UseTurn { get => wallUseTurn; set { wallUseTurn=value; OverrideFlag(3); } }
    private bool wallConsumable;
    public bool Consumable { get => wallConsumable; set { wallConsumable=value; OverrideFlag(4); } }
    private int wallPick=0;
    public int Pick { get => wallPick; set { wallPick=value; OverrideInteger(10); } }
    // Unidade interna Terraria: Axe=30 mostra 150% no tooltip (30 * 5).
    private int wallAxe=0;
    public int Axe { get => wallAxe; set { wallAxe=value; OverrideInteger(11); } }
    private int wallHammer=0;
    public int Hammer { get => wallHammer; set { wallHammer=value; OverrideInteger(12); } }
    private bool wallAccessory;
    public bool Accessory { get => wallAccessory; set { wallAccessory=value; OverrideFlag(5); } }
    private bool wallVanity;
    public bool Vanity { get => wallVanity; set { wallVanity=value; OverrideFlag(6); } }
    private string mount="";
    public string Mount {
        get=>mount;
        set {
            if(value.Length>0&&(value.Length>72||value.Any(c=>!char.IsAsciiLetterOrDigit(c)&&c!='_')))
                throw new ArgumentException("Invalid local BunnyMount name",nameof(value));
            mount=value;
        }
    }
    public BunnyItemBehavior Behavior { get; } = new();
    public string BehaviorDefinition => Behavior.ToJson();
    /// <summary>Colunas opcionais C1: somente atribuições explícitas substituem defaults clonados.</summary>
    public string NativeCloneOverridesDefinition => Behavior.HasVanillaClone
        ? "C1|"+cloneIntegerOverrides.ToString(CultureInfo.InvariantCulture)+"|"+
          cloneFlagOverrides.ToString(CultureInfo.InvariantCulture)+"|"+
          (cloneKnockBackOverride?"1":"0")+"|"+(cloneUseSoundOverride?"1":"0") : "";
    public string NativeAccessoryEffectsDefinition => Behavior.NativeAccessoryEffectsDefinition;
    /// <summary>Descritor nativo de atributos, imunidades e conjunto de armadura.</summary>
    public string NativeArmorDefinition => Behavior.NativeArmorDefinition;
    private string useSound="";
    /// <summary>Nome de um campo SoundID, como Item1. Vazio deixa o item sem som de uso.</summary>
    public string UseSound { get => useSound; set { useSound=value;cloneUseSoundOverride=true; } }
    /// <summary>Descritor de colocacao resolvido pelo nucleo; vazio preserva itens antigos.</summary>
    public string PlacementDefinition => placement + (nativeWallDefaults ? ":"+wallIntegerOverrides.ToString(CultureInfo.InvariantCulture)+":"+wallFlagOverrides.ToString(CultureInfo.InvariantCulture) : "");
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
    /// <summary>Coloca uma parede vanilla. O host valida o campo createWall e sua largura real.</summary>
    public void DefaultToPlaceableWall(int vanillaWallId) {
        if(vanillaWallId<=0||vanillaWallId>=367)throw new ArgumentOutOfRangeException(nameof(vanillaWallId));
        WallPlaceable("wv"+vanillaWallId.ToString(CultureInfo.InvariantCulture));
    }
    /// <summary>Parede local ou UID/Nome, inclusive de outro mod C# ou JS.</summary>
    public void DefaultToPlaceableWall(string modWallName) {
        BunnyContentKey.Validate(modWallName,nameof(modWallName),"BunnyItem.DefaultToPlaceableWall");
        WallPlaceable("wm"+modWallName);
    }
    // The Android helper owns its defaults. Only later assignments override them,
    // including assigning false or a value equal to the SDK initial value.
    private void WallPlaceable(string token) {
        wallMaxStack=9999;placement=token+":0";nativeWallDefaults=true;wallIntegerOverrides=0;wallFlagOverrides=0;
    }
    private void Placeable(string token,int style)
    {
        if(style<0||style>32767)throw new ArgumentOutOfRangeException(nameof(style));
        nativeWallDefaults=false;placement=token+":"+style.ToString(CultureInfo.InvariantCulture);
        UseStyle=1;UseAnimation=15;UseTime=10;MaxStack=9999;
        UseTurn=true;AutoReuse=true;Consumable=true;
    }
    public void DefaultToQuestFish()
    {
        QuestFish = true;
        // Item.DefaultToQuestFish do Android 301720: questItem=true,
        // width=height=26 e rare=Quest (-11). Flags IsQuestFish/weapon rack
        // pertencem ao AnglerQuestLoader; questItem é um campo da instância.
        Width = 26; Height = 26; Rare = ItemRarityID.Quest;
        NativeDefaults.SetBool("questItem",true);
        MaxStack = 1;
        Value = 0;
    }

    public virtual void ModifyTooltips(List<BunnyTooltipLine> lines) { }
    public virtual bool PreDrawTooltipLine(BunnyDrawableTooltipLine line) => true;

    public virtual bool OnUse(int playerIndex) => false;
    public virtual bool OnUse(int playerIndex, BunnyUseContext context) => OnUse(playerIndex);
    public virtual string OnUseRejectionReason(int playerIndex, BunnyUseContext context) => "";
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
