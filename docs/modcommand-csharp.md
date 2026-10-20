# ModCommand C# — lote em desenvolvimento

Referência: `Split/Bunny-Loader/samples/ExampleMod/content/Common/Commands/`
(HealCommand.js e ExampleTimeCommand.js) e
`app/src/main/cpp/script/js/mod/Loaders/CommandLoader.js` no mesmo fork.

A implementação C# **não depende do JS**, não substitui os handlers do JavaScript
e não está registrada como recurso concluído. O código foi isolado na branch
`feature/csharp-modcommand-core-20261020`.

## O que entrou no núcleo

- `BunnyCommand`, `BunnyCommandType` e `BunnyUsageException`.
- `BunnyCommandDispatcher` sem estado global: registra, executa, resolve aliases,
  nomes repetidos, `/mod:comando`, `/help` e descarrega por mod.
- `BunnyCommandCaller` com resposta limitada, localização, jogador e mundo.
- `BunnyCommandClock` para mudanças do ciclo dia/noite com ticks negativos.
- Duas portas em C# dos exemplos originais, com comentários de contratos delicados.
- Testes de roteamento solo/cliente/host, erro de uso, colisão, alias, unloading,
  exceptions, limites, cura e sincronização solicitada ao host.

Os métodos `IBunnyCommandPlayer` e `IBunnyCommandWorld` são interfaces de host
e **não são implementações nativas de Terraria**. Eles permitem testar fielmente
as regras sem esconder integrações ausentes.

## O que falta antes de chamar este loader de completo

- [ ] Detectar e registrar as classes `BunnyCommand` no carregador real de mods C#,
      preservando a identidade do pacote e a ordem de inicialização.
- [ ] Conectar os eventos nativos de chat e `/help` com IL2CPP,
      sem usar o CommandLoader.js nem QuickJS para C#.
- [ ] Construir os adaptadores reais de jogador, mundo, localização e resposta
      em Main.NewText / SendChatMessageToClient / WorldData.
- [ ] Garantir que o lado servidor execute `World` com autoridade e que
      chat local do host (netMode 3) não execute o mesmo comando duas vezes.
- [ ] Sincronizar um contrato seguro entre a thread do jogo e MonoVM com
      timeout e rollback de eventos, sem bloquear o render.
- [ ] Incluir testes integrados de carga/descarregamento, colisões entre mods,
      alterações de idioma, multiplayer real e Android 14 arm64.
- [ ] Promover os dois exemplos para `Playable` somente depois dos itens acima.
- [ ] Validar fidelidade e desempenho em dispositivo; só então marcar a matriz.

## Teste de host sem APK

```sh
dotnet run --project Split/Bunny-Loader/tests/command_core/CommandCore.csproj -c Release
```

O script de CI específico compila apenas projetos C# e executa testes;
não monta, assina nem publica APK.

**Regra de release:** APK proibido enquanto existir qualquer item pendente.
Não mover este lote para a base jogável ou publicar uma versão como completa
com base apenas em testes de host.
