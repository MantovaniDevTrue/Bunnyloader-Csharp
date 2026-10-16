using System.Globalization;
using BunnyLoader.CSharp.Authoring;

namespace BunnyLoader.CSharp.Packager;

internal static partial class Program
{
    private static void ValidateAdvanced(BunnyContentRegistry content)
    {
        HashSet<string> npcs = content.Npcs.Select(n => n.Name).ToHashSet(StringComparer.Ordinal);
        HashSet<string> materials = content.Materials.Select(m => m.Name).ToHashSet(StringComparer.Ordinal);
        HashSet<string> buffs = content.Buffs.Select(b => b.Name).ToHashSet(StringComparer.Ordinal);
        Dictionary<string, BunnyArmor> armor = content.Armor.ToDictionary(a => a.Name, StringComparer.Ordinal);

        foreach (BunnyAccessory accessory in content.Accessories)
        {
            Id(accessory.Rarity);
            if (accessory.DamagePercent < 0 || accessory.DamagePercent > 200 ||
                accessory.WoodCost < 0) throw new InvalidOperationException("Invalid accessory: " + accessory.Name);
        }

        foreach (BunnyArmor piece in content.Armor)
        {
            Id(piece.Rarity);
            if (!Enum.IsDefined(piece.Slot) || piece.Defense < 0 || piece.Defense > 100 ||
                piece.WoodCost < 0 || piece.ManaBonus < 0 || piece.MinionBonus < 0 ||
                piece.MoveSpeedPercent < 0 || piece.MoveSpeedPercent > 100)
                throw new InvalidOperationException("Invalid armor: " + piece.Name);
        }

        HashSet<string> heads = new(StringComparer.Ordinal);
        foreach (BunnyArmorSet set in content.ArmorSets)
        {
            if (!armor.TryGetValue(set.Head, out BunnyArmor? head) || head.Slot != BunnyArmorSlot.Head ||
                !armor.TryGetValue(set.Body, out BunnyArmor? body) || body.Slot != BunnyArmorSlot.Body ||
                !armor.TryGetValue(set.Legs, out BunnyArmor? legs) || legs.Slot != BunnyArmorSlot.Legs ||
                set.DamagePercent < 0 || set.DamagePercent > 200 || !heads.Add(set.Head))
                throw new InvalidOperationException("Invalid armor set: " + set.Head);
        }

        foreach (BunnyBuff buff in content.Buffs)
        {
            if (buff.DefenseBonus < -100 || buff.DefenseBonus > 100)
                throw new InvalidOperationException("Invalid buff value: " + buff.Name);
        }

        foreach (BunnyBuffPotion potion in content.BuffPotions)
        {
            Id(potion.Rarity);
            if (!buffs.Contains(potion.Buff) || potion.DurationTicks < 1 ||
                potion.DurationTicks > 216000 || potion.WoodCost < 0)
                throw new InvalidOperationException("Invalid potion: " + potion.Name);
        }

        foreach (BunnyMaterial material in content.Materials)
        {
            Id(material.Rarity);
            if (material.MaxStack < 1 || material.MaxStack > 9999)
                throw new InvalidOperationException("Invalid material stack: " + material.Name);
        }

        foreach (BunnyNpcDrop drop in content.NpcDrops)
        {
            if (!npcs.Contains(drop.Npc) || !materials.Contains(drop.Material) ||
                drop.ChanceDenominator < 1 || drop.ChanceDenominator > 100000 ||
                drop.MinStack < 1 || drop.MaxStack < drop.MinStack || drop.MaxStack > 9999)
                throw new InvalidOperationException("Invalid NPC drop: " + drop.Npc);
        }
    }

    private static string AccessoryJs(BunnyAccessory accessory) => """
        const { ItemID, TileID } = Terraria.ID;

        export class @NAME@ extends ModItem {
            SetDefaults() {
                this.Item.width = 28;
                this.Item.height = 28;
                this.Item.accessory = true;
                this.Item.rare = ItemRarityID.@RARITY@;
            }

            UpdateAccessory(item, player, vanity, hideVisual) {
                if (vanity) return;
                const factor = 1 + @DAMAGE@ / 100;
                player.meleeDamage *= factor;
                player.rangedDamage *= factor;
                player.magicDamage *= factor;
            }

            AddRecipes() {
                this.CreateRecipe()
                    .AddIngredient(ItemID.Wood, @WOOD@)
                    .AddTile(TileID.WorkBenches)
                    .Register();
            }
        }
        """.Replace("@NAME@", accessory.Name).Replace("@RARITY@", accessory.Rarity)
        .Replace("@DAMAGE@", accessory.DamagePercent.ToString(CultureInfo.InvariantCulture))
        .Replace("@WOOD@", accessory.WoodCost.ToString(CultureInfo.InvariantCulture));

    private static string ArmorJs(BunnyArmor piece, BunnyContentRegistry registry)
    {
        BunnyArmorSet? set = registry.ArmorSets.FirstOrDefault(s => s.Head == piece.Name);
        string armorSetJs = set is null ? "" : """
                AddArmorSets() {
                    this.CreateArmorSet(this.Type, ModContent.ItemType('@BODY@'),
                                        ModContent.ItemType('@LEGS@'), '@BONUS@');
                }

                UpdateArmorSet(item, player) {
                    const bonus = @PERCENT@ / 100;
                    player.meleeDamage += bonus;
                    player.rangedDamage += bonus;
                    player.magicDamage += bonus;
                    player.minionDamage += bonus;
                }

            """.Replace("@BODY@", set.Body).Replace("@LEGS@", set.Legs)
            .Replace("@BONUS@", "Dano aumentado em " + set.DamagePercent.ToString(CultureInfo.InvariantCulture) + "%")
            .Replace("@PERCENT@", set.DamagePercent.ToString(CultureInfo.InvariantCulture));

        return ("""
            const { ItemID, TileID } = Terraria.ID;

            export class @NAME@ extends ModItem {
                SetDefaults() {
                    this.Item.width = 18;
                    this.Item.height = 18;
                    this.Item.defense = @DEFENSE@;
                    this.Item.rare = ItemRarityID.@RARITY@;
                }

                UpdateEquip(item, player) {
                    player.statManaMax2 += @MANA@;
                    player.maxMinions += @MINIONS@;
                    player.moveSpeed += @SPEED@ / 100;
                }

            @SET@
                AddRecipes() {
                    this.CreateRecipe()
                        .AddIngredient(ItemID.Wood, @WOOD@)
                        .AddTile(TileID.WorkBenches)
                        .Register();
                }
            }
            """).Replace("@NAME@", piece.Name)
            .Replace("@DEFENSE@", piece.Defense.ToString(CultureInfo.InvariantCulture))
            .Replace("@RARITY@", piece.Rarity)
            .Replace("@MANA@", piece.ManaBonus.ToString(CultureInfo.InvariantCulture))
            .Replace("@MINIONS@", piece.MinionBonus.ToString(CultureInfo.InvariantCulture))
            .Replace("@SPEED@", piece.MoveSpeedPercent.ToString(CultureInfo.InvariantCulture))
            .Replace("@SET@", armorSetJs)
            .Replace("@WOOD@", piece.WoodCost.ToString(CultureInfo.InvariantCulture));
    }

    private static string BuffJs(BunnyBuff buff) => """
        export class @NAME@ extends ModBuff {
            SetStaticDefaults() {
                Terraria.Main.debuff[this.Type] = @DEBUFF@;
            }

            UpdatePlayer(player, buffIndex) {
                player.statDefense += @DEFENSE@;
            }
        }
        """.Replace("@NAME@", buff.Name)
        .Replace("@DEBUFF@", buff.IsDebuff ? "true" : "false")
        .Replace("@DEFENSE@", buff.DefenseBonus.ToString(CultureInfo.InvariantCulture));

    private static string BuffPotionJs(BunnyBuffPotion potion) => """
        const { ItemID, TileID, ItemUseStyleID, SoundID } = Terraria.ID;

        export class @NAME@ extends ModItem {
            SetDefaults() {
                this.Item.width = 18;
                this.Item.height = 26;
                this.Item.useStyle = ItemUseStyleID.DrinkLiquid;
                this.Item.useAnimation = 15;
                this.Item.useTime = 15;
                this.Item.useTurn = true;
                this.Item.UseSound = SoundID.Item3;
                this.Item.maxStack = ModItem.CommonMaxStack;
                this.Item.consumable = true;
                this.Item.rare = ItemRarityID.@RARITY@;
                this.Item.buffType = ModBuff.getTypeByName('@BUFF@');
                this.Item.buffTime = @DURATION@;
            }

            AddRecipes() {
                this.CreateRecipe()
                    .AddIngredient(ItemID.Wood, @WOOD@)
                    .AddTile(TileID.WorkBenches)
                    .Register();
            }
        }
        """.Replace("@NAME@", potion.Name).Replace("@BUFF@", potion.Buff)
        .Replace("@DURATION@", potion.DurationTicks.ToString(CultureInfo.InvariantCulture))
        .Replace("@RARITY@", potion.Rarity)
        .Replace("@WOOD@", potion.WoodCost.ToString(CultureInfo.InvariantCulture));

    private static string MaterialJs(BunnyMaterial material) => """
        export class @NAME@ extends ModItem {
            SetDefaults() {
                this.Item.width = 20;
                this.Item.height = 20;
                this.Item.maxStack = @STACK@;
                this.Item.rare = ItemRarityID.@RARITY@;
            }
        }
        """.Replace("@NAME@", material.Name)
        .Replace("@STACK@", material.MaxStack.ToString(CultureInfo.InvariantCulture))
        .Replace("@RARITY@", material.Rarity);

    private static string NpcDropsJs(BunnyNpc npc, BunnyContentRegistry registry)
    {
        BunnyNpcDrop[] drops = registry.NpcDrops.Where(d => d.Npc == npc.Name).ToArray();
        if (drops.Length == 0) return "";
        string body = string.Join("\n", drops.Select(drop =>
            "        npcLoot.Add(ItemDropRule.Common(ModContent.ItemType('" + drop.Material + "'), " +
            drop.ChanceDenominator.ToString(CultureInfo.InvariantCulture) + ", " +
            drop.MinStack.ToString(CultureInfo.InvariantCulture) + ", " +
            drop.MaxStack.ToString(CultureInfo.InvariantCulture) + "));"));
        return "    ModifyNPCLoot(npcLoot) {\n" +
            "        const { ItemDropRule } = Terraria.GameContent.ItemDropRules;\n" +
            body + "\n    }\n";
    }
}
