# Plano aprovado de evolucao C#

Pedido do usuario em 15/10/2026: implementar o conjunto de melhorias da auditoria
do GitHub. Este arquivo acompanha todo o escopo, sem transformar uma fundacao
parcial em conclusao do SDK. A fundacao partiu da .90.29.31 (`7229a61`); a base atual integra o PR #24 (.41) e #25; o Lote A e todos
os exemplos da matriz existente permanecem no escopo. A atualizacao
.90.29.32.1 de defesa nativa e a .90.29.33 de efeitos de acessorio foram
integradas nesta branch durante a revisao.

| Area | Entrega concreta | Estado nesta etapa | Criterio para concluir |
| --- | --- | --- | --- |
| Cache Roslyn | Parsing apenas no miss, identidade de referencias/compilador, integridade da DLL | Implementado e exercitado em host | CI e medicao Android de cold/warm start |
| Cache externo | SHA-256 dos fontes/runtime gerenciado, contrato nativo, DLL e descritores | Implementado e exercitado em host | Medir scan unico do framework e cold/warm start no Android; validar recuperacao real |
| Descoberta | Filtrar abstratas/genericos abertos, ancestrais, identidade declarada | Implementado para o contrato documentado | Android; ampliar categorias/sintaxe sem divergencia parser/Roslyn |
| Ferramentas | StarterMod, UID unico, gerador, projeto/autocomplete, pacote so fonte | Implementado em host | Importacao e crafting no Android |
| Referencia SDK | Lista gerada do assembly real e contratos de execucao | Implementado em host | CI rejeitar referencia desatualizada; ampliar descricoes por membro |
| Eventos continuos | Retirar teto acumulado no modo automatico, fila limitada, 32 vinculos e distribuicao | Implementado em host | Teste prolongado no Mono ARM64; memoria/FPS; cotas por mod e orcamento temporal |
| Falhas | Preservar vinculos saudaveis apos falha do principal | Implementado em host | Dois mods no aparelho, falha intencional isolada e retomada |
| Dano sincrono | Prazo por chamada, cancelar pendentes, descartar retornos atrasados, metricas | Implementado em host | Multiplos mods no mesmo quadro, percentis, timeout e ciclo de mundo no Android |
| Ciclo de vida | Contratos publicos de load/world enter/exit/unload e limpeza por mod | Pendente | Hooks realmente despachados e testes de estaticos/saves; nao apenas metodos vazios no SDK |
| Estado por entidade | Snapshots tipados, geracao de spawn, estado de jogador/NPC/projetil | Pendente | Reuso de slot, morte/despawn, troca de mundo e descarte de comandos antigos |
| IA C# geral | Decisoes durante o jogo para NPC/projetil/boss e comandos aplicados na thread correta | Pendente | IA real fora de tabelas; custo, fidelidade e seguranca de GC em ARM64 |
| Independencia de JS | Extrair equipamentos, sets, asas, animacoes/luz e demais policies restantes | Caminho atual de carregamento/gameplay/equipamento migrado para servicos nativos; adaptadores JS fora do bundle | Validar visual/ABI e jogo prolongado no Android; ampliar APIs novas mantendo este contrato |
| API de conteudo | Completar o Lote A e os sistemas reutilizaveis das categorias seguintes | Parcial, manter plano e matriz existentes | Todos os recursos originais aplicaveis, assets, ponte, testes e jogo |
| Exemplos | Armas/melee/ranged/magic/summon; projeteis, pets, minions, sentries; NPC/boss/loot | Parcial | Exemplos completos, editaveis, usando SDK publico sem patch por exemplo |
| Mundo e sistemas | Moveis, paredes, biomas, worldgen, save, UI, config, comandos e localizacao | Parcial | Contratos de cada categoria e roteiro real de persistencia/interface |
| Multiplayer | Autoridade, IDs estaveis, sincronizacao, entrada tardia e save | Pendente | Host/cliente e servidor; NPC/projetil/estado customizado sem divergencia |
| Diagnostico | Logs compactos, erro estruturado comum, PDB opcional e linhas de runtime | Parcial | Compilar/depurar sem custo obrigatorio; erro observavel de cada dispatcher |
| Modularizacao | Separar runtime, scheduler, bindings, diagnostico e benchmarks | Parcial: filas/contadores extraidos | Regressao completa sem mudar transicoes GC/sinais |
| Publicacao | Entrada principal coerente, SDK versionado e releases reproduziveis | Parcial nesta branch | Integrar a branch escolhida, orientar main, build Android e release validada |

## Independencia do caminho C#

NativeItemPolicies e NativeArmorPolicies substituem os adaptadores de
comportamento, equipamentos, asas e conjuntos. Os descritores do ExampleMod
real sao exercitados com BL_HAVE_QUICKJS=0, inclusive os nove itens com
comportamentos e os visuais de escudo/botas. Os arquivos JS antigos ficam apenas
como fixtures de referencia em tests/fixtures/legacy_csharp_policies, fora do
bundle de producao. O loader de mods JS continua disponivel para mods JS.

Veja [contrato, limites e roteiro](csharp-native-policy-independence.md).
Concluir esta migracao nao implementa as categorias ainda ausentes da matriz,
IA C# geral, hot-unload ou multiplayer. A API nova deve continuar independente
sem reintroduzir fallbacks no motor JS.

## Ordem de execucao

1. Validar esta fundacao no CI e no Android; corrigir qualquer regressao da base.
2. Conciliar matriz e evidencias recentes, concluir a validacao Android do Lote A e das policies
   nativas de equipamento. Preservar os assets e a
   fidelidade; nao anunciar efeito isolado como port inteiro.
3. Formalizar ciclo de vida e identidade/estado por entidade. Usar esses contratos
   para IA real, NPCs, bosses e projeteis complexos.
4. Ampliar os exemplos por categorias completas, incluindo sistemas de mundo,
   comandos/UI/config/localizacao. Cada API precisa funcionar em mod independente.
5. Implementar multiplayer com testes dedicados; consolidar SDK/referencia,
   diagnosticos, PDB e publicacao.

Otimizar com medicao e preservar efeitos. Sincronizar direcao com a mira antes
dos disparos nos exemplos correspondentes. Usar os fontes do ExampleModBunny e
do loader; nao introduzir codigo de tModLoader. Nao criar APIs vazias, exemplos
decorativos ou conversores JS como substitutos de uma implementacao C#.

## Entregas

Commits e testes podem ser agrupados por dependencia. APKs ficam para checkpoints
amplos ou verificacoes que realmente exigem Android/ABI, conforme a politica de
[lotes grandes](csharp-lot-a-large-batches.md). A nova fundacao nao incrementa a
versao do ExampleMod nem declara o Lote A completo. A conclusao de cada etapa
exige o criterio da tabela, e nao apenas marcar um checkbox de planejamento.
