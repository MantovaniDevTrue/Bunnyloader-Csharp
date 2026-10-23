# Tradução do ExampleMod Bunny JS para C#

Instrução vigente do usuário (2026-10-23): “só traduz o examplemod bunny js pro nosso c# e organiza igual é no tmod [...] fecha todos os loaders antes gerar apk”. Esta instrução substitui a autorização anterior de builds de teste.

- Referência de comportamento: pacote Bunny JS 1.9.0 anexado, inventariado em `reference/bunny-js-1.9.0.json` com hashes dos fontes.
- Organização: `Common/`, `Content/`, `Assets/` e `Localization/`, seguindo o tModLoader. Preservar UID, classes e assets existentes; adicionar os exemplos faltantes ao mesmo mod C#.
- Traduzir os comportamentos originais, valores, receitas e hooks; implementar os loaders/APIs nativos de que dependem. Não substituir uma classe por um exemplo simplificado e apresentá-lo como equivalente.
- Sem JavaScript oculto para implementar recursos C#. A independência do tooltip animado ainda está pendente. O caminho atual de políticas visuais de equipamento é nativo e possui testes de host sem QuickJS; a confirmação visual no Android permanece separada.
- Compilação, syntax checks e testes de host continuam permitidos. **Não gerar APK enquanto houver comportamentos/loaders pendentes ou falhos.**

O Gradle exige `verifyExampleModApkReadiness` em `assembleDebug`, `assembleRelease`, `packageDebug` e `packageRelease`. O workflow e o publicador também verificam a matriz antes de empacotar/publicar. O validador recusa linhas removidas, pendências, falhas e alegações sem evidências. O estado atual bloqueia o APK; não foi iniciado build 315.

A revisão atual corrige a tabela ItemRarityID para C# e as raridades Green de ExamplePickaxe, ExampleHamaxe e WaspNest. Fontes daquele lote: 0.1.66, sem novo APK. O loader de quest de pescaria também passa a resolver as propriedades estáticas reais do Android (anglerQuestFinished e npcChatCornerItem), com testes dos getters/setters, campos legados e falhas de invocação. O resto da tradução e o fechamento global dos loaders continuam em andamento; a matriz não declara paridade completa. Passar no host permite chegar ao QA Android, mas não comprova gameplay no aparelho.

O lote seguinte corrige `RarityLoader` para o `Main.MouseTextCache` real do Android 301720: estrutura de 48 bytes, passada indiretamente pelo ABI ARM64. O binder valida tipos, tamanho e offsets antes de instalar os hooks; o renderer lê o payload sem cabeçalho de objeto. O tooltip resolve o tipo aninhado por identidade. O fixture executa os escopos e o encaminhamento reais, incluindo restauração da cor, formatos de metadados crus/boxed e rejeições. A confirmação visual e a instalação no aparelho seguem pendentes; isso não encerra o loader de raridade nem a tradução. Nenhum APK foi gerado.

O lote de golfe sobe os fontes para **0.1.67**, preservando os 98 fontes anteriores e acrescentando `ExampleGolfBall` e `ExampleGolfBallProjectile` (100 fontes C# ao todo). Usa os dois PNGs originais, os nomes en-US/pt-BR e os valores do helper Android `Item.DefaultToGolfBall`. A API pública agora oferece `BunnyProjectile.SetStaticDefaults` e políticas tipadas de `ProjectileID.Sets`; o C++ aplica as tabelas antes do primeiro spawn e antes do `SetDefaults` original. A retirada seletiva de `NativeDefaults` restaura células na thread do jogo e preserva outros mods. Os testes de host cobrem recarga e publicação concorrente, mas a física, a renderização e a rede no aparelho continuam pendentes. O código está comentado e o guia está em `content/Documentation/GOLF_BALL_PORT.md`. Sem APK.

O lote de armaduras sobe os fontes para **0.1.68**. Corrige o valor interno das três peças para 50.000 (`SellPrice(gold: 1)`), preserva Green, receitas e textos originais en-US/pt-BR e centraliza as constantes dos efeitos. Verifica também os quatro comportamentos de ExampleSoul no protocolo real e no loader C++. A descoberta antecipada aceita strings interpoladas como expressões opacas; não executa nem aceita essas expressões como identidade literal de conteúdo. A descrição de bônus do conjunto ainda precisa acompanhar o idioma nativo; AddArmorSets permanece pendente por esse motivo. Guia e limitações em `content/Documentation/ARMOR_AND_SOUL_PORT.md`. Nenhum APK gerado.

O lote de localização sobe os fontes para **0.1.69** e implementa a pendência anterior de AddArmorSets no host. `BunnyArmorSet.UseLocalizedText` recebe uma chave relativa, serializada no formato opcional A2; A1 continua compatível. O exemplo lê o JSON original e formata o bônus com a constante de dano. O nativo qualifica a chave com o UID do dono e espera por Language.Exists antes de registrar o LocalizedText. Inspeção do código Android 301720 confirma que Builder.Add chama Language.GetText e armazena o resultado em Description. O fixture usa o protocolo real e cobre en-US, pt-BR, fallback e sete falhas de lookup, preservando efeitos e evitando novos registros na troca de idioma. Essa evidência de host não confirma a aparência nem a execução no aparelho e não encerra o loader inteiro. Nenhum APK gerado.

O lote de retirada de armaduras sobe os fontes para **0.1.70**. A retirada por UID revoga as políticas imediatamente e agenda a limpeza nativa para a thread do jogo, preservando outros mods. SourceLoader conecta esse caminho ao reset, recarga, falha e snapshot completo, incluindo o vazio; uma falha posterior à aplicação dos defaults também revoga as armaduras. O pump remove apenas os conjuntos retirados de All/lookup e libera seus handles depois da limpeza. Mantém a supressão do delegate Pumpkin durante falhas parciais e recupera Add por identidade após deslocamento de índices. Peças com ID reservado mas ausentes do snapshot ativo não reativam conjuntos, inclusive dependências entre mods. Há regressões com o protocolo real, A1/A2, metadados crus/boxed e 100 recargas em outra thread antes do pump; a execução Android continua pendente. Nenhum APK gerado.

O lote dos três loaders fecha os contratos Bunny JS de **LocalizationLoader,
RarityLoader e AnglerQuestLoader** nos fontes e no host. São 69 checks aprovados,
incluindo uma DLL real recompilada no mesmo caminho, revisão compartilhada pelos
três loaders, retirada durável, respostas obsoletas, preservação de outro UID e
contextos coletáveis. A conferência do helper Android também corrigiu o peixe:
26×26, raridade Quest (-11) e `questItem=true` tipado. Nove comportamentos dos
fontes originais receberam evidência automática, com três requisitos de ciclo
completo do contrato Bunny. O escopo exato, referências e limites estão em
`THREE_LOADER_COMPLETION.md`; o relatório é
`HOST_QA_20261023_THREE_LOADERS.json`. QA Android, ferramentas HJSON de desktop e
a conclusão dos outros loaders não são declarados. Nenhum APK gerado.

O lote de partículas da poção traduz `ExampleBuffPotion.SetStaticDefaults` de Bunny JS 1.9.0 para C# com as três cores originais (240/200/140 em RGB) no array nativo `ItemID.Sets.DrinkParticleColors`. O SDK produz um descritor tipado e o C++ valida a tabela `Color[][]`, o tamanho real de `Color` e os canais com metadata IL2CPP antes da publicação. Aloca e aplica `Color[]` na thread do jogo com barreira do coletor, retém os objetos para restaurar a célula no unload sem atingir outro mod e rejeita lote inválido sem publicação parcial. Os testes de host usam o publicador de produção e verificam aplicação, recompilação, rollback, cores, ABI e preservação de modificações externas. Essas provas não substituem teste Android de partículas, inventário ou carga em memória. O tooltip animado nativo independente de JS e os outros comportamentos do ExampleMod continuam pendentes; nenhum APK foi gerado.

O lote do tooltip C# nativo introduz o decodificador tipado de `NativeTooltipPlan` no `BasicItemDefaults` e a retirada seletiva no ciclo de vida do SourceLoader. O `BasicItemWriter` **já serializava** os hooks C# de tooltip; antes, o C++ não consumia a estrutura. A nova camada valida os registros gerados por Roslyn, interpreta Unicode/cor/parâmetros e oferece amostras matemáticas da onda e do arco-íris. O teste C++ usa a saída real de `ExampleTooltipItem` sem JavaScript. **O hook de renderização já está conectado ao SpriteBatch/MouseText e desenha a onda do ExampleTooltipItem no caminho nativo, sem chamar o TooltipLoader.js para produzir essa onda**. A validação visual Android, o suporte a layouts diferentes, a altura/painel do tooltip e a interoperabilidade com tooltips JS continuam pendentes. Escopo: `NATIVE_TOOLTIP_PORT.md`. Sem APK.

O lote de isolamento de tooltip restringe o deslocamento de `DrawString` ao SpriteBatch, SpriteFont e retângulo observado da linha âncora da sessão C#, preservando painéis e outros textos. Reforça testes de sombra, origem, fonte estrangeira e outras camadas. Compilação Android ARM64 e host QA não comprovam ainda layout visual, interoperabilidade com tooltip JS ou escopo PC completo. Sem APK.

O lote de arbitragem Tooltip JS/C# preserva a prioridade dos hooks JavaScript quando necessários e entrega o tooltip puro C++ para exemplos C# compatíveis, com fallback conservador para painel opaco. Expõe estado nativo validado, impede desenho duplicado com uma marca thread-local restaurada pelo wrapper JS, invalida planos antigos na recompilação/unload e confirma contrato de relógio IL2CPP. Host QA usa o arquivo real do TooltipLoader em Node, mais as regressões de fonte C# e C++ nativo; compatibilidade visual e dimensionamento do fundo no Android continuam pendentes. O mod-fonte permanece integralmente C#; só o loader compartilhado conhece a compatibilidade JS. Sem APK.
O lote de defaults clonados acrescenta um protocolo opcional C1: os campos
omitidos herdam a amostra vanilla, enquanto atribuições explícitas de zero,
false e som vazio a substituem. A defesa removida de uma revisão volta a
herdar o clone, ou zero no item simples. A substituição por tipo revoga o
snapshot anterior e rejeita a confirmação tardia de uma instalação; handles
antigos são liberados na thread do jogo. As consultas de elegibilidade de
prefixos passam a considerar os defaults efetivos do item nativo. O wire antigo
continua aceito. O contrato e as limitações de autoria/unload estão em
`content/Documentation/CLONE_DEFAULTS.md`; este lote não conclui o ItemLoader
inteiro, a retirada de tabelas estáticas nem o renderer de tooltip sem JS.

A revisão de viewport implementa fallback JS quando o cursor, escala, controlador ou dados de tela não permitem uma inserção nativa segura. O hook C++ valida e lê `Main.screenHeight` e só insere a linha com headroom suficiente para o texto extra; o teste host cobre bordas e escalas. Isso evita clip e deslocamentos fora da tela, mas não dimensiona nativamente o painel opaco e não equivale à validação visual Android. Sem APK.

O lote de retirada das políticas dinâmicas de item adiciona ownership por UID,
revogação imediata de snapshots e limpeza de handles pelo pump da thread do jogo.
Reset, recarga, falha de carga e falha após publicação dos defaults básicos
conectam essa retirada; o snapshot completo retém apenas os tipos presentes,
inclusive quando está vazio. O fixture usa os fontes reais compilados e cobre
instalação pendente/em execução, 100 recargas, outro UID e callbacks de clone,
cor, luz, asas, robe e reinicialização de animação. Não restaura tabelas estáticas,
texturas/slots, animações já registradas ou todos os defaults de instâncias;
não declara unload global de ItemLoader nem validação Android. Sem APK.

A etapa de geometria do painel acrescenta `NativeTooltipPanelLayout.h` com a política equivalente ao cálculo móvel original de linhas, escala, posição/flip do cursor, limites, padding e banner, e um host-test C++ de cenários e erros. Esse módulo ainda não chama `GUIPanel.DrawBacking`: a ABI nativa e a renderização no Android precisam ser validadas antes de desativar o fallback JS para painel opaco. Nenhum APK foi gerado e a PR continua draft.
O lote de células estáticas de item restaura as três flags de animação e o struct
WingStats completo por tipo. A limpeza compara o último valor publicado,
preserva alterações externas, resolve arrays substituídos, não recria slots
removidos de arrays menores e mantém retry quando a tabela/ABI está indisponível.
Uma nova política espera a limpeza anterior; confirmações antigas não apagam
solicitações novas. A fila expõe waiting-static-retirement e limita o trabalho
nativo a oito tipos com células por pump, em rotação. Testes usam o wire real,
outro UID e lote de 17 tipos. Registro de animação em Main, texturas/slots,
outras tabelas, defaults básicos, unload global e QA Android seguem pendentes.
Sem APK.
