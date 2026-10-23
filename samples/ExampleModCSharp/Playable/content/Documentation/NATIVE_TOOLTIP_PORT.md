# ExampleTooltipItem — preparação do tooltip C# nativo

Fonte de comportamento: `samples/ExampleMod/content/Content/Items/ExampleTooltipItem.js`, Bunny JS 1.9.0.

O `BasicItemWriter` já executava `ModifyTooltips` e `PreDrawTooltipLine` durante a compilação Roslyn e emitia o plano JSON no campo 26 do registro `ITEM`. A implementação anterior somente armazenava esse plano. Este lote introduz `NativeTooltipPlan.h` e faz a admissão C++ interpretar os dados em uma representação tipada, com limites de 24 linhas, 512 bytes de texto por linha, identificadores exclusivos, RGB hexadecimal e parâmetros finitos de animação. Suporta escapes JSON Unicode, incluindo pares substitutos UTF-16, sem QuickJS.

A animação tem o mesmo padrão do fonte: deslocamento vertical `sin(time*speed + index*letterStep)*amplitude` e matiz `time*hueSpeed + index*hueStep`; o plano contém as propriedades da linha `BunnyLoader` e a cor dourada de `Tooltip0`. A política tipada fica associada ao item nativo e pode ser consultada sem retornar ao Mono a cada quadro. `retireBasicItemTooltipsForMod` invalida os planos apenas do UID retirado durante o reset, a substituição e a falha no carregamento.

O teste de host `tests/tooltip_native/native_test.cpp` carrega o registro C# verdadeiro emitido pelo compilador, verifica propriedades e amostras matemáticas e rejeita documentos incompletos, campos indevidos, Unicode inválido e envelopes adulterados. A regressão roda junto com o lote de golfe, sem produzir APK.

**Limitação:** a substituição do `TooltipLoader.js` pela captura de linhas, medição da fonte e desenho nativo no Terraria Android **ainda não foi feita**. A renderização de cinco passagens (quatro sombras + letra), a inserção/altura das linhas, a opacidade e a validação visual no aparelho continuam pendentes. Este lote não reivindica fidelidade visual concluída, independência funcional de JS ou paridade total do ExampleMod.
