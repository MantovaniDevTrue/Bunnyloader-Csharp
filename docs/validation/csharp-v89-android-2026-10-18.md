# Validação Android — Bunny C# .90.29.89

## Origem e alcance

Em 18/10/2026, o usuário informou: “Testei e funcionou perfeitamente”, acompanhado do relatório gerado às 01:00. Esta é uma confirmação de funcionamento do fluxo entregue de receitas do ExampleMod, relatada pelo usuário; não é uma execução de testes no dispositivo pelo agente.

Bunny Loader 0.1.10.16.90.29.89-csharp-native-recipe-groups, ExampleMod CSharp 0.1.48, Lights And Shadows CSharp Native 0.4.2.3 e Weapon Out Lite 1.1.1. Plataforma: Xiaomi 2109119DG, Android 14/API 34, arm64-v8a, Terraria Android 1.4.5.8.6 (301720).

A confirmação não discrimina individualmente todas as variantes de madeira/barra, consumo de ingredientes, ausência de materiais ou reentrada. Não promover esses casos específicos a testes detalhados concluídos sem evidência adicional. O exemplo oficial Content/ExampleRecipes.cs continua Parcial e o Lote A permanece em andamento.

## Evidências da sessão atual

- Relatório: bunny_2026-10-18_00-58-34.txt, processo 4488, SESSION_START unixMs=1792295914208.
- MonoVM: phase=verified, domainInitialized=1, methodExecuted=1, result=42, error vazio.
- Nenhum erro ou aviso listado na última partida.
- Dois mods C# carregados: 14 itens, dois prefixos, dez receitas, cache=2, compilados=0, preparo=609 ms.
- DefaultsRequest dos dois mods: Cache=Hit; CompileMs=0. Entrada no mundo registrada em 5,2 s.
- Save/Exit registra seis células de tiles de mod, 22 itens de personagem e dois itens em baús; callbacks pausados e UnityPlayerQuitted. Esses registros comprovam operações de gravação observadas, não uma bateria completa de restauração.
- Snapshot de combate: 76 OnHitNPC e 91 OnKill executados, error=0, quatro dropped registrados. Não classificar a fila como livre de descartes.
- Os motivos históricos SIGKILL/sem memória de outros processos não demonstram uma queda nesta sessão.

## Desempenho: separar cache de primeira compilação

Os 609 ms medem preparação com cache, não compilação fria. O snapshot de compilação é anterior (13.741 s no relatório), processo 26466: emitMs=5160, totalMs=5780, nativeWallMs=6254. A reutilização/verificação de 190 ms desse worker não substitui o custo original de compilação.

A meta de primeira compilação em até cinco segundos permanece pendente; a evidência histórica disponível supera esse limite. O relatório não contém uma medição de FPS da .89. Não afirmar 60 FPS ou ausência de vazamentos a partir deste teste.

## Código e CI

Código testado: b22962178a6beafa34301caa89a0bced46a598bb, branch feat/csharp-native-recipe-groups-v89, PR [85](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/85).

[Build Android 39080974810](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39080974810): success. Inclui compilação/execução dos contratos C#, wire do fixture compilado para o pump nativo sem QuickJS, smoke Mono, AOT e APK assinado V3. Nove gates independentes aprovados no mesmo SHA:

- [Check Bunny CSharp Native Visual Queue](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39080976963): success.
- [Check Mono Android signal ownership](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39080976965): success.
- [Check Mono pinned worker GC transitions](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39080976966): success.
- [Check Bunny CSharp Visual Queue](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39080976967): success.
- [CSharp Native ScreenFx](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39080977001): success.
- [Check CSharp combat batch](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39080976992): success.
- [Check managed callback lifecycle](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39080976962): success.
- [Check complete ExampleMod audit](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39080976986): success.
- [Check CSharp SDK and continuous runtime foundation](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39080976976): success.

Entrega privada experimental confirmada: tag csharp-v3-run-39080974810-a1; APK BunnyLoader-CSharp-0.1.10.16.90.29.89-csharp-native-recipe-groups-SignedV3.apk e ExampleModCSharp-v0.1.48.bmod. Lights permanece 0.4.2.3. Este registro documental não altera os binários entregues.

## Retomada

Preservar a .89 e os IDs persistidos. Continuar pelas lacunas compartilhadas de receitas: grupos customizados e membros, múltiplas estações, condições, callbacks e operações globais. Consultar a linha ExampleRecipes do inventário oficial e o plano global antes de ampliar a API. A validação de um fluxo não conclui o arquivo oficial inteiro, os demais exemplos nem o Lote A.
