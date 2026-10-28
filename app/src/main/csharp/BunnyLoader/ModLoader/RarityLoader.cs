using System;
using System.Runtime.InteropServices;
using System.Threading;
using BunnyLoader;
using BunnyLoader.Interop;
using Microsoft.Xna.Framework.Graphics;
using Terraria.ID;

namespace Terraria.ModLoader;

public static unsafe class RarityLoader
{
    static readonly object Gate = new();

    static ModRarity[] rarities = Array.Empty<ModRarity>();

    static Hook? popup, prefix, mouseText;

    public static int RarityCount => ItemRarityID.Count + Volatile.Read(ref rarities).Length;

    public static ModRarity? GetRarity(int type)
    {
        ModRarity[] all = Volatile.Read(ref rarities);
        int at = type - ItemRarityID.Count;
        return at >= 0 && at < all.Length ? all[at] : null;
    }

    internal static bool IsModded(int type) => GetRarity(type) != null;

    internal static void Register(ModRarity rarity)
    {
        lock (Gate)
        {
            ModRarity[] all = rarities;
            var grown = new ModRarity[all.Length + 1];
            Array.Copy(all, grown, all.Length);
            rarity.Type = ItemRarityID.Count + all.Length;
            grown[^1] = rarity;
            Volatile.Write(ref rarities, grown);
        }
        ItemHooks.Once("Rarities", Install);
    }

    internal static Color ColorOf(int type)
    {
        if (GetRarity(type) is not ModRarity rarity) return new Color(255, 255, 255);
        try
        {
            return rarity.RarityColor;
        }
        catch (Exception e)
        {
            ItemHooks.Report($"{rarity.FullName}.RarityColor", e);
            return new Color(255, 255, 255);
        }
    }

    internal static Color? NameColor(Item item, int rare, byte diff)
    {
        if (diff != 0 || !IsModded(rare) || item.IsNull || item.expert) return null;
        return ColorOf(rare);
    }

    static void Install()
    {
        ItemHooks.Once("TooltipDraw", ItemTooltipDraw.Install);
        ItemTooltipHooks.InstallLines();
        ItemTooltipDraw.EnsureStringHooks();
        popup = ItemHooks.Raw("Terraria", "Item", "Color GetPopupRarityColor(int itemRarity)",
            (IntPtr)(delegate* unmanaged<int, IntPtr, uint>)&PopupColor);
        prefix = ItemHooks.Raw("Terraria", "Item", "bool Prefix(int prefixWeWant, out bool rolledPrefixIsTopTier)",
            (IntPtr)(delegate* unmanaged<IntPtr, int, byte*, IntPtr, byte>)&Prefix);
        mouseText = ItemHooks.Raw("Terraria", "Main", "void MouseTextInner(Main.MouseTextCache info, bool worldMouse)",
            (IntPtr)(delegate* unmanaged<IntPtr, IntPtr, byte, IntPtr, void>)&MouseTextInner);
    }

    [UnmanagedCallersOnly]
    static uint PopupColor(int rarity, IntPtr method)
    {
        Hook? hook = popup;
        if (hook == null) return 0xFFFFFFFF;
        if (IsModded(rarity))
        {
            Color color = ColorOf(rarity);
            return *(uint*)&color;
        }
        var call = GameCall.Returning(RetKind.Int);
        call.Arg(rarity);
        call.Arg(method);
        return (uint)ItemHooks.Original(ref call, hook, "Item.GetPopupRarityColor");
    }

    [UnmanagedCallersOnly]
    static byte Prefix(IntPtr self, int prefixWeWant, byte* rolledPrefixIsTopTier, IntPtr method)
    {
        Hook? hook = prefix;
        if (hook == null) return 0;
        var item = new Item(self);
        int before = 0;
        try { before = item.rare; }
        catch (Exception e) { ItemHooks.Report("Item.Prefix (the rarity)", e); }
        var call = GameCall.Returning(RetKind.Int);
        call.Arg(self);
        call.Arg(prefixWeWant);
        call.Arg((IntPtr)rolledPrefixIsTopTier);
        call.Arg(method);
        bool rolled = (ItemHooks.Original(ref call, hook, "Item.Prefix") & 0xFF) != 0;
        if (rolled && prefixWeWant != -3)
        {
            try { Prefixed(item, before); }
            catch (Exception e) { ItemHooks.Report("ModRarity.GetPrefixedRarity", e); }
        }
        return rolled ? (byte)1 : (byte)0;
    }

    static void Prefixed(Item item, int before)
    {
        if (GetRarity(before) is not ModRarity rarity) return;
        float value = 1f;
        if (item.prefix != 0)
        {
            item.TryGetPrefixStatMultipliersForItem(item.prefix, out float _, out float _, out float _, out float _, out float _, out float _,
                out int _, out int _, out int _, out float multiplier);
            value = multiplier;
        }
        int offset = value >= 1.2f ? 2 : value >= 1.05f ? 1 : value <= 0.8f ? -2 : value <= 0.95f ? -1 : 0;
        int rare;
        try
        {
            rare = rarity.GetPrefixedRarity(offset, value);
        }
        catch (Exception e)
        {
            ItemHooks.Report($"{rarity.FullName}.GetPrefixedRarity", e);
            rare = before;
        }
        if (rare > ItemRarityID.Quest) rare = Math.Clamp(rare, ItemRarityID.Gray, RarityCount - 1);
        item.rare = rare;
    }

    [UnmanagedCallersOnly]
    static void MouseTextInner(IntPtr self, IntPtr info, byte worldMouse, IntPtr method)
    {
        Hook? hook = mouseText;
        if (hook == null) return;
        Color? color = null;
        try { color = FloorColor(info); }
        catch (Exception e) { ItemHooks.Report("ModRarity (the item name under the cursor)", e); }
        if (color != null) ItemTooltipDraw.Recolor = color;
        try
        {
            var call = GameCall.Returning(RetKind.Int);
            call.Arg(self);
            call.Arg(info);
            call.Arg(worldMouse);
            call.Arg(method);
            ItemHooks.Original(ref call, hook, "Main.MouseTextInner");
        }
        finally
        {
            if (color != null) ItemTooltipDraw.Recolor = null;
        }
    }

    static Color? FloorColor(IntPtr info)
    {
        if (info == IntPtr.Zero) return null;
        var cache = (Main.MouseTextCache*)info;
        if (cache->diff != 0 || !IsModded(cache->rare)) return null;
        if (!string.IsNullOrEmpty(cache->buffTooltip.ToString())) return null;
        Item hover = Main.HoverItem;
        if (!hover.IsNull && hover.type >= 1) return null;
        return ColorOf(cache->rare);
    }
}
