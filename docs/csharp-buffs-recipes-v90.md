# CSharp .90 — buffs e receitas, checkpoint em andamento

Base preservada: .90.29.89 / ExampleMod 0.1.48, confirmada no Android pelo usuário.
Branch: feat/csharp-buff-recipes-v90. Versões de trabalho: .90.29.90/code261 e
ExampleMod 0.1.49; Lights 0.4.2.3 permanece intacto. O PR #86 foi aplicado com sua
autoria preservada, sobre a .89, não sobre a main antiga.

## Estado da solicitação inteira

**Ainda não concluída.** Este checkpoint implementa infraestrutura de buffs,
grupos personalizados e callbacks/condições de receitas. Não declara minions,
Blocky, montarias, minecart ou o ExampleRecipes oficial integralmente portados.
As lacunas abaixo são trabalho pendente, não limitações comprovadas do Android.
Não anunciar o Lote A como finalizado. A build desta branch, quando aprovada, é
experimental WIP e separada da .89 validada.

## Implementação concreta

- Buffs: IsDebuff, NoSave, NoTimeDisplay, NurseCannotRemove, Description e
  DefenseMultiplierPercent. O probe transporta valores realmente executados em
  C#, inclusive defaults calculados. BUFF mantém linhas antigas de três/quatro
  campos e aceita extensões de seis/sete campos. Descrições usam UTF-8 em hex.
- O layout original de NativeContentSpec foi preservado. Removido o ponteiro
  acrescentado pelo PR para não ler além da estrutura usada pelo NativeAOT.
- Campos/tabelas validam tipo, armazenamento, tamanho, rank, elemento e bounds.
  Flags falsas opcionais não geram erro por campo ausente e não escrevem em um
  campo incompatível. Defaults ficam armazenados e são aplicados/reaplicados na
  thread do jogo. Snapshots imutáveis de efeitos evitam leitura concorrente do
  mapa que recebe defaults do compilador.
- Corrigido Player.UpdateBuffs: i é o índice do jogador, não um slot de buff.
  Percorre todos os slots ativos em ordem reversa. MinionProjectile usa uma
  contagem Int32[] validada e Player.DelBuff real; não encurta o buff para simular
  sua remoção. A IA do minion e seu cajado ainda faltam.
- DefenseMultiplierPercent aplica o fator depois de Player.UpdateEquips. A
  ExampleDefenseDebuff demonstra 75%; ExampleDebuffPotion permite testar esse
  efeito por dez segundos. A poção é uma demonstração extra com textura existente.
- BunnyRecipeGroup: membros vanilla/locais/tipados/entre mods, snapshot de builder,
  identidade por UID/nome, construtor/Register/RequireGroup reais do Terraria,
  reuso sem duplicação, dependências tardias e recusa de definições conflitantes.
- BunnyRecipe: AddRecipeGroup, RemoveRecipeGroup, AddCondition,
  AddConsumeIngredientCallback e AddOnCraftCallback. Clone copia os descritores
  e listas de callbacks; closures mantêm as referências normais do C#.
- BunnyCraftContext: vida/máximo, dia/noite, deserto, neve, água ou pia próxima,
  quickCraft, resultado e quantidade disponível. GiveItem solicita até oito
  concessões nativas validadas, com resolução segura de chaves de mod.
- Disponibilidade usa Recipe.FindRecipes; consumo usa
  GetIngredientCraftingDiscount com RequiredItemEntry e contagem real disponível;
  criação usa CraftItem_GrantItem. Falha/timeout conserva o comportamento original
  ou oculta a receita condicionada. Não retorna sucesso fictício.
- O serviço de receitas recebe um RecipeCallbackProvider independente de linguagem.
  A bridge Mono fica em experiments/csharp/NativeRecipeCallbacks; o núcleo de
  grupos/receitas não precisa de QuickJS nem de MonoVM para seus testes.
- Os callbacks usam a thread Mono já existente, fila limitada, preparo dos handlers
  antes de reconhecer a receita e prazo de resposta de até três milissegundos.
  O filtro compartilha esse orçamento entre as receitas. Mods somente de itens
  também inicializam o worker. Resultados tardios não escrevem no jogo.
- Erros de callback preservam exceção/stack C#, mod, classe, fonte, identidade e
  etapa. Mensagens repetidas têm intervalo mínimo de cinco segundos por receita.
- Cache de compilação passou para v24; identidade de SDK/referências e fontes
  continua invalidando o cache. IDs persistidos e assinaturas públicas existentes
  foram preservados. Nenhuma dependência de tModLoader foi adicionada.

## Testes executados no host

- 26 verificações gerenciadas: SDK, Roslyn real, fontes completas do ExampleMod,
  fixture independente de callbacks/grupos e flags calculadas em C#.
- Serviço/pump de receitas de produção sem QuickJS: grupos nativos e personalizados,
  placeholders, quantidades, reuso, conflito e dependência ausente/tardia.
- Hooks nativos reais com IL2CPP controlado: filtro, consumo e limites,
  concessões, transação de comandos, fallback, rejeição de tipo/offset e ID de pia
  UInt16. Não é execução no Terraria nem comunicação Mono ponta a ponta.
- Parser de wire antigo/novo e descritores malformados; percurso de todos os buffs
  e remoção com deslocamento da lista; metadados de SDK por reflection; pacote e
  política de CI. Sintaxe C++ também verificada com -fno-exceptions/-fno-rtti.

CI Android, nove regressões independentes e assinatura/publicação: pendentes neste
registro inicial; atualizar com os resultados reais. Nenhum teste .90 foi executado
no Xiaomi pelo agente. Primeira compilação <=5 s, FPS, multiplayer e estabilidade
permanecem sem nova medição Android.

## Exemplos e teste Android

Guia editável dentro do bmod:
samples/ExampleModCSharp/Playable/content/Documentation/BUFFS_E_RECEITAS_V90.md.
A ExampleWorkbench conserva três receitas e acrescenta a quarta com alternativas
ExampleItem/ExampleSoul, noite/vida baixa, conservação da madeira e bônus de
ExampleItem no OnCraft. Os testes manuais se referem à bancada C# real.

## Pendências obrigatórias para continuar o mesmo conjunto

1. ExampleDefenseBuff: tooltip dinâmico/localizado do tempo restante.
2. ExampleMinionBuff + minion + cajado: contador no texto, IA original, alvo manual,
   busca/visibilidade, separação, retorno/teleporte ao dono, estado de morte/buff,
   summon slots/sacrifício/contato, animação/luz, posição e direção do disparo.
3. Blocky + ExampleCostumePlayer/Accessory: estado anterior/reset, moradores,
   salto/queda, remoção, visual normal/molhado e regras de vaidade/loadout.
4. ExampleMountBuff/MountItem/Mount: registro compartilhado de MountData, slots,
   textures/frames, parâmetros de movimento, per-player data, balões e poeira.
5. ExampleMinecartBuff/MinecartItem/Mount: Cart/MountType, SetAsMinecart,
   movimento, delegations de sons/faíscas, poeira, armazenamento e reentrada.
6. Receitas: merge/edição de grupos existentes, múltiplas estações, todo o estado
   para condições, consumedItems/destinationStack, fogos originais, shimmer e
   alteração global/PostAddRecipes. As APIs atuais não devem ser anunciadas como
   equivalência integral às do PC.

Continuar a partir desta branch e de docs/csharp-completion-plan.md. Nenhum item
pendente pode ser removido do escopo ou marcado como bloqueio técnico sem prova.
Não publicar a conclusão do conjunto com base apenas em compilação/registro.

## Continuidade posterior

O usuário corrigiu contratos nativos em PR88/89/90; a .91 parte da .90.3.1 e preserva essas correções. A auditoria dos 88 módulos, novas APIs de receitas e a entrega experimental atual estão no [relatório .91](csharp-loaders-v91.md). As pendências desta seção histórica não devem provocar uma reimplementação da bancada ou perda dos commits posteriores.
