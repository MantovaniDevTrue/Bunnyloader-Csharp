# ExampleInvasion — C# nativo para Bunny Loader

## Escopo do teste Android

A implementação é experimental: o SDK C# registra um `BunnyInvasionSystem`, o
item `ExampleInvasionBeacon` dispara a invasão e a ponte nativa C++ executa
spawn, morte, recompensas, persistência, rede e HUD no Terraria.
O teste inicial deve ser feito **em mundo local**, sem salvar em nuvem,
preferencialmente em singleplayer antes do host/cliente.

A invasão de exemplo é acessível desde o início do jogo para facilitar o QA.
Não exige derrota de chefe. O `BunnyUseContext` também expõe a condição opcional
`CanStartSurfaceInvasionAfterFirstBoss` para mods que decidirem exigir
`NPC.downedBoss1`.

## Comportamento esperado

1. Registrar `ExampleInvasionBeacon` e fabricar na bancada de trabalho com
   10 `ExampleItem`; o item não deve ser consumido.
2. Usar durante o dia, na superfície, com jogador vivo, mundo carregado e sem
   invasão vanilla. Uso inválido não deve iniciar o evento.
3. A invasão deve iniciar uma única vez, manter progresso após novo uso e
   mostrar avisos e HUD nativos.
4. `ExampleSlimeNPC` deve renderizar e contar 1 ponto ao morrer; o
   `ExampleInvasionEliteSlimeNPC` deve ficar disponível após 10 pontos,
   renderizar e contar 3 pontos. Meta de 30 pontos.
5. Somente inimigos realmente admitidos no evento pontuam. Despawn, duplicação
   de callback, NPC de fora da invasão e alteração de mundo não podem conceder
   pontos extras.
6. Ao completar, o host concede 5 `ExampleItem` uma vez por epoch.
7. O progresso de um **mundo local** deve restaurar ao sair e entrar, sem
   reaproveitar instâncias, tickets ou identidades de NPCs do mundo antigo.
8. Ao hospedar um mundo e conectar um cliente, ativação e mortes devem ser
   autoritativas no host; o cliente recebe HUD/snapshots via
   `invjoin` e `BLINV2`. Reconexão deve recuperar o estado correto.
9. Verificar que o jogo mantém FPS estável e que não há acúmulo de NPCs,
   filas, logs, memória ou travamento durante vários ciclos.

## Implementação

- A lógica C# roda no worker MonoVM com `BunnyInvasionController` e
  `BunnyInvasionSpawnPipeline`. Até 16 invasões por mod; sorteio ponderado,
  limite de NPCs ativos, cooldown, duas ondas, tickets globais e epoch.
- `BunnyItem.OnUse(int, BunnyUseContext)` é opt-in; a assinatura antiga
  `OnUse(int)` permanece compatível. `NativeCSharpItemUse` intercepta o uso
  real do item, valida o jogador e os tipos registrados. Em multiplayer,
  o host valida o pedido recebido antes de encaminhá-lo à lógica C#.
- `NativeInvasionMailbox` limita requisições/respostas e executa o
  `NPC.NewNPC` na thread do Terraria. A identidade do objeto é confirmada
  pela primeira IA do NPC; slots/gerações são protegidos contra reutilização.
- `ReliableNpcKill` usa até 512 eventos pendentes, drena no máximo 16 por
  atualização e deduplica mortes em `CheckActive` e `NPCLoot`.
- `NativeInvasionSpawnSite` pesquisa apenas 9 colunas e no máximo
  53 linhas por coluna. Exige piso sólido contínuo, ar livre de 5×6 tiles,
  rejeita slope/half-block e espinhos do templo (ID 232 confirmado nas fontes
  do Bunny), e consulta colisão nativa sólida, de água e lava. Em caso de
  informação insuficiente, **não** cria NPC.
- O estado local é gravado em `.wld.invasions.bl` com escrita atômica,
  separado por mundo/mod/invasão, sem operações por frame.
- `NativeInvasionNetwork` usa protocolo versionado e tokens de sessão,
  controle de revisões, late join e limpeza periódica de peers inativos.
- `NativeInvasionHud` desenha barras com `MagicPixel` e
  `DrawBorderString`, usando o `SpriteBatch` do Terraria.
- O código desta invasão é C# + C++ nativo; não requer API JavaScript
  nem reutiliza código do tModLoader.

## Limites atuais e critérios de validação

Os testes da CI incluem build do SDK .NET, simulação integrada
C# ⇄ P/Invoke ⇄ C++, testes de NPCs, persistência e rede, além de
verificação de sintaxe Android NDK. **Isso não comprova os hooks IL2CPP
em execução no aparelho**. Ainda é necessário testar as assinaturas de
`Collision.WetCollision`, `Collision.LavaCollision` e hooks de item/NPC/UI,
o efeito de terreno inclinado e os modos 0/1/2/3.

**Mundos em nuvem não estão suportados pelo sidecar local.**
Arte e sons definitivos, tradução completa, balanceamento e acabamento
de release são etapas posteriores ao teste de funcionamento em Android.
Não anunciar suporte total a multiplayer ou nuvem sem validação real.

Nenhum APK foi gerado por esta CI. Manter a PR em *draft* e não
fazer merge/release até finalizar testes reais e corrigir problemas
observados em jogo.

## Rodar testes sem APK

```sh
dotnet build experiments/csharp/BunnyCsSdk/BunnyCsSdk.csproj -c Release
dotnet run --project tests/invasion/Invasion.csproj -c Release
dotnet run --project tests/invasion/net/InvasionNet.csproj -c Release
dotnet run --project tests/invasion/pipeline/Pipeline.csproj -c Release
dotnet run --project tests/invasion/DispatcherIntegration.csproj -c Release
dotnet run --project tests/invasion/TransportIntegration.csproj -c Release
python3 tests/invasion/check_item_use_contract.py
python3 tests/invasion/check_multiplayer_contract.py
python3 tests/invasion/native_spawn_contract.py
g++ -std=c++20 -Wall -Wextra -Werror tests/invasion/NativeSpawnSiteTest.cpp -o /tmp/invasion-terrain
/tmp/invasion-terrain
```

Para `TransportIntegration`, usar o `LD_LIBRARY_PATH` da biblioteca
nativa de simulação construída pelo workflow; o teste não funciona sozinho
sem essa biblioteca.
