# ExampleInvasion — lote C# sem APK

## Estado

A fonte `Content/Systems/ExampleInvasion.cs` demonstra a API
`BunnyInvasionController` do SDK C#. O exemplo reutiliza o NPC C#
`ExampleSlimeNPC`, já existente no pacote. O controlador é testável e
mantém progresso, limite de criaturas, cooldown e deduplicação de mortes.

**Ainda não é uma invasão jogável completa.** O canal host ⇄ MonoVM,
a fila limitada de spawn, o agendamento periódico e o adaptador nativo
estão implementados e passam na CI. Ainda faltam a invocação por item,
persistência, estados de rede, conteúdo visual e validação no aparelho.
Nenhum APK deve ser gerado antes do fechamento integral.

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

## Procura nativa de terreno (lote 5)

`NativeInvasionSpawnSite.h/.cpp` usa o `TileAccess` **já existente no
Bunny** e as tabelas `Main.tileSolid` e `Main.tileSolidTop`.
A busca se limita a 9 colunas candidatas próximas ao local solicitado
e no máximo 53 linhas por coluna; exige espaço livre de 5×6 tiles
sobre piso sólido contínuo de três tiles. Sem local válido, o host
recusa o spawn com `NoSafeSpawnSite`, antes de chamar `NPC.NewNPC`.
A resolução dos campos IL2CPP é cacheada; os arrays de tiles são
revalidados por mundo. Não há leitura de todos os tiles do mapa.

Testes puros verificam chão, plataforma, teto baixo, ausência de
piso, coluna bloqueada e borda de mundo. Compilação de sintaxe
Android é feita com NDK, sem APK.

**Limitação:** a verificação de tile sólido e espaço livre não
inclui líquido, lava, paredes inclinadas, half-block, armadilhas,
nem um teste de colisão final após a criação. Ainda requer
QA de diferentes biomas/terrenos em Android real.

## Canal host ⇄ MonoVM (implementado)

`BunnyInvasionSystem` é descoberto por reflexão no registro de NPCs
do SDK. O `ExampleInvasionSystem` usa o controlador existente, sem
JavaScript. A cada 30 quadros de mundo pronto, o host cria um job
no worker MonoVM, que invoca `BunnyEntitySourceDispatcher.TickWorld`.
Somente mods que possuem uma invasão continuam recebendo esses jobs:
o retorno -2 remove permanentemente o polling desnecessário para
mods sem invasão.

O worker comunica com a biblioteca nativa por P/Invoke, usando
`bunny_invasion_submit` e `bunny_invasion_poll`.
`NativeInvasionMailbox.cpp` limita pedidos a 32 e respostas a 64,
não transfere objetos IL2CPP entre threads, executa até dois spawns
por atualização na thread do Terraria e limpa ambas as filas quando
a sessão do mundo muda. O pedido leva `epoch`, `ticket` e
`nativeWorldSession`; o resultado devolve `slot` e `nativeIdentity`.
`ExampleSlimeNPC` tenta confirmar tanto no `OnSpawn` quanto nos
ticks seguintes de IA, porque a resposta nativa pode chegar depois
da primeira IA.

A CI compila `ManagedRuntime.cpp` e `AutoSourceRuntime.cpp` com
o compilador Android do NDK, **sem linkar aplicativo**, além de simular
a cadeia C# → P/Invoke → C++ → P/Invoke → C# com uma biblioteca
nativa de teste.

O host móvel admite singleplayer (0) e host com jogador (3).
O modo 2 ainda exige seleção de jogador válida em servidor dedicado;
não há validação real no Android ou protocolo de multiplayer da invasão.

**Pendências obrigatórias:** acionamento por item/receita e regras
de início, geração em solo real sem prender inimigos, tratamento de
eventos perdidos por fila de morte sob carga, persistência por mundo,
sincronização host/cliente e entrada tardia, UI/avisos, efeitos,
balanceamento, QA Android/host/cliente e revisão final de release.

Atualmente cada mod C# aceita um `BunnyInvasionSystem`. Suporte a
múltiplas invasões no mesmo mod continua pendente.

## Teste sem APK

```sh
dotnet run --project tests/invasion/Invasion.csproj --configuration Release
dotnet run --project tests/invasion/pipeline/Pipeline.csproj --configuration Release
dotnet run --project tests/invasion/DispatcherIntegration.csproj --configuration Release
python3 tests/invasion/test_npc_hook_contract.py
python3 tests/invasion/check_native_identity_bridge.py
python3 tests/invasion/native_spawn_contract.py
g++ -std=c++20 -Wall -Wextra -Werror -Iapp/src/main/cpp tests/invasion/NativeSpawnSiteTest.cpp -o /tmp/bunny-invasion-terrain
/tmp/bunny-invasion-terrain
python3 tests/invasion/check_world_host_contract.py
g++ -std=c++17 -Wall -Wextra -Werror tests/invasion/WorldAuthorityTest.cpp -o /tmp/bunny-invasion-authority
/tmp/bunny-invasion-authority
```

A CI também compila os arquivos nativos contra os cabeçalhos reais e
faz verificação de sintaxe Android com o NDK. Não executa Gradle,
não gera APK e não comprova funcionamento no aparelho.
