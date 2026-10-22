# CommandLoader C# — testes sem APK

O nucleo `BunnyCommandRegistry` esta integrado aos fontes do SDK, mas **ainda nao intercepta o chat do Terraria**. O exemplo fica em `samples/ExampleModCSharp/Experimental` para nao fingir que funciona no jogo.

```sh
dotnet run --project tests/commands/Commands.HostTests.csproj -c Release
```

A suite verifica: argumentos, aliases, sensibilidade a maiusculas, resposta, conflitos entre mods, namespace `/mod:comando`, mundo local, cliente, host e dedicado, registro atomico, descarregamento, excecoes, ajuda e limites.

Nao empacotar mod nem APK. A finalizacao do loader exige, nesta ordem:

1. Descoberta e registro de classes `BunnyCommand` no compilador e runtime MonoVM.
2. Ponte C++ exclusiva para `ProcessIncomingMessage` e `SendChatMessageFromClient` (nao usar QuickJS), com chamado a original em comandos nao reconhecidos.
3. Conversao segura de respostas para chat local/servidor; manter `/help` vanilla e implementar extensao das descricoes.
4. Bind de `/heal` e `/addTime` por APIs nativas reais, com validacao de jogador e autoridade no servidor.
5. Testes de compilador, C++ host, IL2CPP ARM64, carregamento, rede, unload, FPS e validacao no jogo.

O objetivo e entregar uma API completa, nao marcar como pronto ao compilar este nucleo. O prototipo nao faz acesso direto ao Terraria, e nao gera APK.
