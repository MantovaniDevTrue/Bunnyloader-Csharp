# NPC C# — Inventário de cobertura (baseline .43)

> Referência de implementação: **ExampleModBunny em JavaScript**. O ExampleMod do tModLoader não é fonte de código para o Bunny Loader.

Auditoria dos arquivos-fonte existentes em `samples/ExampleMod/content` e do primeiro `BunnyNPC` C# da versão **0.1.10.16.43**. A lista abaixo é um inventário de suporte e lacunas, **não** uma promessa de compatibilidade com todos os métodos.

**Escopo encontrado:** 4 classes (3 ModNPC e 1 GlobalNPC), 21 métodos distintos, 31 implementações de métodos. Apenas `SetDefaults` e a configuração equivalente a parte de `SetStaticDefaults` têm suporte **parcial**. A execução de um método chamado `OnKill` em projéteis C# **não** equivale a `NPC.OnKill`.

## Fontes inspecionadas

| Referência JS | Categoria | Métodos |
|---|---|---:|
| `ExampleSlimeNPC` (`Content/NPCs/ExampleSlimeNPC.js`) | ModNPC | 7 |
| `ExampleBoss` (`Content/NPCs/ExampleBoss/ExampleBoss.js`) | ModNPC | 9 |
| `ExamplePerson` (`Content/NPCs/ExamplePerson/ExamplePerson.js`) | ModNPC | 14 |
| `ExampleNPCLoot` (`Common/GlobalNPCs/ExampleNPCLoot.js`) | GlobalNPC | 1 |

## Métodos — mapa de compatibilidade

| Método do JS | Ocorrências | Estado na API C# .43 | Diferença |
|---|---:|---|---|
| `AddShops` | 1 | Pendente | No C# NPCShop declaration/registration bridge. |
| `AI` | 1 | Pendente | Existing vanilla aiStyle=1 works, but no arbitrary per-frame managed C# NPC AI. |
| `ApplyBuffImmunity` | 1 | Pendente | No per-NPC buffImmune bridge; slime's buffImmune[20] not implemented. |
| `CanTownNPCSpawn` | 1 | Pendente | No NPC town-spawn callback. |
| `CheckConditions` | 1 | Pendente | No town-home placement conditions callback. |
| `FindFrame` | 1 | Pendente | No C# NPC frame callback; only vanilla animation type/frame count supported. |
| `GetChat` | 1 | Pendente | No C# chat line selection/localization bridge. |
| `HitEffect` | 3 | Pendente | No C# HitEffect callback or dust/gore generation bound to hit/death. |
| `ModifyNPCLoot` | 3 | Pendente | No C# NPC loot rules (normal/expert, custom item, boss bag or GlobalNPC changes). |
| `OnChatButtonClicked` | 1 | Pendente | No C# town chat button handler. |
| `OnKill` | 1 | Pendente | NPC.OnKill is absent; existing C# OnKill is a projectile policy, not an NPC callback. |
| `PreAI` | 1 | Pendente | No dynamic pre-AI callback; cannot react to per-frame NPC state. |
| `SetBestiary` | 3 | Pendente | No C# bestiary entries, spawn tags, portraits or flavor text APIs. |
| `SetChatButtons` | 1 | Pendente | No C# town chat UI button bridge. |
| `SetDefaults` | 3 | Parcial | BunnyNPC.SetDefaults applies only validated basic width/height/damage/defense/lifeMax/aiStyle and limited vanilla animation fields. Sounds, color, value, town/boss defaults are absent. |
| `SetNPCNameList` | 1 | Pendente | No C# town NPC naming integration. |
| `SetStaticDefaults` | 3 | Parcial | Some frame setup is represented by BunnyNPC.Frames/AnimationType properties; the C# hook SetStaticDefaults and other NPCID.Sets mutations do not exist. |
| `SpawnChance` | 1 | Pendente | No C# spawn rate callback; conditions vary by biome, rain, hardmode, day and surface. |
| `TownNPCAttackProj` | 1 | Pendente | No C# town NPC attack projectile hook. |
| `TownNPCAttackProjSpeed` | 1 | Pendente | No C# town NPC projectile speed hook. |
| `TownNPCAttackStrength` | 1 | Pendente | No C# town NPC attack attributes hook. |

## Fidelidade ao ExampleSlimeNPC

| Recurso do JS | Estado | Observação |
|---|---|---|
| name/texture/registered NPC | Validado | SourceMods + Android ExampleSlimeNPC id=697 and in-world appearance confirmed |
| damage=7, defense=2, lifeMax=25, aiStyle=1 | Validado | Roslyn table and Bunny native NPC defaults, tested in Android |
| frame count=2 / vanilla BlueSlime animation | Parcial | C# Frames/AnimationType accepted, not full SetStaticDefaults callback |
| alpha=175 / tint color RGBA(40,200,255,100) | Pendente | Aguardando ponte C# e teste no Android |
| HitSound=NPCHit1, DeathSound=NPCDeath1 | Pendente | Aguardando ponte C# e teste no Android |
| coin value=25 copper | Pendente | Aguardando ponte C# e teste no Android |
| slime rain / shimmer metadata | Pendente | Aguardando ponte C# e teste no Android |
| Poisoned buff immunity[20] | Pendente | Aguardando ponte C# e teste no Android |
| biome/day/rain/hardmode spawn conditions | Pendente | Aguardando ponte C# e teste no Android |
| bestiary info and biome tag | Pendente | Aguardando ponte C# e teste no Android |
| Gel / custom ExampleItem / Slime Staff loot rules | Pendente | Aguardando ponte C# e teste no Android |
| TintableDust impact and death visual effects | Pendente | Aguardando ponte C# e teste no Android |

## Como terminar um exemplo antes de avançar

1. **Presentação e dados (baixo risco):** completar som de impacto/morte, valor monetário, cor/alpha, imunidades, slime rain e shimmer. Toda propriedade deve ser lida pelo Roslyn e aplicada a campos reais do Terraria, com verificação de tipos e limites.
2. **Efeitos e combate:** implementar `HitEffect` com poeira e controles de quantidade, depois os eventos e a persistência de `NPC.OnKill`; comprovar em teste in-game e contadores separados.
3. **Integração ao mundo:** loot com regras e condições, bestiário, bioma e `SpawnChance` com dados reais do mundo; respeitar cliente/servidor.
4. **Callbacks dinâmicos:** criar uma ponte C# ↔ nativo para `PreAI`, `AI`, `FindFrame` e futuras APIs. Passar posição, velocidade, vida, alvo e estado em argumentos explícitos; devolver alterações validadas. Pré-cálculo permanece para recursos que não exigem comportamento em tempo real.
5. **Casos maiores:** `ExampleBoss` completo (duas fases, ataque, música, drops e persistência), `ExamplePerson` completo (morador, spawn, chat, shop, combate, felicidade) e `ExampleNPCLoot` (`GlobalNPC`).

O requisito não é apenas compilar: cada método precisa de **exemplo C# funcional**, teste automatizado, teste de integração no Android, verificação de comportamento visual/sonoro e indicação clara de limitações.

## Arquitetura a preservar

- **QuickJS e MonoVM continuam coexistindo**, sob o carregador único de mods quando a integração de ciclo de vida estiver completa.
- **Callbacks em tempo real não são sinônimo de recompilar por frame**. Compilação Roslyn ocorre ao carregar ou quando o fonte muda; callbacks usam o assembly compilado.
- **Hooks nativos existentes primeiro**, sem copiar código tModLoader. Para IA dinâmica, cachear identificadores de método, impedir reentrância, anexar a thread gerenciada adequadamente, capturar exceções e invalidar handles ao descarregar o mod.
- **Otimizar sem remover efeitos**: decisões pré-computáveis não precisam atravessar a ponte MonoVM em todos os frames. IA contínua precisa de limites medidos no Android; comportamento complexo em fases deve manter fidelidade.
- O `BunnyNPC` da .43 aceita IA vanilla de slime (`AiStyle = 1`) e alguns atributos; **não executa `AI(npc)` arbitrário em C#**.

## Verificação automática

A análise exige que cada método do ExampleModBunny esteja classificado no arquivo `npc_methods.json`. Novos métodos, mudanças nos exemplos ou remoção das pontes C# básicas farão o teste falhar:

```bash
python3 samples/ExampleModCSharp/Tests/check_npc_coverage.py
python3 samples/ExampleModCSharp/Tests/check_npc_coverage.py --json
```

**Baseline móvel:** `.43` validado para registro e slime vanilla. `.44` é por enquanto **uma auditoria de cobertura**, sem mudar o APK de jogo nem marcar funções ausentes como concluídas.
