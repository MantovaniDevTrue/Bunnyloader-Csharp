# ExampleTooltipItem — preparação do tooltip C# nativo

Fonte de comportamento: `samples/ExampleMod/content/Content/Items/ExampleTooltipItem.js`, Bunny JS 1.9.0.

O `BasicItemWriter` já executava `ModifyTooltips` e `PreDrawTooltipLine` durante a compilação Roslyn e emitia o plano JSON no campo 26 do registro `ITEM`. A implementação anterior somente armazenava esse plano. Este lote introduz `NativeTooltipPlan.h` e faz a admissão C++ interpretar os dados em uma representação tipada, com limites de 24 linhas, 512 bytes de texto por linha, identificadores exclusivos, RGB hexadecimal e parâmetros finitos de animação. Suporta escapes JSON Unicode, incluindo pares substitutos UTF-16, sem QuickJS.

A animação tem o mesmo padrão do fonte: deslocamento vertical `sin(time*speed + index*letterStep)*amplitude` e matiz `time*hueSpeed + index*hueStep`; o plano contém as propriedades da linha `BunnyLoader` e a cor dourada de `Tooltip0`. A política tipada fica associada ao item nativo e pode ser consultada sem retornar ao Mono a cada quadro. `retireBasicItemTooltipsForMod` invalida os planos apenas do UID retirado durante o reset, a substituição e a falha no carregamento.

O teste de host `tests/tooltip_native/native_test.cpp` carrega o registro C# verdadeiro emitido pelo compilador, verifica propriedades e amostras matemáticas e rejeita documentos incompletos, campos indevidos, Unicode inválido e envelopes adulterados. A regressão roda junto com o lote de golfe, sem produzir APK.

`NativeTooltipWave.h` implementa uma rotina C++ independente de QuickJS para preparar medidas de prefixo do texto UTF-16 e desenhar cinco passagens por glifo (quatro sombras e uma letra). Preserva a escala, o espaçamento da fonte, o alfa e os deslocamentos senoidais da origem e rejeita UTF-8 malformado. O teste verifica 55 passagens para os 11 caracteres visíveis de `Bunny Loader`, cores RGB, sombra, espaço e cache de medidas, sem consultar Mono por quadro.

**Limitação:** a substituição do `TooltipLoader.js` pela captura de linhas, ligação à fonte e desenho no Terraria Android **ainda não foi feita**. O algoritmo nativo de cinco passagens existe e passa pelo fixture de host, mas precisa ser conectado aos hooks reais do jogo, à inserção/altura das linhas e à opacidade em tempo de execução. A fidelidade visual no aparelho e a independência funcional de JS ainda não estão validadas; o port continua incompleto.
