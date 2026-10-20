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

## Teste sem APK

```sh
dotnet run --project tests/invasion/Invasion.csproj --configuration Release
```

Este teste só compila C#; não executa Gradle, NDK nem empacotamento.
