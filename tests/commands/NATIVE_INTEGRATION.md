# CommandLoader C# — contrato de integração (ainda incompleto)

Referências do Bunny original, sem transposição de código do tModLoader:

- `app/src/main/cpp/script/js/mod/ModCommand.js`
- `app/src/main/cpp/script/js/mod/Loaders/CommandLoader.js`
- `samples/ExampleMod/content/Common/Commands/HealCommand.js`
- `samples/ExampleMod/content/Common/Commands/ExampleTimeCommand.js`

## Implementado e verificável agora

A API `BunnyCommand` e o registro `BunnyCommandRegistry` residem no SDK C#.
O `NativeCommandMailbox` reside em `app/src/main/cpp/experiments/csharp`
e não usa JS, QuickJS, JNI ou objetos IL2CPP internamente.

O mailbox aceita até 64 trabalhos pendentes, até 128 respostas em buffer,
até 1024 bytes por pedido e até 512 bytes por linha de resposta. Retém no
máximo 16 linhas por resposta, limita registros, preserva o destinatário
original e exige ticket válido para concluir exatamente uma vez.

Um registro é atômico por mod. Aliases são normalizados em ASCII e são
permitidas colisões **entre** mods; o dispatcher C# decide quando responder
com `/mod:comando`. Em caso de conflito, o C++ só reconhece o nome.
`newWorld` e `unregisterMod` invalidam tickets anteriores por época.
O worker nunca precisa guardar ponteiros IL2CPP.

```sh
dotnet run --project tests/commands/Commands.HostTests.csproj -c Release
g++ -std=c++20 -O2 -Wall -Wextra -Werror -pedantic -pthread \
  -Iapp/src/main/cpp tests/commands/native_mailbox_test.cpp \
  -o /tmp/native-command-test
/tmp/native-command-test
```

Esses testes são executados pelo GitHub Actions em workflows **sem APK**.

## Exemplos prontos para validação

- `samples/ExampleModCSharp/Experimental/CommandLoader/ExampleHealCommand.cs`:
  `/heal <quantidade>` e `/curar <quantidade>`. O código explica a divisão
  entre validação no MonoVM e alteração da vida na thread do Terraria.
- `samples/ExampleModCSharp/Experimental/CommandLoader/ExampleTimeCommand.cs`:
  `/addTime <ticks>`, com `BunnyCommandType.World` e módulo do ciclo de
  86400 ticks. O servidor é a autoridade sobre a hora.
- `tests/commands/Actions.HostTests.csproj` executa ambos os exemplos;
  `native_command_clock_test.cpp` verifica viradas dia/noite.
- Uma cópia fiel dos três exemplos (`ExampleEchoCommand`, `ExampleHealCommand`,
  `ExampleTimeCommand`) está em `samples/ExampleModCSharp/Playable/content/Common/Commands/`
  para validação direta em um mundo de testes. As versões originais permanecem
  em `Experimental`; nenhuma APK foi gerada e nada foi validado em aparelho.

## Lote complementar: descoberta e transporte verificados

- O `SourceParser` aceita `BunnyCommand` sem `SetDefaults`. O
  `SourceLoader` aceita mods somente de comandos e contabiliza seus tipos.
- `CommandManifestWriter` valida nomes, aliases e permissões a partir
  da DLL compilada. O manifesto `.dll.commands` conserva SHA-256 da DLL e
  é verificado no C++ antes de registrar nomes/aliases no mailbox.
- Um cache de compilação que tenha manifesto ausente ou divergente é
  rejeitado e recompilado, sem permitir registro de dados obsoletos.
- `BunnyCommandWorker` oferece `RegisterAssembly`, `Execute`,
  `Unregister` e `Clear` no SDK C#, devolvendo respostas limitadas em
  hexadecimal UTF-8. `NativeCommandResult.h` valida essas respostas e
  mantém o destinatário preso ao ticket do C++.
- O `ManagedRuntime.cpp` conecta o SDK C# à thread proprietária do Mono:
  registra assemblies e despacha lotes de até oito pedidos por ciclo.
  O mailbox só passa a reconhecer comandos depois de esse registro
  retornar sucesso. Isso evita roubar entradas do chat vanilla no boot.
- `NativeCommandChatHooks.cpp` resolve métodos de
  `ChatCommandProcessor`, `ChatHelper`, `ChatMessage`, `Main`,
  `NetworkText` e `Color`, verificando cada assinatura e retorno antes
  de instalar hooks. Suporta caminhos Chat/Server/World para
  netMode 0, 1, 2 e 3, com mensagens locais e destinadas a clientes.
  Se os metadados não conferirem, o hook fica desarmado.
- Testes host C# e C++ verificam parser, fila, hash, aliases,
  conflitos e transporte de respostas; o NDK faz compilação de sintaxe
  ARM64 das unidades nativas, **sem APK**. Isso não substitui prova
  funcional com o Terraria real.

## Encadeamento real que precisa ser implementado

1. **Descoberta Roslyn — IMPLEMENTADA NO HOST:** nomes, aliases, classes
   exportadas, contagem e SHA256 passam pelo compilador e pelo registro
   nativo. Execução no Android real ainda não foi validada.
2. **Hooks de chat — IMPLEMENTADOS NO CÓDIGO, NÃO VALIDADOS EM JOGO:**
   as duas rotas IL2CPP são instaladas somente após validação de assinatura.
   Comandos desconhecidos seguem ao original; os registrados e prontos
   são consumidos. Falta ensaio no Terraria Android em netMode 0/1/2/3,
   inclusive interação com comandos dos mods JS.
3. **MonoVM — IMPLEMENTADO NO CÓDIGO, NÃO VALIDADO EM JOGO:** o worker
   gerencia `RegisterAssembly` e `Execute` no processo que executa
   `ManagedRuntime`, processa lotes de oito e rejeita tickets de época
   antiga. Falta QA de attach/lookup real, troca rápida de mundos e
   concorrência com callbacks de NPC/projétil.
4. **Respostas na thread do jogo — IMPLEMENTADAS NO CÓDIGO, NÃO VALIDADAS
   EM JOGO:** `Main.NewText` ou `ChatHelper.SendChatMessageToClient`
   executam no game thread, nunca no MonoVM; verificar o ABI do `Color`
   e o formato real de `NetworkText` no dispositivo.
5. **Ações que mudam o mundo — IMPLEMENTADAS NO CÓDIGO:** `BunnyCommandCaller.Heal`
   e `AdvanceWorldTime` geram mensagens `ACTION` tipadas. O C++ confere
   época, jogador e permissões novamente na thread do jogo; realiza a cura,
   chama `HealEffect` e confirma apenas o valor aplicado. O relógio usa
   a aritmética testada de dia/noite do ExampleTimeCommand, e o servidor
   envia `WorldData`. Faltam ensaios reais de ABI e sincronização Android.
6. **Ajuda — IMPLEMENTADA NO CÓDIGO:** `CommandManifestWriter` exporta
   `BunnyCommand.Description` em HEX UTF-8, validado pelo parser nativo.
   `ComposeMessage` recebe o texto original do Terraria/JS e acrescenta
   somente comandos C# prontos, sem entradas duplicadas. Ainda falta
   validar o método IL2CPP real e os conflitos multi-mod em aparelho.
7. **Sessões — IMPLEMENTADAS NO CÓDIGO:** entrar/sair do mundo invalida
   ações pendentes e desativa aliases; o worker chama `Clear` e registra
   novamente as classes na sessão seguinte. Ainda faltam testes Android
   de troca rápida, hot-unload/reload de mods e instâncias compartilhadas.
8. **Android:** testar solo, cliente, servidor hospedado no telefone
   (netMode 3), duas instâncias, colisões entre mods, chat vanilla,
   save/reload, 60 FPS, cancelamento e troca rápida de mundo.

**Bloqueio de release:** qualquer item pendente impede merge, pacote para
uso real, APK e marcação da categoria como `validada`. Um teste host verde
comprova somente o núcleo isolado, não os hooks Android.
