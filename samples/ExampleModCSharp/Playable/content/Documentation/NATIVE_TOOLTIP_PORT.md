# ExampleTooltipItem — preparação do tooltip C# nativo

Fonte de comportamento: `samples/ExampleMod/content/Content/Items/ExampleTooltipItem.js`, Bunny JS 1.9.0.

O `BasicItemWriter` já executava `ModifyTooltips` e `PreDrawTooltipLine` durante a compilação Roslyn e emitia o plano JSON no campo 26 do registro `ITEM`. A implementação anterior somente armazenava esse plano. Este lote introduz `NativeTooltipPlan.h` e faz a admissão C++ interpretar os dados em uma representação tipada, com limites de 24 linhas, 512 bytes de texto por linha, identificadores exclusivos, RGB hexadecimal e parâmetros finitos de animação. Suporta escapes JSON Unicode, incluindo pares substitutos UTF-16, sem QuickJS.

A animação tem o mesmo padrão do fonte: deslocamento vertical `sin(time*speed + index*letterStep)*amplitude` e matiz `time*hueSpeed + index*hueStep`; o plano contém as propriedades da linha `BunnyLoader` e a cor dourada de `Tooltip0`. A política tipada fica associada ao item nativo e pode ser consultada sem retornar ao Mono a cada quadro. `retireBasicItemTooltipsForMod` invalida os planos apenas do UID retirado durante o reset, a substituição e a falha no carregamento.

O teste de host `tests/tooltip_native/native_test.cpp` carrega o registro C# verdadeiro emitido pelo compilador, verifica propriedades e amostras matemáticas e rejeita documentos incompletos, campos indevidos, Unicode inválido e envelopes adulterados. A regressão roda junto com o lote de golfe, sem produzir APK.

`NativeTooltipWave.h` implementa uma rotina C++ independente de QuickJS para preparar medidas de prefixo do texto UTF-16 e desenhar cinco passagens por glifo (quatro sombras e uma letra). Preserva a escala, o espaçamento da fonte, o alfa e os deslocamentos senoidais da origem e rejeita UTF-8 malformado. O teste verifica 55 passagens para os 11 caracteres visíveis de `Bunny Loader`, cores RGB, sombra, espaço e cache de medidas, sem consultar Mono por quadro.

`NativeTooltipDrawSession.h` agora prepara a inserção da linha `BunnyLoader` antes da descrição `Tooltip0`, desloca os desenhos seguintes uma altura de linha e preserva a cor `FFD75A` apenas na passagem principal da descrição. O `NativeRarityGameplay.cpp` conecta essa política à cadeia existente de `SpriteBatch.DrawString` e `Main.MouseText_DrawItemTooltip`. A vinculação consulta `Main.HoverItem` e a política do próprio UID C#; a captura é desativada para outros itens e em unload. `SpriteFont.MeasureString` mede cada prefixo uma vez por fonte/texto, a altura também é armazenada e os glifos reutilizam raízes gerenciadas limitadas a ASCII. Quando não há plano compatível ou falta uma API nativa, a rotina retorna ao desenho do jogo sem tentar reconstruir a UI em JavaScript.

O teste de host cobre o plano compilado do ExampleTooltipItem, uma única inserção, deslocamento das linhas posteriores, nenhum desenho indevido para outro mod, recusa de política que mantém o desenho padrão, cor dourada, 55 passagens de glifo e Unicode na decodificação. O compilador ARM64 valida o código de integração com o hook real, mas não simula um SpriteBatch Android.

**Limitação:** o suporte de desenho nativo conectado é específico ao layout `ItemName` / `BunnyLoader` / `Tooltip0` do exemplo. Ainda faltam verificação de painel/limites da tela, desenho visual no Terraria Android, coexistência com hooks visuais de mods JavaScript, tradução dinâmica da descrição e generalização a todos os planos de tooltip. O teste de host e a compilação ARM64 não demonstram fidelidade visual no aparelho. O port continua incompleto e nenhum APK foi gerado.

O ajuste de isolamento da sessão de desenho limita os deslocamentos à mesma instância de `SpriteBatch` e `SpriteFont` e a uma região relativa à linha `Tooltip0` (até 64 px horizontalmente e de -4 a +320 px verticalmente). A verificação do texto dourado também exige a mesma fonte, batch e posição da linha âncora. Isso impede que a alteração mova ou pinte textos de outras interfaces, camadas, fontes ou itens fora da região. O teste de host inclui esses cenários, as quatro passagens de sombra, uma única inserção e casos fora do intervalo. É uma proteção conservadora, não um sistema completo de layout: layouts de tooltip mais largos ou maiores podem manter o desenho vanilla, e ainda não há QA de painel no Android.

## Propriedade do desenho e coexistência entre C# e JavaScript

A instalação nativa expõe `tooltipNativeReady(type)`, verdadeiro apenas quando os hooks nativos de texto estão instalados e o plano do item pode ser executado pelo `NativeTooltipDrawSession`. O host valida antecipadamente a propriedade nativa `Main.GlobalTimeWrappedHourly` como `Single`, evitando habilitar a animação se o relógio não estiver disponível.

O `TooltipLoader.js` mantém a renderização antiga para mods JavaScript e para os casos em que o renderizador C# não está pronto. Quando o plano C# está pronto, não há necessidade de reconstruir o tooltip inteiro em JS. Quando há painel opaco, o JavaScript continua calculando o tamanho do fundo até existir uma implementação nativa validada dessa etapa. Outros hooks JS — GlobalItem, ModItem, prefixos, raridades, moedas e classes de dano — continuam tendo precedência.

A API `tooltipJsOverride(bool)` sinaliza o intervalo em que o JavaScript é dono do desenho. O sinal é específico da thread e o valor anterior é restaurado em `finally`; durante esse intervalo, o gancho nativo não insere uma segunda linha `Bunny Loader`. O `TooltipLoader` também compara o wire C# atual ao cache; recompilações atualizam o plano e unload o remove, impedindo conteúdo visual de uma versão antiga.

O teste automatizado `tests/tooltip_native/js_ownership_test.cjs` executa a implementação real de seleção do loader com NativeReady ligado/desligado, painel opaco, raridade/prefixos/hooks JS, revisão do wire e descarregamento. O workflow de host executa esse teste, o decodificador C++ e a compilação do exemplo. Não há DLL C# chamando a API JavaScript: a integração JS é apenas compatibilidade do loader existente com mods JavaScript.

**Pendente no Android:** validar visualmente a animação, limites e altura do painel com escala móvel, posicionamento do dedo, hooks simultâneos de outros mods, recarga dentro do jogo, fontes diferentes e 60 FPS. O fundo opaco ainda usa o caminho JS quando necessário; não afirmar paridade total até implementar e testar o tamanho do fundo nativamente. Nenhum APK foi produzido.

## Limites da tela e fallback conservador

O `TooltipLoader.js` usa o mesmo contexto de posicionamento que seu renderizador original (`Settings.Instance.Tooltips.Scale`, `Distance.Y`, `DrawOffset.Y`, posição normal ou `overloadedMousePositionY` e `Main.screenHeight`). Com dados faltantes, escala não suportada, tela pequena, controle com banner ou falta de espaço vertical, ele mantém o renderer JS, que já executa a medição das linhas, desloca o tooltip e dimensiona o painel opaco. O limite de segurança é conservador: reserva o maior entre 180 px e 240 px multiplicados pela escala, sem presumir que um tooltip genérico tenha altura fixa. A seleção é testada no `js_ownership_test.cjs` com borda inferior, cursor sobrecarregado, escala e tela estreita.

No C++ o hook valida o tipo e o tamanho do campo nativo `Main.screenHeight` e aplica também `nativeTooltipHasHeadroom` à posição já calculada pelo Terraria, à altura real da linha e à escala. Se faltar espaço, não injeta o texto nem desloca outras linhas. O teste C++ cobre limites positivos, negativos, escala e tamanho de tela. Isso não substitui o cálculo completo do fundo do PC ou do Terraria: painéis opacos permanecem no caminho JS e a integração nativa ainda exige inspeção visual real no Android.

A proteção é intencionalmente conservadora e pode usar o fallback JavaScript mais vezes perto das bordas. Ela não deve ser registrada como paridade visual completa ou benchmark de FPS no aparelho. Não gera APK.

## Geometria C++ do painel — etapa de preparação

A política `NativeTooltipPanelLayout.h` replica a parte geométrica do `TooltipLoader.js` do Bunny Loader: medição de cada linha, largura máxima, altura acumulada, inserção da linha C# uma única vez, escala já aplicada às medidas, âncora à esquerda/direita conforme o ponteiro, posição sobrecarregada pelo jogo, distâncias, deslocamento configurado, padding do painel opaco, limites do dispositivo e deslocamento causado pelo banner do controle. Se não houver espaço físico, a operação é recusada sem modificar o estado de saída. A política também recusa layouts com valores NaN, tamanhos exagerados ou banner que não permita acomodar todo o painel.

O arquivo `tests/tooltip_native/panel_layout_test.cpp` valida cenários equivalentes às fórmulas do loader móvel, incluindo o exemplo de quatro linhas, cursor nos dois lados, borda inferior, ponteiro sobrecarregado, painel com e sem padding, tela menor, posição do banner e entrada inválida. O teste é compilado com `-Wall -Wextra -Werror` no workflow de golfe, sem QuickJS, Mono nem APK.

**Importante:** neste lote há um **calculador de geometria nativa**, não um desenhista de fundo conectado ao Terraria. O método `GUIPanel.DrawBacking` contém argumentos gerenciados, structs e possíveis diferenças de ABI ARM64; não será chamado a partir do C++ sem validação de assinatura, disposição dos tipos e hook seguro em dispositivo real. A prioridade do fallback JavaScript para painéis opacos permanece ativa. A paridade de tamanho/posição com o jogo e o FPS real seguem pendentes no Android.

## Preparação da ponte GUIPanel DrawBacking

A ponte `NativeTooltipPanelBacking.h` mantém um único descritor nativo e valida a classe de painel e a overload exata com cinco parâmetros por metadados IL2CPP. Ela exige `Vector2` com dois campos `Single` em offsets 0/4, `Color` com quatro canais `Byte` reais, `Texture2D` como referência gerenciada e `Nullable<Rectangle>` com `hasValue` e um `Rectangle` de quatro `Int32`. Retorno `Void`, nomes dos parâmetros, tipo de textura e largura das estruturas são conferidos; qualquer divergência impede a publicação do descritor.

O método `draw` da ponte usa `runtime_invoke` validado, nunca converte o endereço de `DrawBacking` para um ponteiro de função presumindo a ABI de structs do ARM64. As posições e dimensões são recebidas de `NativeTooltipPanelLayout`. O retângulo opcional está sem valor e as exceções gerenciadas são tratadas como falha; a textura continua sendo propriedade do jogo.

O teste de host `tests/tooltip_native/panel_backing_test.cpp` usa metadados falsos tipados para exercitar a ponte verdadeira, verifica os argumentos enviados à invocação simulada, valida o layout de cor e rejeita overload ambígua, parâmetro errado, tipo de textura incompatível, structs deslocadas e exceções gerenciadas. A inclusão no `NativeRarityGameplay.cpp` também verifica que o cabeçalho compila com o NDK ARM64 no workflow da PR.

**Não está ativo no Terraria:** ainda não existe resolução comprovada do namespace real do `GUIPanel`, conexão às texturas/cor dos settings móveis, chamada em jogo, desativação do fallback JS ou teste visual Android. O teste host valida apenas o contrato esperado pelo fonte JS; não demonstra que o jogo real exponha exatamente esse contrato. Se a metadata for diferente, o binder deve rejeitar sem desenhar. Nenhum APK foi gerado.

## Fontes reais de textura e cor do painel — contratos C++

O loader Bunny identifica a classe gráfica por `bl.classOf('', 'GUIPanel')` (por exemplo em `ModConfigMenu.js` e `HostSettingsMenu.js`). A ponte `NativeTooltipPanelBacking` foi corrigida para exigir esse namespace vazio em vez de inventar `Terraria.UI.GUIPanel`. O teste rejeita explicitamente o owner errado.

A rotina original `TooltipLoader.js/#DrawBacking` escolhe `Settings.Instance.Tooltips.Backing.BackingColor`, com overrides por `item.tooltipContext`: 29 duplicação, 32 Void, 35 bandeira, 15 loja, 3 baú, 44 pesquisa, 4 banco, 5 prefixo, 47 estação de crafting; contextos 8–12 usam `Equip0/1/2TooltipBacking` conforme loadout 0/1/2; contextos 16–20 e 33 usam `Equip0TooltipBacking`. Todos os demais preservam `BackingColor`. A cor da borda vem de `Backing.BorderColor`; a textura do fundo, de `Backing.Backing.Texture`; e a da borda, de `Backing.Border.Texture`.

O `NativeTooltipBackingPalette.h` implementa esses nomes originais em C++, com teste exaustivo dos contextos e loadouts. O `NativeTooltipPanelTexturePath.h` implementa um acesso tipado a `Settings → Tooltips → Backing → Backing/Border → Texture`, usando somente campos gerenciados ou getters com metadados de tipo verificados. Não conserva referências brutas entre mundos; cada leitura valida a cadeia de objetos e retorna falha sem alterar os resultados se um objeto estiver nulo, for de outro tipo ou gerar exceção. O acesso a campos confere offsets e tamanho da instância para não ler fora do objeto.

O fixture `tests/tooltip_native/texture_path_test.cpp` verifica getters, tipo `Texture2D`, textura nula, erro gerenciado, objetos de outro tipo, campos inválidos e correção do resultado após nova leitura. A integração no código nativo usa os headers reais; ainda não lê os valores reais de `Settings.Instance` no aparelho nem trata o possível caso de `Backing` ser uma estrutura por valor. Esse caso deve cair no fallback, não ser reinterpretado como referência.

**Pendente:** resolver o objeto concreto `Settings.Instance`, validar se cada membro é campo, getter, classe ou estrutura no IL2CPP Android e extrair as duas cores verdadeiras. Só depois ligar ambas as chamadas ao `GUIPanel.DrawBacking`, testar o painel em diferentes escalas e desligar o fallback JS opaco. Nenhuma cor fixa ou textura alternativa foi inventada. Sem APK.

## Cores gerenciadas da borda e dos contextos

O `NativeTooltipPanelColors.h` resolve a cor efetiva do tooltip e a cor da borda a partir dos membros originais selecionados por `NativeTooltipBackingPalette`, sem manter paletas RGB inventadas. Aceita `Color` como campo de instância com offset verificado ou como resultado `Color` de getter sem argumentos. Para getters, `runtime_invoke` retorna o objeto boxed e os quatro bytes são lidos somente se o tipo retornado for exatamente o `Color` ligado. A ordem de R/G/B/A é obtida com `NativeColorLayout` e conferida contra a metadata, não presumida do dispositivo.

A leitura monta valores temporários para fundo e borda e publica ambos apenas se as duas operações tiverem sucesso. Uma falha de tipo, offset, getter ou referência não deixa um estado parcialmente aplicado. `tests/tooltip_native/colors_test.cpp` verifica a cor padrão, o getter da loja, a borda, o tipo de objeto, erro gerenciado, offsets, e layout de canais fora de ordem. Os headers de textura, cor e desenho são compilados juntos na unidade `NativeRarityGameplay.cpp` para Android ARM64.

**Limite:** não foi comprovado que `Settings.Instance.Tooltips.Backing` seja sempre uma referência de classe no Terraria mobile; structs por valor precisam de uma implementação separada com ABI validada. Ainda não há publicação de cores, invocação de desenho ou isolamento do estado do painel no jogo. Até a validação Android, o painel opaco permanece no caminho JS original, e nenhum APK é produzido.

## Inspeção conjunta de recursos do painel

A resolução de `Settings.Instance` usa exclusivamente o membro estático original da classe `Settings` em namespace vazio. O `NativeTooltipPanelSettings.h` valida metadados antes de ler o singleton e suporta tanto campo estático quanto getter estático sem parâmetros, sem reter ponteiros de instâncias entre sessões. `NativeTooltipPanelResourcePath` retorna um único snapshot contendo a instância atual, o objeto de backing e as referências tipadas para as duas texturas, publicando tudo somente depois de validar cada etapa. O teste de textura cobre campos estáticos, getter, troca de instância, exceção gerenciada, referência nula e a preservação do snapshot anterior em caso de falha.

`NativeTooltipPanelInspection.h` reúne a validação de `Settings.Instance`, da cadeia de texturas, das cores de fundo/borda e do contrato de `GUIPanel.DrawBacking`. Ele exige que a textura de cada lado seja compatível com o tipo aceito pelo binder de desenho; os valores só são publicados quando a inspeção inteira funciona. Não usa JavaScript para implementar o leitor C++.

`pumpNativeTooltipGameplay` agenda a inspeção na thread do jogo, com intervalo de 600 atualizações apenas depois que o hook de tooltip C# estiver instalado e houver planos C# registrados. Registra `Stage=BackingMetadata Result=Validated Draw=Disabled` quando o contrato puder ser lido, ou um único diagnóstico `Result=Pending` com o motivo quando não puder. Essa verificação não chama `DrawBacking`, não troca o dono do tooltip e não faz varreduras por quadro.

Ainda faltam resolver e provar os casos em que `Tooltips` ou `Backing` sejam estruturas por valor, usar `Color.Multiply` com a opacidade exata do jogo, posicionar o fundo real em conjunto com todas as linhas e validar a aparência no Android. Não interpretar `Validated` como a renderização nativa do painel concluída. Sem APK.
