using System;
using System.Collections.Generic;

namespace BunnyLoader.CSharp.Authoring;

public sealed record BunnyModInfo(
    string Uid,
    string Id,
    string Name,
    string Version,
    string Author,
    string Description)
{
    public string License { get; init; } = "MIT";
    public string Category { get; init; } = "Teste";
    public int TerrariaBuild { get; init; } = 301720;
}

public sealed record BunnyProjectile(string Name)
{
    public int Width { get; init; } = 8;
    public int Height { get; init; } = 8;
    public int AiStyle { get; init; } = 1;
    public int TimeLeft { get; init; } = 420;
    public int Penetrate { get; init; } = 1;
    public bool Friendly { get; init; } = true;
    public bool Hostile { get; init; }
    public bool Ranged { get; init; } = true;
    public bool IgnoreWater { get; init; } = true;
    public bool TileCollide { get; init; } = true;
}

public sealed record BunnyWeapon(string Name, string Projectile)
{
    public int Damage { get; init; } = 22;
    public int Knockback { get; init; } = 2;
    public int Crit { get; init; }
    public int UseTime { get; init; } = 18;
    public float ShootSpeed { get; init; } = 13f;
    public string Rarity { get; init; } = "Green";
    public string UseSound { get; init; } = "Item5";
    public int WoodCost { get; init; } = 10;
}

public sealed record BunnyNpc(string Name)
{
    public int Width { get; init; } = 32;
    public int Height { get; init; } = 24;
    public int Damage { get; init; } = 14;
    public int Defense { get; init; } = 5;
    public int LifeMax { get; init; } = 90;
    public float KnockBackResist { get; init; } = 0.7f;
    public int FrameCount { get; init; } = 2;
    public string AnimationNpc { get; init; } = "BlueSlime";
    public string HitSound { get; init; } = "NPCHit1";
    public string DeathSound { get; init; } = "NPCDeath1";
    public string? CombatProjectile { get; init; }
}

public sealed record BunnySummonItem(string Name, string Npc)
{
    public int WoodCost { get; init; } = 10;
    public string Rarity { get; init; } = "Green";
}

public sealed record BunnyAccessory(string Name)
{
    public int DamagePercent { get; init; } = 10;
    public string Rarity { get; init; } = "Green";
    public int WoodCost { get; init; } = 10;
}

public enum BunnyArmorSlot
{
    Head,
    Body,
    Legs
}

public sealed record BunnyArmor(string Name, BunnyArmorSlot Slot)
{
    public int Defense { get; init; } = 5;
    public int ManaBonus { get; init; }
    public int MinionBonus { get; init; }
    public int MoveSpeedPercent { get; init; }
    public string Rarity { get; init; } = "Green";
    public int WoodCost { get; init; } = 10;
}

public sealed record BunnyArmorSet(string Head, string Body, string Legs)
{
    public int DamagePercent { get; init; } = 10;
}

public sealed record BunnyBuff(string Name)
{
    public int DefenseBonus { get; init; } = 10;
    public bool IsDebuff { get; init; }
}

public sealed record BunnyBuffPotion(string Name, string Buff)
{
    public int DurationTicks { get; init; } = 3600;
    public string Rarity { get; init; } = "Green";
    public int WoodCost { get; init; } = 10;
}

public sealed record BunnyMaterial(string Name)
{
    public string Rarity { get; init; } = "Green";
    public int MaxStack { get; init; } = 9999;
}

public sealed record BunnyNpcDrop(string Npc, string Material)
{
    public int ChanceDenominator { get; init; } = 2;
    public int MinStack { get; init; } = 1;
    public int MaxStack { get; init; } = 2;
}

public sealed class BunnyContentRegistry
{
    public List<BunnyProjectile> Projectiles { get; } = new();
    public List<BunnyWeapon> Weapons { get; } = new();
    public List<BunnyNpc> Npcs { get; } = new();
    public List<BunnySummonItem> SummonItems { get; } = new();
    public List<BunnyAccessory> Accessories { get; } = new();
    public List<BunnyArmor> Armor { get; } = new();
    public List<BunnyArmorSet> ArmorSets { get; } = new();
    public List<BunnyBuff> Buffs { get; } = new();
    public List<BunnyBuffPotion> BuffPotions { get; } = new();
    public List<BunnyMaterial> Materials { get; } = new();
    public List<BunnyNpcDrop> NpcDrops { get; } = new();

    public void Add(BunnyProjectile projectile) => Projectiles.Add(projectile);
    public void Add(BunnyWeapon weapon) => Weapons.Add(weapon);
    public void Add(BunnyNpc npc) => Npcs.Add(npc);
    public void Add(BunnySummonItem item) => SummonItems.Add(item);
    public void Add(BunnyAccessory item) => Accessories.Add(item);
    public void Add(BunnyArmor item) => Armor.Add(item);
    public void Add(BunnyArmorSet armorSet) => ArmorSets.Add(armorSet);
    public void Add(BunnyBuff buff) => Buffs.Add(buff);
    public void Add(BunnyBuffPotion potion) => BuffPotions.Add(potion);
    public void Add(BunnyMaterial material) => Materials.Add(material);
    public void Add(BunnyNpcDrop drop) => NpcDrops.Add(drop);
}

public abstract class BunnyContentModule
{
    public abstract BunnyModInfo Info { get; }
    public abstract void Register(BunnyContentRegistry content);
}
