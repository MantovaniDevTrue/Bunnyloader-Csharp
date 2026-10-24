# Cinco loaders C# — QA Mobile 301720

APK unificado recomendado: **Bunny Loader C#** (`com.bunnyloader.csharptest`,
`1.0.22-csharp-unified-qa`). Substitui o aplicativo C# QA anterior,
sem exigir APK Five QA paralelo. Terraria instalado precisa ser 1.4.5.8.6,
Android ARM64. O pacote contém fontes C# editáveis, sem runtime JS.

1. Instale o APK; importe e ligue `fiveloaderscsharp-0.1.1.bl`. Neste primeiro
   ensaio, deixe os outros mods desligados. Crie um personagem e mundo de QA.
2. No chat do Terraria (não no editor JS), escreva `/lote`. Depois de alguns
   segundos, deve mostrar `mundo=True`, `StatModifier: 40` e `updates=19/19`.
   Se faltar algum callback, a própria resposta dá seu nome. Faça um segundo
   `/lote` para confirmar que as contagens continuam funcionando.
3. Fabrique **QA5 - Varinha de Gore** com uma madeira, sem bancada. Ela tem
   dano-base 40 calculado por StatModifier; prefixos/bônus podem alterar o dano
   mostrado. Atire contra blocos. Um fragmento deve cair após a bala morrer.
   `/lote` mostra pedidos de Gore e recusas. Pedido aceito não prova desenho:
   confira visualmente o fragmento. Não usa poeira como substituto.
4. Abra o editor nativo de controles do Terraria. Atribua a ação
   **QA dos cinco loaders / TestarLote** a um botão; também experimente
   atribuí-la dentro de um grupo de botões. Toque uma vez, segure meio segundo,
   solte e dê dois toques rápidos. Feche o editor e use `/lote`: `press`,
   `hold` e `duplo` precisam aumentar. A linha `TouchHost` deve mostrar
   `registro=ok`, `host=Ready`, `bound=True` e `triggerFrames>0`.
   Se a ação aparecer, mas não responder, envie toda a linha `TouchHost`. Abra o chat e segure o botão: `hold`
   não deve aumentar enquanto o chat estiver aberto. Salve o perfil, reinicie
   o jogo e confirme a ação, o ícone e a posição preservados.
5. Digite `/lote add` três vezes. Anote `pontos` e use **Salvar e sair**.
   Reabra o mesmo mundo: `/lote` precisa restaurar os pontos e aumentar `saves`.
   Abra outro mundo novo: pontos devem começar em zero. Volte ao primeiro:
   o progresso deve continuar lá. Depois feche e reabra o aplicativo e repita.
6. Opcional: `/lote tempo 2` duplica o avanço do relógio; use `/lote tempo 1`
   para restaurar. Isso não acelera os ticks de jogadores, blocos ou NPCs.
   Sair do mundo também restaura o multiplicador padrão.

## Resultado a devolver

Envie o relatório do Bunny depois do ensaio e diga: contagem `updates`,
se viu o fragmento, se o botão/grupo/ícone persistiu e se os pontos voltaram.
Se algo falhar, o relatório e o nome do callback ausente localizam o problema.

## Escopo

Lote: #133 StatModifier/StatInheritanceData, #126 SystemUpdate (19 callbacks),
#121 Gore, #122 WorldSave/WorldNetwork e #120 controles nativos de toque.
StatInheritanceData é uma estrutura de fatores; este lote não adiciona
DamageClass do PC ao Mobile. Organização das pastas segue Common/Systems,
Common/Commands e Content; a ABI segue o Terraria Mobile.

A compilação e os testes de host não certificam FPS ou gameplay no Xiaomi.
O ensaio principal é single player com saves locais. O código de rede tem
validação de codec, autoridade e transporte nos testes; ensaio multiplayer
no Android exige servidor e cliente com esta mesma compilação e o mesmo mod.
O comando de QA bloqueia mutações de cliente. Não desligue um mod em um mundo
aberto: Gore permanece registrado até reiniciar o processo; esse lote não
promete retirada individual de tipos nativos durante a partida.

O APK é uma entrega de QA autorizada deste lote, não uma versão estável nem
uma declaração de que todos os loaders do projeto estão concluídos.
