# Infraestrutura .90.29.1 / ExampleMod 0.1.36

O [plano global](../../../..//docs/csharp-completion-plan.md) contabiliza TODOS os
201 arquivos JS do upstream atualizado, sem excluir categorias. Consulte a
[matriz completa](../../docs/generated/example_completion/matrix.md) e seu JSON.
Os estados distinguem porta parcial, implementacao ligada ao runtime e validacao
real no Terraria. Os equipamentos nativos da .90.27.6 continuam preservados.

A infraestrutura anterior acrescentou BunnyItem e receitas publicas reutilizaveis, com os fontes
originais de ExamplePickaxe/ExampleHamaxe e do material ExampleItem. Os atributos,
animacao de uso/melee/mineracao, som Item1 e receitas sao processados pelo host.
A moeda customizada do arquivo ExampleItem.js ainda esta pendente e registrada
como lacuna. Nao ha shaders personalizados neste lote.

Testar: [roteiro Android](../../tests/basic_items/ANDROID.md). O guia e a matriz
acompanham o .bmod em content/Documentation. Fontes .cs continuam no pacote.

---

# ExampleModCSharp — guia e testes da API C# do Bunny Loader

**Pacote atual: 0.1.33 / Bunny .90.27.6.** O Guardian Shield e as Trailblazer
Boots usam slots nativos Shield/Shoes. A instalacao funciona com apenas mods C#;
itens salvos recebem o slot antes de o Terraria processar o equipamento. As botas
visiveis chamam a rotina nativa de chamas ao correr no chao, sem montaria.
Veja os comentarios nos dois acessorios e o guia dentro do .bmod.
Testes de host cobrem instalacao automatica, registro tardio, repeticoes,
falhas/retry e condicoes do rastro. O desenho no aparelho ainda exige verificacao.

**Versão atual do exemplo: 0.1.25**, para o lote .90.25. Comece pelo [guia incluído no .bmod](Playable/content/LEIA_PRIMEIRO.md) e pelas [oito receitas comentadas](Playable/content/Common/Recipes/).

**Histórico anterior:** **Base Android validada:** Bunny Loader C# .86 (473 callbacks V4 sem falhas, 87 empurrões confirmados visualmente, debuffs funcionais e 60 FPS). **Descoberta automática:** catálogo da API na branch experimental `.87`; não altera a APK.

Este exemplo segue os fontes do Bunny Loader e do ExampleModBunny, sem copiar APIs do tModLoader. Não garante execução direta de mods de PC.

**Inventário gerado automaticamente:** [API C# e diferenças JS/C#](../../docs/generated/csharp_api_catalog.md) · [Receitas de código C# real](../../docs/generated/csharp_api_examples.md) · [Catálogo JSON](../../docs/generated/csharp_api_catalog.json) · [Lacunas e prioridades de API C#](../../docs/generated/csharp_api_missing_priorities.md) · [Testes negativos de assinatura](../../docs/generated/csharp_api_unsupported_compile.md). O status `device_verified` depende de confirmação real no Android, e não apenas de compilar.

**Compilação automática:** [17 casos e resultados](../../docs/generated/csharp_api_compile_results.md) · [Dados JSON dos testes](../../docs/generated/csharp_api_compile_results.json). A bateria usa os arquivos-fonte reais e testa os membros do SDK, sem alterar a APK.

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
| [Playable/content/Content/Prefixes](Playable/content/Content/Prefixes/) | Dois prefixos C# experimentais para armas e acessórios |
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
- **.85.1 + ExampleModCSharp 0.1.13:** 479 chamadas V4, 410 efeitos aplicados, nenhum drop por fila e 60 FPS no Android; cache ativo no segundo carregamento.
- **.86 + ExampleModCSharp 0.1.14:** 473 callbacks V4, 87 impulsos reais confirmados visualmente, sem falhas gerenciadas nem drops e 60 FPS no Android.

A restauração dos três prefixos da .90.11 foi confirmada pelo usuário no Android. Os utilitários e diagnósticos novos da .90.25 têm validação automatizada de host; gameplay desta nova build ainda requer confirmação no aparelho.

Não presumir suporte geral a tModLoader, múltiplos mods C# simultâneos ou multiplayer. Para um novo método, usar o SDK fonte deste repositório, não uma assinatura do PC.




Grupo Lote A .90.28.5: 51 fontes, incluindo WaspNest, ExampleStatAccessory, Beard,
BossMask, Robe e Soul. Leia content/Documentation/LOTE2_ITENS.md dentro do bmod.

## .90.29.1 / 0.1.36 — entrega experimental

Receitas BunnyItem agora passam diretamente pelo servico nativo; identificacao
de ingredientes/estacoes e compartilhada entre JS/C# e nao chama a API JS.
Leia Playable/content/Documentation/RECEITAS_NATIVAS.md para os limites e
roteiro Android. A matriz embutida tem 201 arquivos e inclui dois novos exemplos
de botoes de toque ainda nao portados. Nao foram adicionadas classes nesta
etapa: continuam 51 fontes. A bancada C# e o Lote A completo ainda pendem.
