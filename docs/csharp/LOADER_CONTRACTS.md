# Contratos de hooks do Bunny Loader C#

O arquivo `tools/csharp_contracts/extended_hooks.json` centraliza os **cinco modos estendidos já existentes** no `BunnyUniversalHooks.cs` e no `UniversalHooks.cpp`.

Execute `python3 tools/csharp_contracts/generate.py --write` após mudar um modo e `python3 tools/csharp_contracts/generate.py` para verificar a geração. O gerador mantém `BunnyExtendedHookModes.g.cs` e `UniversalHookModes.generated.h` idênticos ao contrato, verifica o retorno e os parâmetros dos delegates C#, a presença das rotas de registro e das chamadas nativas e falha quando os artefatos ficam desatualizados. O CI verifica os dois lados sem gerar APK.

Os cinco modos cobrem somente as assinaturas de hooks universais já implementadas. Não representam paridade dos 88 loaders JavaScript, nem garantem que um método IL2CPP específico seja seguro para hook. O host continua validando o método, a ABI e a thread do jogo; a tradução de cada comportamento do ExampleModBunny continua exigindo comparação com o código original e QA no Android.

Esta etapa não altera a ABI, o limite de 64 hooks, os prazos de execução, nem o ciclo de vida. Mods C# continuam escritos em C#, sem adicionar dependência de execução JavaScript. A evolução seguinte é usar a mesma abordagem para contratos tipados de Item, Projectile, NPC e Player, incluindo testes de unload e multiprocessamento.
