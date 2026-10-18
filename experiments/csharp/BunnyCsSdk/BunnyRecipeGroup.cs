using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;

namespace BunnyLoader.Source;

/// <summary>Grupo de alternativas instalado em Terraria.RecipeGroup pelo núcleo nativo.
/// Identidade é local ao mod; uma referência entre mods usa UID/Nome.</summary>
public sealed class BunnyRecipeGroup
{
    public string Name { get; }
    public string DisplayName { get; }
    private readonly List<string> members = new();
    public BunnyRecipeGroup(string name, string displayName)
    {
        BunnyContentKey.Validate(name, nameof(name), "BunnyRecipeGroup");
        if (name.Length>72 || name.Contains('/')) throw new ArgumentException("A definition must use a local group name", nameof(name));
        if (string.IsNullOrWhiteSpace(displayName) || Encoding.UTF8.GetByteCount(displayName)>256 ||
            displayName.Any(char.IsControl)) throw new ArgumentException("Expected a group description of 1..256 UTF-8 bytes", nameof(displayName));
        Name=name; DisplayName=displayName;
    }
    public BunnyRecipeGroup AddItem(int vanillaItemId)
    {
        if (vanillaItemId<1 || vanillaItemId>=6196) throw new ArgumentOutOfRangeException(nameof(vanillaItemId));
        return Add("v"+vanillaItemId.ToString(CultureInfo.InvariantCulture));
    }
    public BunnyRecipeGroup AddItem(string modItemName)
    {
        BunnyContentKey.Validate(modItemName, nameof(modItemName), "BunnyRecipeGroup.AddItem");
        return Add("m"+modItemName);
    }
    public BunnyRecipeGroup AddItem<TItem>() where TItem : class
        => AddItem(BunnyContentKey.TypeName(typeof(TItem),false,"BunnyRecipeGroup.AddItem<TItem>"));
    private BunnyRecipeGroup Add(string key)
    {
        if (members.Contains(key)) throw new InvalidOperationException("[CSharp][ERROR][RecipeGroup] Group="+Name+" Member="+key+" Reason=DuplicateMember");
        if (members.Count>=256) throw new InvalidOperationException("Recipe groups support at most 256 members");
        members.Add(key); return this;
    }
    internal string Definition()
    {
        if (members.Count==0) throw new InvalidOperationException("[CSharp][ERROR][RecipeGroup] Group="+Name+" Reason=EmptyGroup");
        // Snapshot: changing this builder later cannot mutate a registered recipe.
        return "h"+Name+"~"+Convert.ToHexString(Encoding.UTF8.GetBytes(DisplayName))+"~"+string.Join("+",members);
    }
}
