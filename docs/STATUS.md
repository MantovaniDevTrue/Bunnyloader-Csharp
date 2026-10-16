# Estado atual

Atualizado em **16/10/2026**. Checkpoint de código e APK:
**`0.1.10.16.90.29.62-csharp-projectile-initial-tick`**, `versionCode=222`,
commit [`fe424d7`](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/commit/fe424d7ca2b56bd5474eaaedd3706f17f6f03997).
Mantovani confirmou nesta data: **“.62 foi perfeita”**. Registramos o teste
Android informado. A PR #35 também registra 69 FPS, 280/280 callbacks e zero
rejeições para a .62; os números são atribuídos a esse registro, sem extrapolar
para outros cenários.

## Evidências por checkpoint

| Versão completa | Evidência | Alcance |
| --- | --- | --- |
| `0.1.10.16.90.29.57` | Relatório Android sem erros/avisos; 2 mods C#, 17 itens, 2 prefixos, 11 receitas; save de personagem/mundo; confirmação do usuário | Correções de equipamento, Color e luz no mundo; preparo frio de 14.609 ms nessa sessão |
| `0.1.10.16.90.29.58` | Usuário confirmou validação; PR #31 registra Android a 60 FPS | IA C# de NPC/projétil sobre as correções da PR #28 |
| `0.1.10.16.90.29.61` | PR #34: `projectileAiJobsProcessed=735`; projétil hit 23/28 e kill 58/62; NPC hit 128/128 e kill 23/23; zero erros | Ponte com curva nativa; houve rejeições; FPS da sessão não informado |
| `0.1.10.16.90.29.62` | Build e sete workflows de CI aprovados; usuário confirmou funcionamento perfeito; PR #35 registra 280/280 callbacks, zero rejeições e 69 FPS | Primeiro tick antecipado; números correspondem ao teste registrado, não a todas as cargas |

Ambiente desta linha: **Xiaomi 2109119DG, Android 14/API 34, arm64-v8a,
Terraria 1.4.5.8.6 (301720)**. A validação nesse aparelho não substitui testes
em outros dispositivos, multiplayer ou sessões prolongadas.

A [PR #35 (.63)](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/35)
acrescenta controle de QuickJS e avisos de sessão mista em outra branch. Ela
ainda pede validação Android e não está incluída no checkpoint integrado .62.

## Build e pacotes correspondentes

- [Build Android .62 — 38916992142](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/38916992142).
- [PR #34 — primeiro tick](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/34),
  [PR #33 — ponte de curvas](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/33),
  [PR #30 — IA e base nativa](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/30).
- [Entrega restrita .62](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp-Builds/releases/tag/csharp-v3-run-38916992142-a1)
  — requer acesso ao repositório privado.
- APK `.62`: SHA-256 `4813b5c490096774913e12e571e6b2148a1a44385f16ae6e2dcf58c92a9ba40e`.
- ExampleModCSharp `0.1.40` dessa entrega: SHA-256 `347fa274b2b50cfd57bf7f033edc8a4af03e56d9dbc1c13236e0e2331aee71c4`.
- ArmorPoliciesCSharp `0.1.0`: SHA-256 `83b73a6fd875a670a71174a3756f98d9e4b43739d10049785ba1901497fda2de`.

O ExampleMod manteve `0.1.40` entre builds com bytes diferentes. Use o pacote
da mesma release do APK e confira o hash. Novas mudanças de pacote devem
incrementar sua versão. Esta organização não altera os bytes do pacote `.62`.
Dados verificáveis: [checkpoint.json](checkpoint.json).

## Suporte e limites

| Área | Situação |
| --- | --- |
| Compilação `.cs` e cache | Roslyn no Android, worker isolado, integridade de fontes/referências/DLL/descritores |
| Carregamento | Antes do mundo; resumo de sucesso; falhas e assets pendentes mantêm diagnóstico |
| Equipamento/receitas/armaduras | Serviços nativos; caminho C# implementado independe dos adaptadores JS |
| Player e dano | Hooks `ref/out`, prazo no pré-hit e notificações posteriores; não é ciclo C# universal por frame |
| IA NPC/projétil | Snapshots no worker persistente e velocidade validada por identidade/sessão; cadência amostrada |
| Combate de entidades | Fila não bloqueante; NPC.OnHit observa redução de vida, incluindo dano contínuo; entrega best-effort |
| Exemplos | Parcial; [matriz](../Split/Bunny-Loader/docs/generated/example_completion/matrix.md) preserva lacunas |
| Multiplayer/hot-unload | Contrato completo pendente |
| FPS e estabilidade | Evidência por sessão/aparelho; não extrapolar CI ou microbenchmarks |

Erros de sessões antigas não são erros novos. `SINAL status=9`, reinstalação e
encerramento por memória precisam de contexto para estabelecer causa.
[Roteiro de validação](SDK-VALIDACAO.md).
