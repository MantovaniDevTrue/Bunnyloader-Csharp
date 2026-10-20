# ExampleInvasion — lote C# sem APK

## Estado

A fonte `Content/Systems/ExampleInvasion.cs` demonstra a API
`BunnyInvasionController` do SDK C#. O exemplo reutiliza o NPC C#
`ExampleSlimeNPC`, já existente no pacote. O controlador é testável e
mantém progresso, limite de criaturas, cooldown e deduplicação de mortes.

**Ainda não é uma invasão jogável completa.** O canal host ⇄ MonoVM,
a fila limitada de spawn, o agendamento periódico e o adaptador nativo
estão implementados e passam na CI. Também existem invocador, salvamento
por mundo, retries de mortes, múltiplas invasões por mod, rewards
nativos, alertas no chat e encaminhamento autenticado de item cliente→host.
Ainda falta um transporte real host→cliente para snapshots versionados,
saves em nuvem, HUD visual completo e QA no aparelho.
Nenhum APK deve ser gerado antes do fechamento integral.

## Contrato obrigatório do host

1. O host deve decidir se está em servidor/singleplayer, bloquear
   mutações de clientes e chamar `Start(true)` somente após uma invocação
   real e válida, respeitando a política de eventos do mod.
2. Uma atualização de mundo deve enviar ticks autoritativos ao worker
   MonoVM usando `Pipeline.Tick(true, jogadoresDisponíveis)`. Quando há vaga,
   `TrySpawn(true, sorteio, out pedido)` gera `Ticket + Epoch`.
   O host executa `spawnNativeInvasionNpc` **na thread do Terraria**,
   com o nome e tipo do mod verificados. A resposta retorna `slot +
   nativeIdentity` e deve ser encaminhada ao worker com
   `NativeSpawnCreated(true, epoch, ticket, slot, identity)`.
   Se o spawn falhar, chamar `NativeSpawnFailed` com o mesmo epoch/ticket.
   O dispatcher C# verifica a IA do NPC, que confirma
   `slot + generation + nativeIdentity` antes de contabilizar o inimigo.
3. O dispatcher encaminha morte e despawn a cada controlador registrado;
   apenas o dono da reserva do NPC pontua. O buffer nativo de mortes
   reenvia eventos que encontraram o MonoVM ocupado, com deduplicação.
4. Mudança/saída de mundo, unload e troca de personagem chamam
   `WorldExit`. Não transportar contadores nem membros de outro mundo.
5. Incluir sincronização servidor → clientes de início, progresso e
   conclusão, entrada tardia, UI/avisos, condições de vitória e
   falhas de reconexão. O salvamento local por mundo usa sidecar
   nativo, mas os snapshots não constituem protocolo de rede.
6. Incluir efeitos/recursos visuais finais, item de invocação, receitas,
   teste em Android e teste com host/cliente antes de qualquer APK.

## Integração ao ciclo real dos NPCs

O `BunnyEntitySourceDispatcher` faz os encaminhamentos genéricos
de `OnSpawn`, `OnKill` e `OnDespawn` para todos os controladores
registrados, usando `Slot/Generation/NativeIdentity`. O mesmo NPC
não pontua se não pertencer a uma reserva confirmada. O
`ExampleSlimeNPC` não depende mais de chamadas manuais para
`ExampleInvasion`. Os eventos de morte do C++ usam
`ReliableNpcKill`, com até 512 retries e drenagem limitada a 16
eventos por atualização; o buffer limpa na troca de mundo.

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

## Persistência de invasão por mundo (lote 7)

`BunnyInvasionWorldPersistence` salva a fase, o progresso e o
cooldown na extensão `.wld.invasions.bl`, usando
`Main.ActiveWorldFileData` e os campos `FileData._path` e
`_isCloudSave` já usados no Bunny original. O arquivo é
`bunny-invasions 1`, com uma entrada identificada por
`modId + invasionId`. Outros mods e invasões armazenados no mesmo
arquivo são preservados. Leitura e gravação pertencem ao worker MonoVM,
e o host fornece apenas o caminho verificado na thread do Terraria.

A gravação usa um arquivo temporário, `fflush`, `fsync` e
`rename` atômico. Arquivos com versão desconhecida, registros
inválidos ou limites excedidos não são sobrescritos. Mundo em nuvem
e sessão antiga não têm acesso à persistência local. Toda transição
de mundo invalida o caminho anterior.

Na primeira atualização gerenciada após a entrada no mundo,
`TickWorld` tenta restaurar o progresso. Inimigos ativos, tickets
de spawn e identidades antigas **não** são restaurados; o
controlador incrementa a geração lógica e recria ondas novas.
A gravação ocorre na mudança de fase/progresso (início, mortes,
conclusão) e, quando necessário, no máximo uma vez a cada
600 quadros para atualizar o cooldown. Não há escrita em disco
por frame nem por NPC gerado. Em `KillNpc` e no caminho de hit
letal, a alteração é gravada antes de a execução do job terminar.

Testes C++ cobrem mundos independentes, dois mods no mesmo
arquivo, retorno ao mundo anterior, progressão, conclusão,
arquivos corrompidos, nuvem e sessões inválidas. O teste
end-to-end C# → biblioteca nativa real → C# valida que sair
e reentrar recupera o progresso sem revalidar NPCs antigos.

**Limites:** suporte de mundo na nuvem não está implementado.
O cooldown é persistido periodicamente, sem escrita por quadro. Falhas de armazenamento
devem ser diagnosticadas no Android antes da conclusão do loader.
A sincronização host/cliente ainda precisa de protocolo próprio.
Os arquivos de teste são locais ao runner e nenhum APK é gerado.

## Uso nativo de item C# (lote 6)

`BunnyItem.OnUse(int playerIndex)` é um callback opt-in. O dispatcher
descobre por reflexão somente os `BunnyItem` que sobrescrevem esse
método, registra o nome local do item por P/Invoke e consulta uma
fila limitada no tick MonoVM. Não há chamada Mono por frame de item.

`NativeCSharpItemUse.cpp` registra um único hook verificado na assinatura
`Player.ItemCheck_StartActualUse(Item)`, já usada pelo Bunny.
O método original do jogo roda normalmente; antes dele, o hook
captura o tipo do item e o índice do jogador. A implementação
identifica o item por `modItemTypeByKey(mod + "/" + name)`,
admitindo apenas tipos realmente registrados pelo mod.
A fila suporta até 64 bindings e 64 usos; sessões do mundo antigas
não são reaproveitadas. O host expõe `bunny_item_use_ready` para
confirmar que o tipo de item foi resolvido e o hook instalado
antes de o dispatcher C# começar a consumir eventos; testes cobrem
respostas atrasadas de instalação. O cliente 1 registra apenas o hook e encaminha o uso ao host pelo
canal nativo `NetRequests`; o servidor valida o índice do cliente,
o item selecionado e a propriedade do item ao mod antes de
enfileirar o evento para MonoVM. Modos 0/2/3 usam callbacks
autoritativos. Itens sem `OnUse` são ignorados.

O novo `ExampleInvasionBeacon` é um `BunnyItem` não consumível com
receita na bancada de trabalho usando 10 `ExampleItem`.
`OnUse` chama `ExampleInvasion.Start(true)`. Chamadas repetidas
durante a invasão não reiniciam o progresso, como os testes
de ida e volta C# → C++ → C# demonstram. A textura atual é
reutilizada do `ExampleItem`; arte final permanece pendente.

**Limites:** o pedido cliente→host está no código, mas não foi
testado com jogadores conectados no Android. A confirmação da
requisição ainda usa o chat nativo do Bunny, que não equivale
a um transporte de snapshots host→cliente. Mods registrados
pelo dispatcher que possuem apenas itens podem usar o novo hook;
a descoberta de pacotes sem quaisquer entidades C# precisa
de validação adicional no carregador. A API não foi validada no aparelho.
Não substituir testes Android por simulações.

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
Somente mods com invasão ou itens com `OnUse` continuam recebendo
esses jobs: o retorno -2 remove o polling de mods sem ambos.

O worker comunica com a biblioteca nativa por P/Invoke, usando
`bunny_invasion_submit` e `bunny_invasion_poll`.
`NativeInvasionMailbox.cpp` limita pedidos a 32 e respostas a 64,
não transfere objetos IL2CPP entre threads, executa até dois spawns
por atualização na thread do Terraria e limpa ambas as filas quando
a sessão do mundo muda. O pedido leva `epoch`, `ticket` e
`nativeWorldSession`; o resultado devolve `slot` e `nativeIdentity`.
O dispatcher tenta confirmar a identidade na IA sem alocações,
inclusive se a resposta nativa chegar após o primeiro `OnSpawn`.

A CI compila `ManagedRuntime.cpp` e `AutoSourceRuntime.cpp` com
o compilador Android do NDK, **sem linkar aplicativo**, além de simular
a cadeia C# → P/Invoke → C++ → P/Invoke → C# com uma biblioteca
nativa de teste.

O host móvel admite singleplayer (0), cliente apenas para hooks de item
(1), servidor dedicado (2) selecionando um jogador ativo a cada 30
quadros e host-jogador (3). O protocolo `BunnyInvasionWireState`
é versionado, limitado e rejeita snapshots antigos, incluindo
entrada tardia e troca de mundo nos testes .NET. Seu canal real
de envio host→cliente ainda precisa ser instalado e testado.

**Pendências obrigatórias:** canal real host→cliente de snapshots;
validação Android do item de invocação, start rules e gameplay multiplayer;
cobertura de lava/líquidos, blocos inclinados, armadilhas e colisão real;
verificação de saves Android/nuvem; HUD visual e assets definitivos;
efeitos sonoros/visuais; balanceamento, QA Android/host/cliente
e revisão final de release.

Até 16 `BunnyInvasionSystem` independentes por mod são aceitos,
com tickets globais únicos, spawn em turnos e persistência por ID.
`ExampleInvasionSystem` premia cinco `ExampleItem` pela ponte
nativa de `giveItem`, com deduplicação por sessão/epoch.
O servidor publica marcos de 10% e mudanças de fase como avisos
leves pelo `Main.NewText` já usado no Bunny original, sem render
por quadro. Ainda não é um HUD gráfico completo.

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
python3 tests/invasion/check_item_use_contract.py
python3 tests/invasion/check_world_save_contract.py
python3 tests/invasion/check_multiplayer_contract.py
dotnet run --project tests/invasion/net/InvasionNet.csproj --configuration Release
dotnet run --project tests/invasion/MultiInvasionIntegration.csproj --configuration Release
g++ -std=c++17 -Wall -Wextra -Werror tests/invasion/WorldAuthorityTest.cpp -o /tmp/bunny-invasion-authority
/tmp/bunny-invasion-authority
```

A CI também compila os arquivos nativos contra os cabeçalhos reais e
faz verificação de sintaxe Android com o NDK. Não executa Gradle,
não gera APK e não comprova funcionamento no aparelho.
