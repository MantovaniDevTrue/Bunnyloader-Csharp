# ExampleModCSharp — guia e testes da API C# do Bunny Loader

**Versão experimental do mod:** 0.1.13. **Base Android validada:** Bunny Loader C# .84 + ExampleModCSharp 0.1.12 (416/416 callbacks V4, 58–60 FPS). **.85 / Efeitos de gameplay:** pedidos de debuffs vanilla produzidos no C# e executados em NPC.AI; validação Android pendente.

Este exemplo segue os fontes do Bunny Loader e do ExampleModBunny, sem copiar APIs do tModLoader. Não garante execução direta de mods de PC.

## Índice

| Diretório | Finalidade |
| --- | --- |
| [Playable](Playable/) | Pacote .bmod com fontes .cs e sprites |
| [Playable/content/Content/Items/Weapons](Playable/content/Content/Items/Weapons/) | Duas armas C# independentes |
| [Playable/content/Content/Projectiles](Playable/content/Content/Projectiles/) | Dois projéteis C# independentes |
| [Playable/content/Common/Live](Playable/content/Common/Live/) | Eventos OnHitNPC/OnKill gerenciados |
| [Playable/content/Common/Players](Playable/content/Common/Players/) | Exemplo BunnyPlayer |
| [Playable/content/Content/Buffs](Playable/content/Content/Buffs/) | Buff do exemplo |
| [Playable/content/Content/NPCs](Playable/content/Content/NPCs/) | NPC de exemplo |
| [Docs/GettingStarted.md](Docs/GettingStarted.md) | Procedimentos históricos e explicações |
| [Docs/SupportedAPI.md](Docs/SupportedAPI.md) | API comprovada e limites atuais |
| [Tests](Tests/) | Testes automáticos e roteiro Android |
| [Compatibility](Compatibility/) | Cobertura declarada |
| [Examples](Examples/) | Exemplos históricos, não base principal |

## Como começar

Copie a estrutura de Playable para um novo mod, troque o ID e UID por identificadores únicos e edite apenas as classes BunnyWeapon, BunnyProjectile e assets necessários. Empacote como .bmod com o empacotador existente do Bunny Loader. Não inclua .so, .dll ou arquivo JS em um mod-fonte C# independente.

Consulte as implementações reais de ExampleWand e ExampleWaveWand. O movimento de ExampleBoltMotion e ExampleWaveBoltMotion é pré-calculado em C# e aplicado pelo hook nativo. Compare BunnyLiveEvents com ExampleWaveBoltLiveEvents para aprender os callbacks gerenciados por tipo.

## Resultados Android verificados

- **.72:** FIFO64 separada, 2.897/2.897 eventos no teste prolongado, zero perdas e 60 FPS relatados.
- **.72 + ExampleModCSharp 0.1.8:** duas curvas funcionando, zero erros e 60 FPS relatados.
- **.73 + ExampleModCSharp 0.1.9:** duas classes de callback MonoVM por projétil, 985/985 eventos agregados e 879/879 secundários, sem falhas/descartes. FPS da .73 não informado.
- **.80:** dois mods C#, três armas, pausa/retomada, salvamento e reentrada validados.
- **.81 + ExampleModCSharp 0.1.10:** callback V2 com fallback antigo, testado no Android a 59–60 FPS contra 40 NPCs.
- **.82 + ExampleModCSharp 0.1.10:** 439/439 V2, 201 legados sem falhas, 85 slots de NPC e FPS 59–60 relatados.
- **.83 + ExampleModCSharp 0.1.11:** 383/383 V3, 138 legados, 0 falhas; contexto de NPC e 59–60 FPS confirmados em Android.
- **.84 + ExampleModCSharp 0.1.12:** 416/416 V4, 84 callbacks legados, NPC + jogador capturados, 58–60 FPS no Android.
- **.85 + ExampleModCSharp 0.1.13:** OnHitNPCV4 devolve um pedido de OnFire/Frostburn/CursedInferno; a fila nativa aplica debuff via NPC.AddBuff na thread NPC.AI após validar o alvo; teste Android pendente.

Não presumir suporte geral a tModLoader, múltiplos mods C# simultâneos ou multiplayer. Para um novo método, usar o SDK fonte deste repositório, não uma assinatura do PC.
