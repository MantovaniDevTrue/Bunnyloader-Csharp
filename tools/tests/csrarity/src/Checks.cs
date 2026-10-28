using System;
using CsRarity.Content.Items;
using CsRarity.Content.Rarities;
using log4net;
using Microsoft.Xna.Framework.Graphics;
using Terraria;
using Terraria.ID;
using Terraria.ModLoader;

namespace CsRarity;

public static class Checks
{
    const int Legendary = 81, BrokenPrefix = 39;

    static int pass, fail, frames;
    static bool done;

    static ILog Log => ModContent.GetInstance<CsRarityMod>().Logger;

    public static void Reset()
    {
        frames = pass = fail = 0;
        done = false;
    }

    public static void Frame(Player p)
    {
        if (done || ++frames != 120) return;
        done = true;
        try
        {
            Run();
            Give(p, ModContent.ItemType<PlainBlade>());
            Give(p, ModContent.ItemType<ShinyBlade>());
        }
        catch (Exception e) { Report("run", false, e.ToString()); }
        Log.Info($"csrarity END pass={pass} fail={fail} (look at the two blades: the name is green for Plain and pink for Shiny in the tooltip, " +
                 "on the floor under the cursor and in the text that rises when you pick one up)");
    }

    static void Run()
    {
        var plain = ModContent.GetInstance<Plain>();
        var shiny = ModContent.GetInstance<Shiny>();
        var wild = ModContent.GetInstance<Wild>();
        var broken = ModContent.GetInstance<Broken>();

        Report("the types come after the game's", plain.Type >= ItemRarityID.Count && shiny.Type >= ItemRarityID.Count && plain.Type != shiny.Type,
            $"plain={plain.Type} shiny={shiny.Type}");
        Report("RarityType<T> gives the type", ModContent.RarityType<Plain>() == plain.Type && ModContent.RarityType<Shiny>() == shiny.Type,
            $"{ModContent.RarityType<Plain>()} {ModContent.RarityType<Shiny>()}");
        Report("GetRarity finds the mod rarities and only them", RarityLoader.GetRarity(plain.Type) == plain && RarityLoader.GetRarity(shiny.Type) == shiny &&
            RarityLoader.GetRarity(ItemRarityID.Purple) == null && RarityLoader.GetRarity(RarityLoader.RarityCount) == null, $"count={RarityLoader.RarityCount}");
        Report("RarityCount counts the mod rarities", RarityLoader.RarityCount >= ItemRarityID.Count + 4, $"{RarityLoader.RarityCount}");
        Report("SetStaticDefaults ran once", Plain.StaticDefaults == 1 && Shiny.StaticDefaults == 1, $"plain={Plain.StaticDefaults} shiny={Shiny.StaticDefaults}");
        Report("FullName", plain.FullName == "CsRarity/Plain", plain.FullName);

        Item blade = Make<PlainBlade>();
        Report("the item takes the rarity in SetDefaults", blade.rare == plain.Type, $"rare={blade.rare}");

        Report("the popup color is the RarityColor", Same(Item.GetPopupRarityColor(plain.Type), plain.RarityColor) &&
            Same(Item.GetPopupRarityColor(shiny.Type), shiny.RarityColor), $"{Item.GetPopupRarityColor(plain.Type)} {Item.GetPopupRarityColor(shiny.Type)}");
        Report("a RarityColor that throws gives white", Same(Item.GetPopupRarityColor(broken.Type), new Color(255, 255, 255)),
            $"{Item.GetPopupRarityColor(broken.Type)}");
        Color green = Item.GetPopupRarityColor(ItemRarityID.Green);
        Report("the game's rarities keep their color", !Same(green, plain.RarityColor) && !Same(green, new Color(255, 255, 255)), $"{green}");

        Report("a good prefix moves Plain up to Shiny", Prefixed(Legendary, out int rare, plain.Type) && rare == shiny.Type, $"rare={rare}");
        Report("a bad prefix keeps Plain", Prefixed(BrokenPrefix, out rare, plain.Type) && rare == plain.Type, $"rare={rare}");
        Report("a bad prefix moves Shiny down to Plain", Prefixed(BrokenPrefix, out rare, shiny.Type) && rare == plain.Type, $"rare={rare}");
        Report("a good prefix keeps Shiny", Prefixed(Legendary, out rare, shiny.Type) && rare == shiny.Type, $"rare={rare}");

        bool random = true;
        string seen = "";
        for (int i = 0; i < 20; i++)
        {
            Item item = Sword(shiny.Type);
            item.Prefix(-1, out bool _);
            seen += item.rare + " ";
            random &= item.rare == plain.Type || item.rare == shiny.Type;
        }
        Report("a random prefix never leaves the mod rarities", random, seen);

        Report("a rarity past the end is held to the last one", Prefixed(Legendary, out rare, wild.Type) && rare == RarityLoader.RarityCount - 1,
            $"rare={rare} count={RarityLoader.RarityCount}");
        Report("a GetPrefixedRarity that throws keeps the rarity", Prefixed(Legendary, out rare, broken.Type) && rare == broken.Type,
            $"rare={rare}");
    }

    static Item Make<T>() where T : ModItem
    {
        Item item = Item.New();
        item.SetDefaults(ModContent.ItemType<T>(), default);
        return item;
    }

    // The game rolls prefixes only for the items its PrefixLegacy tables know (GetRollablePrefixes), so a mod weapon gets
    // none before ModPrefix; the rarity is checked on a vanilla sword holding the mod rarity.
    static Item Sword(int rare)
    {
        Item item = Item.New();
        item.SetDefaults(ItemID.IronBroadsword, default);
        item.rare = rare;
        return item;
    }

    static bool Prefixed(int prefix, out int rare, int start)
    {
        Item item = Sword(start);
        bool ok = item.Prefix(prefix, out bool _);
        rare = item.rare;
        if (!ok) Log.Info($"csrarity: Prefix({prefix}) did not apply");
        return ok;
    }

    static bool Same(Color a, Color b) => a.R == b.R && a.G == b.G && a.B == b.B;

    static void Give(Player p, int type)
    {
        for (int k = 0; k < 50; k++)
            if (p.inventory[k].type == type) return;
        for (int k = 0; k < 50; k++)
        {
            Item item = p.inventory[k];
            if (item.type > 0 && item.stack > 0) continue;
            item.SetDefaults(type, default);
            return;
        }
    }

    static void Report(string name, bool ok, string detail)
    {
        if (ok) pass++;
        else fail++;
        Log.Info($"csrarity {name}: {(ok ? "ok" : "FAIL")} {detail}");
    }
}
