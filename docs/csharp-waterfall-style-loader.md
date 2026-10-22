# WaterfallStyleLoader C# — lote independente, sem APK

Base de trabalho: branch integrada v1.0. Este lote é independente das PRs paralelas de
Cloud, Hair, Gore, SceneEffect, SystemUpdate, Dust e receitas.

## Contrato de referência

- tModLoader: ModWaterfallStyle, Slot, Texture, SetStaticDefaults,
  AddLight(int i, int j), ColorMultiplier(ref float r, ref float g, ref float b, float a).
- Bunny original: Loaders/WaterfallStyleLoader.js e ModWaterfallStyle.js.
- Terraria Android 1.4.5.8.6: os primeiros 28 tipos são vanilla; o exemplo JS
  aumenta a tabela WaterfallManager.waterfallTexture a partir do índice 28.
- Referência externa: https://docs.tmodloader.net/docs/stable/class_mod_waterfall_style.html

O C# segue a assinatura float do tModLoader. O JS usa Ref de componentes de
cor 0..255; copiar essa conversão diretamente para o C# quebraria o contrato.

## Implementado neste commit (somente fontes)

- Classe pública ModWaterfallStyle com o contrato tML e Slot estável por sessão.
- WaterfallStyleLoader: IDs determinísticos por ordem de registro entre mods,
  detecção de duplicatas, verificação de caminhos/texturas, limite configurado,
  Seal/Activate, leitura do snapshot imutável e Unload de sessão.
- ColorMultiplier com três referências float, rollback da cor em exceções/NaN.
- AddLight com coordenadas de tiles, amostragem de 3 em 3 até 24 tiles,
  limite de emissões por callback e encaminhamento para IWaterfallLightSink.
- Exemplo C# com a textura PNG ORIGINAL do ExampleModBunny, copiada pelo SHA
  do blob Git (sem converter, redesenhar nem perder qualidade).
- Testes de host que compilam o SDK e o exemplo, sem build de Android.

## Ainda necessário antes de considerar COMPLETO

1. Descobrir automaticamente as classes ModWaterfallStyle nos assemblies C#
   compilados pelo Roslyn, respeitando identidade de cada pacote e dependências.
2. Resolver o arquivo PNG no pacote ativo, carregar Texture2D na thread adequada
   e ampliar WaterfallManager.waterfallTexture com rollback completo de falhas.
3. Selecionar o Slot C# durante a escolha real de cascata por ModWaterStyle /
   bioma, mantendo os estilos vanilla intactos.
4. Conectar os hooks nativos de DrawWaterfall/cores e a fila de luzes a IL2CPP;
   não executar Mono callbacks na render thread nem fazer varreduras massivas.
5. Garantir descarte de assets, hooks e filas na troca de mundo/recarga dos mods;
   confirmar os limites de multiplayer e cache por frame com logs verificáveis.
6. Executar a suíte de host e um QA de jogo, depois de TODO o caminho acima estar
   implementado. O Android é exigido para atestar fidelidade visual e FPS.

Enquanto os itens acima estiverem pendentes, o estado é **WIP / source-only**.
A classe compilável sozinha não significa que o Terraria esteja chamando
AddLight ou ColorMultiplier. NÃO gerar APK, NÃO publicar como funcional,
NÃO marcar as APIs como device_verified.

## Uso no mod

Veja samples/ExampleModCSharp/Playable/content/Content/Biomes/ExampleWaterfallStyle.cs

O loader atribui Slot depois de verificar a textura. O arquivo precisa estar
no caminho Texture relativo ao diretório content/ do mod. O método
WaterfallLighting.AddLight enfileira a luz para execução futura pelo host e
só é permitido dentro do callback AddLight. A textura não pertence ao
tModLoader desktop e deve ser validada no Android.

## Comando da suíte de host

    dotnet run --project tests/csharp_waterfall/CSharpWaterfallTests.csproj -c Release

A CI executa apenas .NET SDK e testes; não invoca Gradle, não assina nem
publica APK. Resultado verde em host não equivale a validação Android.
