# Receitas C# extraidas do ExampleModCSharp

Cada trecho e o arquivo C# existente completo, sem inventar API. Nao e uma traducao automatica de callbacks JS.

## legacy_live_hits — device_verified

Fonte: samples/ExampleModCSharp/Playable/content/Common/Live/BunnyLiveEvents.cs

Callbacks tradicionais confirmados com dois mods C#.

```csharp
namespace BunnyLoader.Source;

public static class BunnyLiveEvents
{
    private static int hits;
    private static int kills;

    public static int OnHitNPC(int call, int damage)
    {
        hits++;
        return damage + (call % 7) + hits;
    }

    public static int OnKill(int call, int damage)
    {
        kills++;
        return damage + (call % 5) + kills;
    }
}
```

## managed_npc_debuff — device_verified

Fonte: samples/ExampleModCSharp/Playable/content/Common/Live/ExampleWaveBoltLiveEvents.cs

Efeitos de debuff reais confirmados Android .85.1/.86.

```csharp
namespace BunnyLoader.Source;

public static class ExampleWaveBoltLiveEvents
{
    private static int hits;
    private static int kills;

    public static int OnHitNPC(int call, int damage)
    {
        hits++;
        return 1000 + damage + (call % 11) + hits;
    }

    public static int OnHitNPCV2(int call, int damage, int npcIndex)
    {
        hits++;
        return 10000 + damage + (call % 11) + hits + npcIndex;
    }

    public static int OnHitNPCV3(int call, int damage, int npcIndex, int npcType, int npcLife, float posX, float posY)
    {
        hits++;
        return 20000 + damage + (call % 11) + hits + npcIndex + npcType + npcLife + (int)posX + (int)posY;
    }

    public static int OnHitNPCV4(int call, int damage, int npcIndex, int npcType,
        int npcLife, float npcX, float npcY, int playerIndex, int playerLife,
        int playerLifeMax, float playerX, float playerY)
    {
        hits++;
        int playerHealthPercent = playerLifeMax > 0
            ? playerLife * 100 / playerLifeMax
            : 0;
        float dx = npcX - playerX;
        int enemyIsClose = dx > -160f && dx < 160f ? 1 : 0;
        if (hits % 4 == 0) {
            int direction = npcX < playerX ? 1 : 2;
            int strengthTenths = playerHealthPercent < 35 ? 55 : 35;
            return 2000000 + direction * 100 + strengthTenths;
        }
        int buffId = playerHealthPercent < 35 ? 39 : enemyIsClose == 1 ? 44 : 24;
        int durationTicks = npcLife > 100 ? 240 : 120;
        return 1000000 + buffId * 1000 + durationTicks;
    }

    public static int OnKill(int call, int damage)
    {
        kills++;
        return 2000 + damage + (call % 13) + kills;
    }
}
```

## npc_registration — device_verified

Fonte: samples/ExampleModCSharp/Playable/content/Content/NPCs/ExampleSlimeNPC.cs

Slime C# registrado e testado em Terraria Android.

```csharp
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleSlimeNPC : BunnyNPC
{
    public override void SetDefaults()
    {
        DisplayName = "Example Slime";
        Texture = "Content/NPCs/ExampleSlimeNPC.png";
        Width = 32;
        Height = 24;
        Damage = 7;
        Defense = 2;
        LifeMax = 25;
        AiStyle = 1;
        AnimationType = 1;
        Frames = 2;
    }
}
```

## projectile_motion — device_verified

Fonte: samples/ExampleModCSharp/Playable/content/Content/Projectiles/ExampleWaveBoltMotion.cs

Curvas controladas por C# validadas em dispositivo.

```csharp
using System;

namespace ExampleModCSharp;

public static class ExampleWaveBoltMotion
{
    public static float TurnAtTick(int tick)
    {
        return MathF.Sin(tick * 0.20f) * 0.018f;
    }
}
```

## projectile_registration — device_verified

Fonte: samples/ExampleModCSharp/Playable/content/Content/Projectiles/ExampleWaveBolt.cs

Projeteis C# ativos no Android .86.

```csharp
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleWaveBolt : BunnyProjectile
{
    public override void SetDefaults()
    {
        DisplayName = "Example C# Wave Bolt";
        Texture = "Content/Projectiles/SourceBolt.png";
        Width = 10;
        Height = 10;
        AiStyle = 1;
        Friendly = true;
        Penetrate = 2;
        TimeLeft = 180;
    }
}
```

## weapon_registration — device_verified

Fonte: samples/ExampleModCSharp/Playable/content/Content/Items/Weapons/ExampleWaveWand.cs

Arma C# funcional em combate Android .86.

```csharp
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleWaveWand : BunnyWeapon
{
    public override void SetDefaults()
    {
        DisplayName = "Example C# Wave Wand";
        Texture = "Content/Items/Weapons/SourceWand.png";
        Damage = 12;
        UseTime = 16;
        ShootSpeed = 9f;
        Shoot = nameof(ExampleWaveBolt);
    }
}
```

## accessory — ci_verified

Fonte: samples/ExampleModCSharp/Playable/content/Content/Items/Accessories/ExampleFocusAccessory.cs

Definicao de acessorio compila, teste extenso do equipamento pendente.

```csharp
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleFocusAccessory : BunnyAccessory
{
    public override void SetDefaults()
    {
        DisplayName = "Example Focus Accessory";
        Texture = "Content/Items/Accessories/ExampleFocusAccessory.png";
        DefenseBonus = 0;
    }
}
```

## buff_definition — ci_verified

Fonte: samples/ExampleModCSharp/Playable/content/Content/Buffs/ExampleDefenseBuff.cs

Definicao BunnyBuff e fonte C# presentes; nao e debuff vanilla V4.

```csharp
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleDefenseBuff : BunnyBuff
{
    public override void SetDefaults()
    {
        DisplayName = "Example Defense";
        Texture = "Content/Buffs/ExampleDefenseBuff.png";
        DefenseBonus = 10;
    }
}
```

## buff_potion — ci_verified

Fonte: samples/ExampleModCSharp/Playable/content/Content/Items/Consumables/ExampleBuffPotion.cs

Definicao BunnyBuffPotion presente; uso e duracao ainda exigem teste isolado.

```csharp
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleBuffPotion : BunnyBuffPotion
{
    public override void SetDefaults()
    {
        DisplayName = "Example Defense Potion";
        Texture = "Content/Items/Consumables/ExampleBuffPotion.png";
        Buff = nameof(ExampleDefenseBuff);
        Duration = 600;
    }
}
```

## player_equips — ci_verified

Fonte: samples/ExampleModCSharp/Playable/content/Common/Players/ExampleFocusPlayer.cs

ResetEffects/UpdateEquips declarados; persistencia nao comprovada.

```csharp
using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleFocusPlayer : BunnyPlayer
{
    public override void SetDefaults()
    {
        Accessory = nameof(ExampleFocusAccessory);
    }

    public override void ResetEffects()
    {
        DefenseBonus = 0;
    }

    public override void UpdateEquips(bool accessoryEquipped)
    {
        if (accessoryEquipped)
            DefenseBonus = 7;
    }
}
```

## npc_ai_prototype — experimental

Fonte: samples/ExampleModCSharp/Playable/content/Common/Live/ExampleLiveNpcAI.cs

AI gerenciada demonstrada, nao equivale a hooks JS NPC AI universais.

```csharp
namespace BunnyLoader.Source;

public static class BunnyLiveNpc
{
    private static int updates;

    public static int AI(int playerDeltaX, int npcLife)
    {
        updates++;
        if (npcLife <= 0 || playerDeltaX >= -24 && playerDeltaX <= 24)
            return 0;
        int speed = npcLife <= 12 ? 18 : 12 + updates % 3;
        return playerDeltaX > 0 ? speed : -speed;
    }
}
```
