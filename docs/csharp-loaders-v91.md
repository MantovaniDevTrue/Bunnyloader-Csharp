# Continuidade dos loaders — .90.29.91 / ExampleMod 0.1.50

## Base preservada

Branch `feat/csharp-loader-foundation-v91`, derivada de
`77be9001c2e0328324bb9b60db0fd3a7a64ad23a`, PR90 / .90.3.1.
Não reverte as correções de RecipeGroup Register, retorno real do registrador,
assinatura de CraftItem_GrantItem, fontes de entidade ou fixtures -Werror.
Bunny code266; Lights 0.4.2.3 e IDs dos conteúdos permanecem preservados.

A última build examinada foi [39131332633](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39131332633).
O job Android inteiro passou, inclusive assinatura e entrega privada. O workflow
ficou vermelho na verificação posterior por `Resource not accessible by
integration (addComment)`. A release `csharp-v3-run-39131332633-a1` contém o APK
.90.3.1 e ExampleMod 0.1.49. Não confundir erro de comentário com falha de compilação.

## Implementação desta continuidade

- `BunnyRecipeBehavior`: hooks virtuais IsAvailable, ConsumeIngredient e OnCraft,
  herança indireta, métodos auxiliares e estado de instância. AddBehavior liga
  somente overrides reais; rejeita shadowing/nenhum override. Validação de todas
  as capacidades precede a alteração do builder.
- `BunnyRecipeCondition`: predicados compostos All/Any/Not, condições existentes
  do snapshot e remoção por identidade. Composição copia arrays; Clone preserva
  listas independentes e a semântica normal das referências de delegates.
- `BunnyRecipeStage`: estágio explícito do callback. GiveItem fora de Craft
  agora é recusado com erro real, em vez de gerar comandos silenciosamente ignorados.
- Predicados compostos evitam closures por avaliação; o decoder usa nove inteiros
  na stack e só cria a lista de concessões quando solicitada. Isso reduz alocações
  no caminho frequente, sem remover callbacks ou alterar o orçamento.
- O dispatcher reconstrói apenas o dono solicitado, conserva o assembly por mod
  e deixa falhas de outro dono isoladas. Validação dos nove campos do snapshot
  recusa resultado inválido, disponibilidade negativa e extensões não reconhecidas.
- O ExampleWorkbench mantém suas quatro receitas e passa a demonstrar classe
  de comportamento pública em arquivo próprio, com comentários e guia no bmod.
- A verificação de entrega lê metadados privados e grava o resumo do job. Não
  precisa postar comentário nem solicitar permissão de escrita em issues/PRs.
  Confere privacidade, tag/run, SHA da fonte, nomes/tamanhos dos quatro assets;
  assinatura e bytes continuam sendo verificados pelo build Android.
- A publicação experimental desta branch exige nove suítes de push aprovadas
  no mesmo SHA. Falha, cancelamento, ausência ou resultado de outro commit recusam
  entrega. Nenhum APK é anunciado como conclusão do conjunto.

## Auditoria integral dos módulos

[Inventário dos loaders](generated/csharp-loaders.md) e
[JSON](generated/csharp-loaders.json): 88 módulos originais, incluindo helpers,
com hash por arquivo, indícios de hooks e referências de SDK/bridges existentes.
Todos estão contabilizados. A auditoria ainda não certifica paridade integral de
nenhum módulo. Mapeamentos parciais não significam suporte de todos os hooks.
O gerador valida existência de APIs e fontes; não executa nem importa JS.

Os 206 exemplos JS e 1.221 arquivos oficiais permanecem nos inventários anteriores.
Não alterar estados por coincidência de nome ou sucesso do CI. O próximo grupo
funcional obrigatório inclui o ciclo de buffs e seus textos, minion/cajado/IA,
Blocky/costume, montarias/carrinhos e as lacunas restantes de receitas. Depois,
continuar as dependências de cada módulo; não criar classes vazias para aumentar
cobertura. Nenhuma lacuna foi declarada limitação técnica sem comprovação.

## Testes e limites

Host: 29 verificações gerenciadas com Roslyn real e pacote completo; novos casos
exercitam herança/handlers, disponibilidade e consumo, concessões, clones,
capacidade transacional, erro isolado e operações recusadas por estágio.
Preservados contratos nativos de receitas/buffs/ABI, SDK por reflection,
capacidade de 192 conteúdos, assets e 33 exemplos do escopo A.

A verificação privada possui sete cenários de execução com CLI controlada,
incluindo origem errada, assets ausentes/duplicados, destino público e falha do
build. O audit de loaders só comprova inventário, não comportamento no Terraria.
CI Android/nove suítes: registrar resultado e SHA após execução. Android .91,
multiplayer, cold compile <=5s, desempenho e FPS ainda aguardam medição real.

Guia do aparelho: `Split/Bunny-Loader/samples/ExampleModCSharp/Playable/content/Documentation/RECIPE_LOADER_V91.md`.
Não afirmar que todos os loaders ou o Lote A estão concluídos.
