# ExampleInvasion — lote C# sem APK

## Estado

A fonte `Content/Systems/ExampleInvasion.cs` demonstra a API
`BunnyInvasionController` do SDK C#. O exemplo reutiliza o NPC C#
`ExampleSlimeNPC`, já existente no pacote. O controlador é testável e
mantém progresso, limite de criaturas, cooldown e deduplicação de mortes.

**Ainda não é uma invasão jogável.** O SDK, isoladamente, não cria NPCs
no Terraria. Nenhum arquivo aqui deve ser empacotado ou anunciado como
recurso completo antes do fechamento da integração nativa.

## Contrato obrigatório do host

1. O host deve decidir se está em servidor/singleplayer, bloquear
   mutações de clientes e chamar `Start(true)` somente após uma invocação
   real e válida, com um único evento ativo por mundo.
2. Uma atualização de mundo no thread do jogo chama
   `Controller.Tick(true, jogadoresDisponíveis)`. Quando há vaga, ela
   usa `TrySpawn(true, sorteio, out pedido)` e cria o NPC via Terraria
   nativo. Deve chamar `ConfirmSpawn` com slot+generation reais após
   confirmação ou `SpawnFailed` se a criação falhar.
3. O evento real de morte do NPC deve chamar `EnemyKilled` uma única vez.
   Despawn normal chama `EnemyDespawned`, sem conceder pontos. O slot
   deve estar vinculado ao pedido confirmado da mesma invasão.
4. Mudança/saída de mundo, unload e troca de personagem chamam
   `WorldExit`. Não transportar contadores nem membros de outro mundo.
5. Incluir sincronização servidor → clientes de início, progresso e
   conclusão, entrada tardia, UI/avisos, condições de vitória, salvamento
   real por mundo e falhas de reconexão. Os snapshots atuais servem
   para persistência lógica, não são um protocolo de rede.
6. Incluir efeitos/recursos visuais finais, item de invocação, receitas,
   teste em Android e teste com host/cliente antes de qualquer APK.

## Integração ao ciclo real dos NPCs

O `ExampleSlimeNPC.OnKill` agora encaminha a morte do NPC gerenciado
para `ExampleInvasion.EnemyKilled`, usando o par `Slot/Generation` já
fornecido pelo `BunnyEntitySourceDispatcher`. `OnDespawn` devolve a
vaga sem pontuar. Sem uma reserva confirmada por `ConfirmSpawn`,
o mesmo NPC não altera o progresso.

A auditoria identificou que `NpcAiHost.cpp` rejeitava eventos
gerenciados quando `Main.netMode != 0`. O contrato móvel do Bunny
usa 0 para singleplayer, 1 para cliente, 2 para servidor e 3
para host que também joga. Os pontos de entrada de IA e morte agora
usam `worldIsAuthoritative` para admitir apenas 0, 2 e 3.
`CheckActive` e `NPCLoot` foram protegidos contra geração
de eventos de morte em clientes.

A CI inclui o `BunnyEntitySourceDispatcher` real para provar que
`OnKill` registra somente mortes admitidas; também verifica
os quatro modos nativos e faz uma auditoria estática de todos os
pontos de entrada C++. **Essa checagem não substitui executar o
hook IL2CPP no Android real.**

## Teste sem APK

```sh
dotnet run --project tests/invasion/Invasion.csproj --configuration Release
dotnet run --project tests/invasion/DispatcherIntegration.csproj --configuration Release
python3 tests/invasion/test_npc_hook_contract.py
g++ -std=c++17 -Wall -Wextra -Werror tests/invasion/WorldAuthorityTest.cpp -o /tmp/bunny-invasion-authority
/tmp/bunny-invasion-authority
```

Este teste só compila C#; não executa Gradle, NDK nem empacotamento.
