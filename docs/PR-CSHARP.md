# Mapa de PRs do suporte C#

Consulta de 26/10/2026. Este arquivo serve para facilitar a revisão do Jonata, **não** para declarar que todas as PRs abaixo foram integradas ou aprovadas. O estado das PRs pode mudar depois desta data.

## Código a revisar primeiro

- **Entrega fixa:** [`handoff/jonata-csharp-20261026`](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/tree/handoff/jonata-csharp-20261026). Snapshot de `6d96539f749b2ac8736c08bc38244cf88ec6cda0`, com documentação de leitura.
- **Desenvolvimento/QA:** [`qa/csharp-world-tree-gore-apk-20261024`](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/tree/qa/csharp-world-tree-gore-apk-20261024). Pode avançar sem alterar a entrega fixa.
- **`main` do fork:** conserva outra estrutura de fonte mais antiga. Não é o ponto de entrada para revisar esta integração Android.
- **`main` original:** [JonataOliveiraa/Bunny-Loader](https://github.com/JonataOliveiraa/Bunny-Loader). Não foi modificada por esta organização.

## PRs relevantes que estavam abertas

| PR | Tema | Leitura |
| --- | --- | --- |
| [#149](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/149) | Unificação do C#, Dust/Invasion, ExampleMod e cinco loaders | Histórico da integração |
| [#152](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/152) | Sistemas C# de mundo | PR empilhada |
| [#154](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/154) | Salvamento/persistência de mundo | Depende da #152 |
| [#155](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/155) | Rede de mundo em C# | Depende da #154 |
| [#159](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/159) | Comandos de QA da rede host/cliente | Depende da #155 |
| [#160](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/160) | Random e colisão de projéteis | Depende da #159 |
| [#161](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/161) | Árvores, mudas e gore do ExampleMod | Depende da #160 |
| [#144](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/144) | Contratos gerados de hooks e categorias de conteúdo | Trabalho de infraestrutura |
| [#141](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/141) | Port do ExampleMod e três loaders | Trabalho ainda descrito como WIP |

Essas PRs permaneceram abertas no levantamento, várias como rascunho e algumas com bases empilhadas. O código necessário para um comportamento pode também já aparecer na linha de QA; **não fazer merge automático de cada PR** nem presumir que é preciso copiá-la inteira.

## Correções recentes já incorporadas à branch de QA

- [#172](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/172): proteção de comprimento do array de altura de tiles.
- [#173](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/173) e [#174](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/174): proteção de tabelas e ponteiros do AnglerQuest.
- [#175](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/175): diagnóstico de contratos nativos do traje ExampleCostume.
- [#176](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/176): teste de fidelidade ao efeito GoldFlame/BlueFlare do ExampleModBunny fornecido pelo desenvolvedor.

Elas não comprovam que todos os efeitos estejam certificados no Android. Servem como trilha de correções aplicadas ao snapshot e para evitar retrabalho.

## Experimentos separados

A [PR #138](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/138) contém uma experiência de núcleo de loader inspirada em tModLoader. **Não adotar como base de implementação de gameplay do nosso port.** A referência para os mods Bunny é o código-fonte do Bunny Loader e o ExampleModBunny fornecido; o C# não executa o tModLoader.

## Como continuar organizando

- Preservar as PRs abertas enquanto houver dependências ou mudanças não revisadas.
- Identificar quais commits já estão na linha de QA antes de propor merges.
- Consolidar correções por subsistema; não juntar trabalhos experimentais apenas para diminuir a contagem de PRs.
- Registrar validação separada para MonoVM/Roslyn, contrato do loader e execução real de cada efeito.
- Arquivar documentação histórica por referência, sem apagar testes, fontes, créditos ou commits úteis.

Nenhuma PR foi fechada, mesclada ou removida por esta organização.
