# Tradução do ExampleMod Bunny JS para C#

Instrução vigente do usuário (2026-10-23): “só traduz o examplemod bunny js pro nosso c# e organiza igual é no tmod [...] fecha todos os loaders antes gerar apk”. Esta instrução substitui a autorização anterior de builds de teste.

- Referência de comportamento: pacote Bunny JS 1.9.0 anexado, inventariado em `reference/bunny-js-1.9.0.json` com hashes dos fontes.
- Organização: `Common/`, `Content/`, `Assets/` e `Localization/`, seguindo o tModLoader. Preservar UID, classes e assets existentes; adicionar os exemplos faltantes ao mesmo mod C#.
- Traduzir os comportamentos originais, valores, receitas e hooks; implementar os loaders/APIs nativos de que dependem. Não substituir uma classe por um exemplo simplificado e apresentá-lo como equivalente.
- Sem JavaScript oculto para implementar recursos C#. A independência do tooltip animado ainda está pendente. O caminho atual de políticas visuais de equipamento é nativo e possui testes de host sem QuickJS; a confirmação visual no Android permanece separada.
- Compilação, syntax checks e testes de host continuam permitidos. **Não gerar APK enquanto houver comportamentos/loaders pendentes ou falhos.**

O Gradle exige `verifyExampleModApkReadiness` em `assembleDebug`, `assembleRelease`, `packageDebug` e `packageRelease`. O workflow e o publicador também verificam a matriz antes de empacotar/publicar. O validador recusa linhas removidas, pendências, falhas e alegações sem evidências. O estado atual bloqueia o APK; não foi iniciado build 315.

A revisão atual corrige a tabela ItemRarityID para C# e as raridades Green de ExamplePickaxe, ExampleHamaxe e WaspNest. Fontes do mod: 0.1.66, sem novo APK. O loader de quest de pescaria também passa a resolver as propriedades estáticas reais do Android (anglerQuestFinished e npcChatCornerItem), com testes dos getters/setters, campos legados e falhas de invocação. O resto da tradução e o fechamento global dos loaders continuam em andamento; a matriz não declara paridade completa. Passar no host permite chegar ao QA Android, mas não comprova gameplay no aparelho.

O lote seguinte corrige `RarityLoader` para o `Main.MouseTextCache` real do Android 301720: estrutura de 48 bytes, passada indiretamente pelo ABI ARM64. O binder valida tipos, tamanho e offsets antes de instalar os hooks; o renderer lê o payload sem cabeçalho de objeto. O tooltip resolve o tipo aninhado por identidade. O fixture executa os escopos e o encaminhamento reais, incluindo restauração da cor, formatos de metadados crus/boxed e rejeições. A confirmação visual e a instalação no aparelho seguem pendentes; isso não encerra o loader de raridade nem a tradução. Nenhum APK foi gerado.
