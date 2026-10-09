# Compatibilidade C# — base 0.1.10.16.34

Estados: `device_verified` significa validado no Terraria Android; `ci_verified` significa apenas teste de compilacao/estrutura; `experimental` significa implementacao parcial; `not_implemented` significa que nao se deve ensinar codigo para ela.

| Recurso | Estado | Exemplo |
|---|---|---|
| Duas armas e dois projeteis C# | device_verified | SourceWand, SourceStaff, SourceBolt, SourceSpark |
| SetDefaults gerenciado | device_verified | Itens e projeteis |
| Homing C# | device_verified | SourceBolt |
| OnHitNPC C# + On Fire vanilla | device_verified | SourceBolt |
| OnKill C# + queimadura em area | device_verified | SourceBolt |
| Duas politicas de movimento independentes | device_verified | SourceBolt + SourceSpark |
| Dispatcher generico de curvas | device_verified | SourceSpark no mod de teste |
| Compilacao por UUID registrado em 0.1.10.16.35 | experimental | ExampleModCSharp/Playable |
| Carregamento de mods C# arbitrarios testado no Android | not_implemented | — |
| OnHitNPC/OnKill genericos em todo mod | experimental | Integracao SourceBolt apenas |
| NPCs C# personalizados | not_implemented | — |
| Buffs C# personalizados | not_implemented | — |
| Acessorios C# personalizados | not_implemented | — |
| Eventos do jogador C# | not_implemented | — |
| Multiplayer para politicas C# experimentais | not_implemented | — |

As confirmacoes de desempenho em 60 FPS foram feitas no aparelho do desenvolvedor com os exemplos descritos, nao em todos os celulares ou todas as condicoes.

Este documento deve permanecer sincronizado com `compatibility.json` e com os testes automatizados. O pacote independente ainda nao recebe estado `device_verified` ate ser testado no aparelho.
