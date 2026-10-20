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

## Encadeamento real que precisa ser implementado

1. **Descoberta Roslyn:** instanciar todas as classes públicas
   `BunnyCommand` da DLL, validar aliases, nomes e permissões e transmitir
   descritores ao C++ sob a identidade real do mod; aceitar mods somente de
   comandos, sem item/projétil artificial. Preservar cache e invalidação.
2. **Hook do chat:** mapear no Android as assinaturas do Bunny original
   `ChatCommandProcessor.ProcessIncomingMessage(ChatMessage,int)` e
   `ChatHelper.SendChatMessageFromClient(ChatMessage)`. Instalar hooks C++
   uma vez, na thread do jogo, somente depois de confirmar o método IL2CPP.
   Se o mailbox devolver `Vanilla`, chamar o original inalterado.
   `Queued` significa consumir a mensagem; `Busy` significa consumir e
   responder que a fila está ocupada, sem mandar o comando ao vanilla.
3. **MonoVM:** no worker proprietário da VM, pegar um lote limitado de
   `takeRequests`, executar o registry C# correto e enfileirar respostas
   tipadas usando `deliver(epoch, sequence, replies)`. Nunca bloquear o
   desenho esperando `Action`; qualquer resultado com época antiga falha.
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
