# Próximo grupo: WaterStyle + WaterfallStyle

Auditoria de continuação sobre `32d6ffa2`. **Ambos continuam pendentes**; não há
API C# de água/cachoeira exposta nem teste Android dessa integração. Esta página
evita repetir a descoberta dos contratos e não conta como loader implementado.

Referências lidas integralmente: `script/js/mod/Loaders/WaterStyleLoader.js`
(306 linhas, SHA-256 a6d6b854c04f5642f9f05e1b699619de8ece59eeae8e155a429b72b1cc6a790c),
`WaterfallStyleLoader.js` (132 linhas, cb9243465c3baf8a471019e7f9edf0cc0a5234d3fc8931f6a9e4226552b34c5a),
`ModWaterStyle.js`, `ModWaterfallStyle.js` e os dois exemplos de
`build/reference-examplemod/content/Content/Biomes`. Caminhos de runtime abaixo
são relativos a `app/src/main/cpp`.

| Contrato original | Mecanismo que precisa existir no serviço compartilhado |
| --- | --- |
| Add/Get/Current/TotalCount | UID/classe, slots intercalados C#/JS, referências por chave e contagem nativa validada, nunca IDs exclusivos do Example |
| Texturas da água | LiquidRenderer._liquidTextures, TextureAssets.Liquid e LiquidSlope; fallback da rampa para bloco e dos assets ausentes para o estilo original, com diagnóstico e leases |
| Fades e limites de DrawWaters | liquidAlpha/activeLiquidAlpha no LocalUserGameState recriado; ampliar todas as tabelas antes de publicar o total usado nos dois laços nativos |
| Escolha por cena | Priority >= BiomeLow; preservar fontes quando ignoreFountains=false, lua de sangue à noite e bgStyle vanilla 1..8/13/14; não substituir no menu |
| Cachoeira visível | Depois de WaterfallManager.Draw original, uma DrawWaterfall por estilo de água visível, com seu alpha e seu ChooseWaterfallStyle |
| Splash | Dust.dustWater → GetSplashDust, preservando o original quando não houver estilo de mod |
| Gotas de gore | Apenas dentro de TileDrawing.EmitLiquidDrops; resolver GetDropletGore, inicializar corretamente a física do tipo escolhido, alias vanilla de Update e restauração do tipo se continuar ativo |
| Luz na água | LightingEngine e LegacyLighting, _workingLightMap.LightDecayThroughWater, Vector3 real; resultado multiplicado por .91, sem perda de largura/arredondamento |
| MakeRain/NewRain | MakeRain com waterStyle temporário 0 e restauração; variantes customizadas só para as gotas efetivamente criadas; manter sorteio original quando não sobrescrito |
| NewRainForced | Cobrir também Eyebrella, que não passa por MakeRain |
| Textura da chuva | GetRainTexture por caminho/asset, cache/leases, trocar TextureAssets.Rain só durante DrawRain e restaurar inclusive em falha |
| Tintura de bioma | Lambda real de DyeInitializer.LoadLegacyHairdyes, Color/by-ref bool validados, um ponto por canal por frame; preservar Shimmer e alpha original |
| Cachoeira: cor | Gate DrawWaterfall com estado aninhado; três overloads reais de SpriteBatch.Draw, incluindo Color por ref e VertexColors de quatro pontas; restaurar valores de referência depois do desenho |
| Cachoeira: iluminação | Só entradas waterfall.type=0 visíveis; pontos a cada 3 tiles, até min(stopAtStep,24), respeitando currentMax e o array real; AddLight original em cada ponto |

Não transportar uma chamada MonoVM síncrona por sprite/partícula. Agrupar trabalho
quando preservar o mesmo número de callbacks e sua ordem; um cache que remove
chamadas aleatórias, ignora entradas ou troca a física não preserva o contrato.
O caminho padrão pode continuar nativo quando o callback não foi sobrescrito.
Dados temporários e texturas precisam de revisão, limites, roots e leases como
na infraestrutura de superfície. Não copiar o layout de VertexColors ou Vector3
do PC nem tratar Color e Color por ref como a mesma assinatura.

## Exemplos e dependências reais

`ExampleWaterStyle` utiliza a cachoeira de exemplo, BlueCrystalShard, o gore
`ExampleDroplet`, multiplicadores de luz 1/1/1, chuva própria e cabelo branco.
`ExampleWaterfallStyle.AddLight` adiciona .5/.5/.5 em cada ponto. O exemplo não
sobrescreve ColorMultiplier, mas o loader precisa oferecer esse callback também.

Assets originais conferidos pelo IHDR:

| Arquivo em Content/Biomes | Dimensões |
| --- | --- |
| ExampleWaterStyle.png | 48 × 1360 |
| ExampleWaterStyle_Block.png | 306 × 16 |
| ExampleWaterStyle_Slope.png | 72 × 16 |
| ExampleWaterfallStyle.png | 512 × 40 |
| ExampleRain.png | 12 × 42 |

Esses cinco PNGs ainda não estão na pasta C#; portar com os exemplos completos.
`Content/Gores/ExampleDroplet.png` já está no ExampleMod C#, e BunnyGore possui
lookup/configuração/spawn. **IsDrip/SpecialAI não demonstram o alias de Update
706 usado pelo loader JS.** Implementar/verificar essa dependência reutilizável,
preservando a ordem dos hooks de outros mods e restaurando o tipo corretamente.
Biome/SceneEffect e música do bioma continuam dependências separadas; um comando
de QA do Example pode testar o renderer, mas não concluir o bioma por associação.

## Revisão do serviço anterior e correção

**Atualização:** o risco abaixo foi corrigido nos fontes e coberto por testes em
[csharp-water-native-safety.md](csharp-water-native-safety.md). Os dois contratos
de contagem/arrays estão implementados; os loaders completos continuam pendentes.

### Estado observado antes da correção

`content/tiles/ModWater.cpp` já oferece setWaterStyleCount, independente de VM.
Entretanto chama patchLoopEnd duas vezes e só depois confere se ambos retornaram
1. Pela leitura de `hook/CodePatch.cpp`, cada chamada escreve imediatamente; se
o segundo padrão/escrita falhar, o primeiro pode já ter sido alterado e g_count
não é atualizado. Isso é uma observação verificável no código, **não uma falha
reproduzida no dispositivo**. Planejar/verificar os dois padrões antes de qualquer
escrita e tratar permissões/publicação transacional antes de reutilizar o serviço.

Tests do grupo precisam executar serviços reais sem QuickJS e depois C#/JS no
mesmo processo: assets, tabelas recriadas, padrões de código incompatíveis sem
escrita parcial, prioridade/fonte/lua, duas iluminações, gota/alias, chuva normal e
forçada, restauração em exceção, três overloads de cor, iluminação dos pontos e
unload em ambas as ordens. Manter 3/15 até esses contratos estarem implementados.
