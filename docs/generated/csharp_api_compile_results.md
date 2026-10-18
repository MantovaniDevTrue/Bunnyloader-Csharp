# Bunny C# — compilacao de exemplos em lote

Resultado de dotnet build com Roslyn e net8.0. Isto nao prova a execucao no Terraria Android.

Casos aprovados: 17/17
SDK: 12 metodos, 36 propriedades.

| Exemplo | Area | Compilacao | Fonte |
| --- | --- | --- | --- |
| weapon_wand | weapon_registration | pass | Content/Items/Weapons/ExampleWand.cs, main.cs |
| weapon_wave | weapon_registration | pass | Content/Items/Weapons/ExampleWaveWand.cs, Content/Projectiles/ExampleWaveBolt.cs |
| projectile_bolt | projectile_registration | pass | main.cs |
| projectile_wave | projectile_registration | pass | Content/Projectiles/ExampleWaveBolt.cs |
| projectile_hit_policy | legacy_live_hits | pass | main.cs |
| buff | buff_definition | pass | Content/Buffs/ExampleDefenseBuff.cs |
| potion | buff_potion | pass | Content/Items/Consumables/ExampleBuffPotion.cs, Content/Buffs/ExampleDefenseBuff.cs |
| accessory | accessory | pass | Content/Items/Accessories/ExampleFocusAccessory.cs |
| player_equips | player_equips | pass | Common/Players/ExampleFocusPlayer.cs, Content/Items/Accessories/ExampleFocusAccessory.cs |
| npc_slime | npc_registration | pass | Content/NPCs/ExampleSlimeNPC.cs |
| live_legacy | legacy_live_hits | pass | Common/Live/BunnyLiveEvents.cs |
| live_v4 | on_hit_context_v4 | pass | Common/Live/ExampleWaveBoltLiveEvents.cs |
| live_tick | legacy_live_hits | pass | Common/Live/ExampleLiveRuntime.cs |
| npc_ai | npc_ai_prototype | pass | Common/Live/ExampleLiveNpcAI.cs |
| motion_bolt | projectile_motion | pass | ExampleBoltMotion.cs |
| motion_wave | projectile_motion | pass | ExampleWaveBoltMotion.cs |
| sdk_contracts | sdk_all_public_members | pass | experiments/csharp/BunnyCsSdk/BunnySourceScriptApi.cs |
