using System;
using System.Linq;

namespace BunnyLoader.Source;

internal static class BunnyRarityReference
{
    internal static string Resolve(Type owner, Type rarity)
    {
        if (rarity.Assembly != owner.Assembly ||
            !typeof(BunnyRarity).IsAssignableFrom(rarity) ||
            !rarity.IsClass || rarity.IsAbstract || rarity.ContainsGenericParameters ||
            rarity.Name.Length == 0 || rarity.Name.Length > 72 ||
            !rarity.Name.All(c => char.IsAsciiLetterOrDigit(c) || c == '_'))
            throw new ArgumentException("Expected a concrete registered BunnyRarity in the item's assembly");
        return rarity.Name;
    }
}
