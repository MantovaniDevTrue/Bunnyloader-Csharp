# Continuidade — referencia 1.9.0 e grupo A2

## Base preservada

Branch `codex/lot-a-complete-v90-29`, HEAD examinado
`94c8c1c7161e0d7dfbe1dbc7f02d444021006fa3`.
Bunny `.90.29.17-csharp-armor-tooltip-lookup`, versionCode 176.
Nenhuma alteracao no runtime nativo, offsets, formato de save, IDs, warm-start,
cache ou gates anteriores nesta continuacao. O SDK ja participa do hash Roslyn;
modificacoes no SDK invalidam somente o cache de compilacao correspondente.

## Alteracoes reais

- Inventario reproduzivel do `examplemod.bl` 1.9.0: 206 scripts e 471 arquivos
  em content/, com hashes por arquivo e do pacote inteiro. Sem executar JS.
- Todos os 201 caminhos anteriores preservados. Cinco globais novos:
  ExampleGlobalBossBar, ExampleGlobalBuff, ExampleGlobalRecipe, ShakeTrees,
  SunflowerChanges. Apenas Content/ExampleRecipes.js muda alem de espacos nas
  pontas dos fontes antigos. Fontes novos/alterados preservados no snapshot.
- ExampleWings integrado ao ExampleMod C# 0.1.38, fonte editavel, sprites
  originais, slot Wings, oito parametros de voo e receita na ExampleWorkbench.
  Usa API existente, sem novo atalho exclusivo do exemplo.
- Validacao unica de identidade para ingredientes, estacoes, colocacao e
  partes de conjuntos. Impede segmentos vazios e caminhos com mais de um /,
  informa API, parametro, comprimento, etapa, motivo e rejeicao. Nao altera
  uma receita antes de validar sua chave.
- Testes comparam identidades/categorias declaradas com todos os descritores
  exportados pelo compilador, sem atualizar manualmente uma contagem de tipos.
- O teste de voo roda tambem sobre o wire compilado do ExampleWings real,
  incluindo gravidade invertida e reaplicacao do slot. A fixture anterior
  continua como regressao independente; nao substitui o teste do ExampleMod.
- Matriz corrige linhas obsoletas de bancada e armaduras e explicita a
  dependencia JS restante nos equipamentos. Contratos anteriores preservados
  nao significam que a independencia C# esteja concluida.

## Testes e artefatos

Testes locais executados: inventario (4 casos), regeneracao/check da matriz,
texturas de todos os 14 BunnyItems, geometria nativa de tiles, registrador
nativo de receitas e pump real com BL_HAVE_QUICKJS=0, incluindo intermod,
ordem tardia, isolamento, rollback, tipos e limites.
Compilacao Roslyn, regressao de comportamento e nove gates: consultar CI deste
commit; nao presumir aprovacao. Nao existe aparelho Android conectado aqui.

O workflow de auditoria produz ExampleModCSharp-v0.1.38.bmod somente apos seus
checks. APK fica no proximo checkpoint do grupo A2; o push desta branch deixa
de iniciar Android automaticamente, conforme politica de grupos grandes.
Nao publicar este incremento como conclusao do Lote A.

## Proxima execucao

1. Conferir CI deste commit e corrigir qualquer falha antes de ampliar o grupo.
2. Continuar A2: substituir CSharpItemBehaviorLoader/CSharpArmorPolicy/
   CSharpWingPolicy por servicos nativos independentes, preservando a bridge JS.
3. Completar os comportamentos originais de Boots/Shield/StatBonusAccessory,
   costume e seus estados de jogador; nao trocar por versoes cosmeticas.
4. Conferir todos os 33 registros do Lote A e manter os cinco globais novos
   nos lotes correspondentes. Nenhuma categoria pode desaparecer da matriz.
5. Quando fechar o grupo, compilar um APK experimental com todos os gates,
   publicar no fluxo privado existente e validar os exemplos reais no Android.

## Roteiro Android do grupo (ainda pendente)

No ExampleMod C#, fabricar/colocar a ExampleWorkbench, criar ExampleHelmet,
ExampleBreastplate, ExampleLeggings e ExampleWings com ExampleItem. Conferir
consumo, disponibilidade junto da bancada, recolhimento unico, save/reload,
defesa, mana/minion/imunidade e bonus completo de armadura. Equipar as asas,
voar ate esgotar, testar animacao/subida/queda, gravidade invertida, loadout,
morte/respawn e troca de mundo. Usar mod JS junto para regressao e medir FPS
nas mesmas condicoes da base. Nao usar bancada vanilla como prova da C#.
