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

public sealed class BunnyContentRegistry
{
    public List<BunnyProjectile> Projectiles { get; } = new();
    public List<BunnyWeapon> Weapons { get; } = new();
    public List<BunnyNpc> Npcs { get; } = new();
    public List<BunnySummonItem> SummonItems { get; } = new();

    public void Add(BunnyProjectile projectile) => Projectiles.Add(projectile);
    public void Add(BunnyWeapon weapon) => Weapons.Add(weapon);
    public void Add(BunnyNpc npc) => Npcs.Add(npc);
    public void Add(BunnySummonItem item) => SummonItems.Add(item);
}

public abstract class BunnyContentModule
{
    public abstract BunnyModInfo Info { get; }
    public abstract void Register(BunnyContentRegistry content);
}
