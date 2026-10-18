using System;
using System.Linq;
using System.Reflection;

namespace BunnyLoader.Source;

/// <summary>A reusable predicate evaluated on the Mono worker against an immutable crafting snapshot.</summary>
public sealed class BunnyRecipeCondition
{
    public string Description { get; }
    private readonly Func<BunnyCraftContext,bool> predicate;
    public BunnyRecipeCondition(string description,Func<BunnyCraftContext,bool> predicate)
    {
        ArgumentNullException.ThrowIfNull(predicate);
        if(string.IsNullOrWhiteSpace(description)||description.Length>256||description.Any(char.IsControl))
            throw new ArgumentException("[CSharp][ERROR][RecipeCondition] Stage=Validate Expected=1..256PrintableCharacters Reason=InvalidDescription",nameof(description));
        Description=description;this.predicate=predicate;
    }
    public bool Evaluate(BunnyCraftContext context) { ArgumentNullException.ThrowIfNull(context);return predicate(context); }
    public BunnyRecipeCondition Not(string description) => new(description,c=>!Evaluate(c));
    public static BunnyRecipeCondition All(string description,params BunnyRecipeCondition[] conditions)
        => Combine(description,conditions,true);
    public static BunnyRecipeCondition Any(string description,params BunnyRecipeCondition[] conditions)
        => Combine(description,conditions,false);
    private static BunnyRecipeCondition Combine(string description,BunnyRecipeCondition[] conditions,bool all)
    {
        ArgumentNullException.ThrowIfNull(conditions);
        if(conditions.Length<1||conditions.Length>32||conditions.Any(c=>c==null))
            throw new ArgumentException("Expected 1..32 non-null conditions",nameof(conditions));
        var snapshot=(BunnyRecipeCondition[])conditions.Clone();
        return new(description,c=>Evaluate(snapshot,c,all));
    }
    private static bool Evaluate(BunnyRecipeCondition[] conditions,BunnyCraftContext context,bool all)
    {
        foreach(var condition in conditions)if(condition.Evaluate(context)!=all)return !all;
        return all;
    }
    public static BunnyRecipeCondition Day { get; } = new("Day",c=>c.IsDay);
    public static BunnyRecipeCondition Night { get; } = new("Night",c=>!c.IsDay);
    public static BunnyRecipeCondition InDesert { get; } = new("Desert",c=>c.InDesert);
    public static BunnyRecipeCondition NearWater { get; } = new("Water or sink nearby",c=>c.NearWater);
    public static BunnyRecipeCondition InSnow { get; } = new("Snow",c=>c.InSnow);
}

/// <summary>Class-based recipe hooks. Override only the hooks needed; inherited overrides are supported.
/// Instances and captured state follow normal C# reference semantics, including recipe clones.</summary>
public abstract class BunnyRecipeBehavior
{
    public virtual string ConditionDescription => GetType().Name;
    public virtual bool IsAvailable(BunnyCraftContext context) => true;
    public virtual int ConsumeIngredient(BunnyCraftContext context,int ingredientType,int amount) => amount;
    public virtual void OnCraft(BunnyCraftContext context) { }
    internal int CallbackMask()
    {
        int mask=0;
        var names=new[]{nameof(IsAvailable),nameof(ConsumeIngredient),nameof(OnCraft)};
        for(int i=0;i<names.Length;i++)
        {
            var parameters=i==1?new[]{typeof(BunnyCraftContext),typeof(int),typeof(int)}:new[]{typeof(BunnyCraftContext)};
            var method=GetType().GetMethod(names[i],BindingFlags.Public|BindingFlags.Instance,null,parameters,null);
            if(method==null||method.GetBaseDefinition().DeclaringType!=typeof(BunnyRecipeBehavior))
                throw new InvalidOperationException("[CSharp][ERROR][RecipeBehavior] Class="+GetType().FullName+" Method="+names[i]+" Stage=Bind Reason=HiddenOrIncompatibleHook");
            if(method.DeclaringType!=typeof(BunnyRecipeBehavior))mask|=1<<i;
        }
        if(mask==0)throw new InvalidOperationException("[CSharp][ERROR][RecipeBehavior] Class="+GetType().FullName+" Stage=Bind Reason=NoOverriddenHooks");
        return mask;
    }
}

public enum BunnyRecipeStage { Availability,Consumption,Craft }
