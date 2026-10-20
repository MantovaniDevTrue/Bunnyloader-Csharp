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
- Testes C# verificam o assembly **somente de comandos** passando no
  `CompiledAssemblyProbe.Verify`; testes C++ validam parser, fila,
  hash da DLL, colisões e serialização. Nenhuma destas validações
  instala um hook ou significa execução em Android.

## Encadeamento real que precisa ser implementado

1. **Descoberta Roslyn — IMPLEMENTADA NO HOST:** nomes, aliases, classes
   exportadas, contagem e SHA256 passam pelo compilador e pelo registro
   nativo. Falta ativar os comandos dentro do worker MonoVM do jogo.
2. **Hook do chat:** mapear no Android as assinaturas do Bunny original
   `ChatCommandProcessor.ProcessIncomingMessage(ChatMessage,int)` e
   `ChatHelper.SendChatMessageFromClient(ChatMessage)`. Instalar hooks C++
   uma vez, na thread do jogo, somente depois de confirmar o método IL2CPP.
   Se o mailbox devolver `Vanilla`, chamar o original inalterado.
   `Queued` significa consumir a mensagem; `Busy` significa consumir e
   responder que a fila está ocupada, sem mandar o comando ao vanilla.
3. **MonoVM — PENDENTE:** a API `BunnyCommandWorker` está pronta e testada
   no host, mas falta ligá-la ao worker MonoVM proprietário em
   `ManagedRuntime.cpp`. Ele deve pegar um lote limitado de
   `takeRequests`, executar os comandos C# e devolver respostas via
   `deliver(epoch, sequence, replies)`. Nunca bloquear o desenho; descartar
   resultados da sessão antiga.
4. **Game thread:** drenar `takeReplies` e usar as rotas reais
   `Main.NewText` ou `ChatHelper.SendChatMessageToClient`. Não chamar
   qualquer método Terraria através de C# no worker.
5. **Ações que mudam o mundo:** portar `/heal` e `/addtime` com pedidos
   tipados de alteração aplicados exclusivamente na thread do jogo. Uma
   confirmação de cura precisa refletir a vida realmente curada; `WorldData`
   deve ser transmitido pelo servidor em netMode 2 e 3.
6. **Ajuda:** consultar o registry do mod para compor `/help` junto à
   implementação vanilla, como faz o Bunny original, sem duplicar entradas.
7. **Sessões:** ao descarregar mod ou trocar mundo, invalidar tanto as filas
   nativas quanto bindings e objetos da sessão Mono. Não reaplicar respostas
   ou ações da sessão anterior.
8. **Android:** testar solo, cliente, servidor hospedado no telefone
   (netMode 3), duas instâncias, colisões entre mods, chat vanilla,
   save/reload, 60 FPS, cancelamento e troca rápida de mundo.

**Bloqueio de release:** qualquer item pendente impede merge, pacote para
uso real, APK e marcação da categoria como `validada`. Um teste host verde
comprova somente o núcleo isolado, não os hooks Android.
