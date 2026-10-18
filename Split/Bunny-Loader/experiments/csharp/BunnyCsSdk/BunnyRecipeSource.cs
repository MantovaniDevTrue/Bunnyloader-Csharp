using System;
using System.Collections.Generic;

namespace BunnyLoader.Source;

/// <summary>Shared recipe authoring for registered Bunny item categories.
/// Derive from BunnyItem, BunnyWeapon, BunnyAccessory or BunnyBuffPotion to register content.</summary>
public abstract class BunnyRecipeSource
{
    private readonly List<string> recipes=new();
    public IReadOnlyList<string> RecipeDefinitions => recipes.AsReadOnly();
    internal virtual int RecipeStackLimit => 9999; // Actual native result limit is also checked before registration.
    public virtual void AddRecipes() { }
    public BunnyRecipe CreateRecipe(int stack=1) => new(this,stack);
    internal void RecordRecipe(string definition)
    {
        if(recipes.Count>=16)throw new InvalidOperationException("[CSharp][ERROR][RecipeLoader] Class="+GetType().FullName+" Stage=Register Expected=AtMost16Recipes Actual="+(recipes.Count+1)+" Reason=RecipeCapacityExceeded");
        recipes.Add(definition);
    }
}
