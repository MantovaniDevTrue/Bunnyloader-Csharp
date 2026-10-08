# Pure C# Native Mod 0.3.0

Exemplo nativo C# NativeAOT, sem arquivos JavaScript. O pacote usa `entry: "@native"` e os registros são enviados diretamente pela exportação `bunny_mod_register_native_content`.

## Novidades

- `Native Pure Helm`: 5 de defesa
- `Native Pure Chest`: 6 de defesa, +20 de mana e +1 minion
- `Native Pure Legs`: 4 de defesa e +5% de velocidade
- **Bônus do conjunto completo**: +6 de defesa
- `Native Pure Orb`: projétil hostil criado pela Host API nativa
- `Native Pure Slime`: C# escolhe ataques; fase 1 dispara uma esfera, fase 2 dispara duas com intervalo menor

A arma com projétil perseguidor, o amuleto, o buff e a poção da versão 0.2.0 continuam disponíveis.

## Testes no Android

Usar o APK de teste 0.1.10.10 com o mod 0.3.0, inicialmente sozinho e em singleplayer.

No console de depuração do aplicativo:

```js
JSON.stringify({
  mods: bl.csharp.externalMods(),
  npc: bl.csharp.npcStatus(),
  projectiles: bl.csharp.projectileWriteStatus()
}, null, 2)
```

Esperado: `nativeContent: true`, `nativeItems: 6`, `nativeProjectiles: 2`, `nativeArmorPieces: 3`, `nativeArmorSets: 1`, `npcCombat: true` e `npcBindings: 1`.

Para invocar o slime depois de confirmar `nativeNpcType >= 0`:

```js
const mod = bl.csharp.externalMods().find(m => m.nativeContent);
if (mod && mod.nativeNpcType >= 0)
    Terraria.NPC.SpawnOnPlayer(Terraria.Main.myPlayer, mod.nativeNpcType, 0, 0, 0, 0);
```

Com o slime atacando, `npcStatus().attacksQueued` e `attacksConsumed` devem subir; `attacksRejected` deve continuar zero. Verificar se as esferas realmente aparecem, acertam o jogador e se a fase 2 atira duas por ataque.

Vestir as três peças, conferir defesa e bônus de conjunto; testar mana, quantidade de minions, velocidade e desequipar as peças para verificar se os efeitos são revertidos. Repetir usando o amuleto e a poção da versão anterior e conferir desempenho em 60 FPS.

## Limitações temporárias

- O registro de armadura é nativo, mas os slots visuais usados neste protótipo são emprestados dos slots vanilla. Sprites próprios para a armadura vestida ainda precisam ser implementados no C++ (geração nativa de ArmorIDs e TextureAssets); os sprites de inventário são próprios.
- Os disparos nativos usam `Terraria.DataStructures.EntitySource_Parent` e `Projectile.NewProjectile`, resolvidos em runtime. O método precisa ser validado no aparelho. Se faltar qualquer assinatura, a tentativa de ataque é recusada sem enviar disparos ao JavaScript.
- IA C# de NPC e disparos hostis continuam restritos ao singleplayer.
- O registro e os efeitos de armas, acessórios e buffs são compatíveis com a API 1 existente. Mods JavaScript não precisam ser convertidos.

Código-fonte: `experiments/csharp/ExamplePureCSharpMod/Exports.cs`, `app/src/main/cpp/experiments/csharp/NativeContent.cpp` e `NativeNpcShots.cpp`.
