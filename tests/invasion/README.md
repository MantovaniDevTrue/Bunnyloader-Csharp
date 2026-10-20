# ExampleInvasion — lote C# sem APK

## Estado

A fonte `Content/Systems/ExampleInvasion.cs` demonstra a API
`BunnyInvasionController` do SDK C#. O exemplo reutiliza o NPC C#
`ExampleSlimeNPC`, já existente no pacote. O controlador é testável e
mantém progresso, limite de criaturas, cooldown e deduplicação de mortes.

**Ainda não é uma invasão jogável.** O SDK e o adaptador nativo
`NativeInvasionSpawn.cpp` já possuem implementação e testes separados,
mas falta o canal de execução host ⇄ MonoVM e o ciclo automático
de spawn. Nenhum arquivo deve virar APK antes do fechamento integral.

## Contrato obrigatório do host

1. O host deve decidir se está em servidor/singleplayer, bloquear
   mutações de clientes e chamar `Start(true)` somente após uma invocação
   real e válida, com um único evento ativo por mundo.
2. Uma atualização de mundo deve enviar ticks autoritativos ao worker
   MonoVM usando `Pipeline.Tick(true, jogadoresDisponíveis)`. Quando há vaga,
   `TrySpawn(true, sorteio, out pedido)` gera `Ticket + Epoch`.
   O host executa `spawnNativeInvasionNpc` **na thread do Terraria**,
   com o nome e tipo do mod verificados. A resposta retorna `slot +
   nativeIdentity` e deve ser encaminhada ao worker com
   `NativeSpawnCreated(true, epoch, ticket, slot, identity)`.
   Se o spawn falhar, chamar `NativeSpawnFailed` com o mesmo epoch/ticket.
   A primeira IA real do NPC chama `OnSpawn`, que confirma
   `slot + generation + nativeIdentity` antes de contabilizar o inimigo.
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
vaga sem pontuar. Sem reserva confirmada pelo primeiro tick com a identidade nativa,
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

## Adaptador nativo e confirmação

`NativeInvasionSpawn.cpp` consulta o tipo já registrado do mod, valida
autoridade, mundo, jogador e slot livre; resolve `NPC.NewNPC` pela
assinatura existente no Bunny original. Não usa JavaScript.
O resultado carrega `epoch/ticket/slot/identity`; `BunnyInvasionSpawnPipeline`
aguarda a primeira atualização do NPC para descobrir a geração.
Pedidos sem confirmação expiram; a mudança de epoch rejeita respostas
antigas e o processo não credita NPCs que surgiram fora da invasão.

O dispatcher gerenciado agora dispõe de um overload `TickNpc` de
13 argumentos e recebe a identidade original já presente no
`ManagedEntityJob`. O caminho nativo usa a assinatura de 13 argumentos,
enquanto o overload antigo de 12 permanece disponível a chamadas C#.

**O canal host ⇄ MonoVM que conecta o pedido de spawn ao adaptador
e devolve o resultado ainda falta.** A função nativa não está sendo
invocada automaticamente. Também faltam item, UI, persistência e
multiplayer verificado em aparelho.

## Teste sem APK

```sh
dotnet run --project tests/invasion/Invasion.csproj --configuration Release
dotnet run --project tests/invasion/pipeline/Pipeline.csproj --configuration Release
dotnet run --project tests/invasion/DispatcherIntegration.csproj --configuration Release
python3 tests/invasion/test_npc_hook_contract.py
python3 tests/invasion/check_native_identity_bridge.py
python3 tests/invasion/native_spawn_contract.py
g++ -std=c++17 -Wall -Wextra -Werror tests/invasion/WorldAuthorityTest.cpp -o /tmp/bunny-invasion-authority
/tmp/bunny-invasion-authority
```

A CI também compila o adaptador C++ contra os cabeçalhos reais do projeto.
São testes estáticos/nativos e .NET; não executam Gradle, NDK nem
empacotamento e não comprovam funcionamento no aparelho.
