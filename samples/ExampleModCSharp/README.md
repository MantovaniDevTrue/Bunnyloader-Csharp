# Android Hook QA 0.1.47

Pacote temporario que usa o scanner IL2CPP .85 para H3 Wiring.HitSwitch, H4 Player.CalculateFrameFromMiscCounter e H5 WorldGen.Check1x1. Metodos presentes no Android, callbacks ainda em teste. Estado real no relatorio, nao na mensagem OnEnterWorld. Guia: `Playable/content/Documentation/HOOK_QA_ANDROID.md`.

# Android Hook QA 0.1.47

ExampleMod temporario para a APK .84. H1 int() validado previamente; H2-H5 testam assinaturas IL2CPP com candidatos e diagnostico de qual metodo foi aceito. O resumo real aparece no relatorio Bunny Loader, nao na mensagem de OnEnterWorld. Leia `Playable/content/Documentation/HOOK_QA_ANDROID.md`.

# ExampleMod CSharp 0.1.47 — QA temporario

Use com APK .84 para testar os cinco hooks. O OnEnterWorld e precomputado e nao exibe contadores ao vivo. Os resultados verdadeiros aparecem na secao `hooks universais C#` do relatorio Bunny Loader. Detalhes em `Playable/content/Documentation/HOOK_QA_ANDROID.md`.

# Android QA .83 — ExampleMod CSharp 0.1.47 (TEMPORARIO)

Pacote preparado para validar em aparelho os cinco hooks tipados da API CSharp .83. Todos os cinco estao ligados automaticamente somente nesta versao de diagnostico e se desligam apos ate duas chamadas. A mensagem `ExampleWelcomePlayer.OnEnterWorld` resume H1-H5; nomes e erros em `Playable/content/Documentation/HOOK_QA_ANDROID.md`. Os cinco metodos de Terraria sao candidatos nao confirmados na build Android. Nao ha alteracao de retorno nem simulacao de eventos artificiais; assim `ARMADO` e diferente de `OK1/OK2`. O probe antigo `ResetEffects` permanece inativo.

# Batch de cinco assinaturas C# da .83

ExampleMod CSharp 0.1.42 inclui cinco exemplos isolados dos hooks:
1. `ExampleHookInt32Zero`: Gera inteiro aleatorio vanilla e preserva retorno; teste apenas em mundo descartavel; assinatura `Int32Zero`
2. `ExampleHookVoidInt32`: Observa chamada de procura de casa de NPC; nao modifica parametro; assinatura `VoidInt32`
3. `ExampleHookVoidInt32Pair`: Observa colocacao de cactos com coordenadas; metodo mexe em tiles; assinatura `VoidInt32Pair`
4. `ExampleHookInt32Triple`: Conta fios na area e retorna inteiro original, opcionalmente pode substituir valor em teste; assinatura `Int32Triple`
5. `ExampleHookVoidInt32Triple`: Observa conversao de tile no Terraria; metodo altera mundo e precisa teste em mundo descartavel; assinatura `VoidInt32Triple`

O modo de jogatina instala zero hooks universais deste lote por padrao, inclusive o antigo Native Access Probe de ResetEffects. Todos os exemplos ficam em `Playable/content/Common/Hooks/`, com `EnableAtLoad = false` e status `Calls` / `LastError` por classe. O usuario pode habilitar apenas um para testar a ABI em mundo descartavel. Os exemplos reproduzem os conceitos a partir do SDK C# proprio, sem codigo JavaScript ou tModLoader.

# Exemplo unificado — Bunny CSharp .82 / ExampleMod 0.1.41

O ExampleMod agora inclui as mesmas tres armaduras nativas e texturas de ArmorPoliciesCSharp (sem criar segundos itens), alem de uma implementacao integrada do NativeAccessProbe. Os pacotes separados continuam somente como fixtures de regressao e nao sao necessarios para jogar na .82. Desative os mods antigos antes de testar o pacote unificado.

O bind da assinatura Android de `UnifiedRandom.Next(int,int)` tambem e testado sem instalar o hook. Consulte `ExampleNativeAccess.PairMethodPresent` e `PairMethodError` para diagnostico. A fonte unica do teste read-only fica em `Playable/content/Common/Hooks/ExampleNativeAccess.cs`. Intercepta `Terraria.Player.ResetEffects` uma vez, chama o original e amostra os campos de vida, posicao, dia/noite e indice de On Fire a cada 64 chamadas, sem escrever. O teste da assinatura `int(int,int)` esta em `Playable/content/Common/Hooks/ExampleIntPairHook.cs`, inicialmente **desativado** por ser uma funcao de RNG muito chamada. O modulo pode ser ativado por CSharp chamando `ExampleModCSharp.ExampleIntPairHook.Start()` e parado com `Stop()`. `Status()` oferece contadores; `OverrideSmallRange` so deve ser habilitado em mundo descartavel, pois altera aleatoriedade.

`Terraria.Utilities.UnifiedRandom.Next(int,int)` esta presente nas fontes publicas Terraria de PC. **A presenca exata nos metadados IL2CPP Android 1.4.5.8.6 ainda sera verificada no Xiaomi**; se o registro falhar, o metodo retorna diagnostico, sem instalar um hook incorreto. Nao habilitar o teste de RNG durante gameplay comum ou geracao de mundo.

> Estado desta branch: consulte o [guia atual do SDK](../../../../docs/SDK-CSharp.md).
> Efeitos de ExampleStatAccessory sao nativos; StrongBees de WaspNest tambem,
> enquanto a textura Back continua no adaptador JS. Para um exemplo pequeno,
> veja [NativeAccessoryCSharp](../NativeAccessoryCSharp/README.md).
> As notas de versoes abaixo preservam o historico.

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

**Versão atual do exemplo: 0.1.25**, para o lote .90.25. Comece pelo [guia incluído no .bmod](Playable/content/LEIA_PRIMEIRO.md) e pelas [oito receitas comentadas](Playable/content/Common/CombatHelpers/).

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
