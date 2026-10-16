# Lote A — desenvolvimento em grupos grandes

Data de inicio: 2026-10-14. Branch: `codex/lot-a-complete-v90-29`.

Base Android preservada: `.90.29.8`, ExampleMod C# 0.1.36. O usuario
validou o funcionamento e mediu `cache=2`, `compilados=0`,
`preparo=693ms`; a entrada completa no mundo marcou 7,1 segundos.
**Nao alterar a otimizacao de carregamento neste lote.**

## Politica de entregas

- Desenvolver varias APIs e varios exemplos do mesmo grupo antes de gerar APK.
- Cada commit pode disparar as oito suites de regressao independentes,
  checks de fonte, compilacao C# de fixtures e validacoes de contratos.
- A compilacao Android completa desta branch foi retirada do trigger
  automatico `push` enquanto o lote estiver em desenvolvimento.
  O workflow ainda suporta `workflow_dispatch`.
- Ao terminar um grupo amplo, reativar temporariamente o trigger `push`
  neste branch no commit de fechamento, ou disparar manualmente o workflow
  a partir desta branch, apos as suites verdes. Depois, suspender novas
  compilacoes Android enquanto o grupo seguinte for desenvolvido.
- **Nao publicar cinco APKs** para pequenos ajustes solucionaveis por testes
  de parser, SDK, Roslyn, native host, ABI mocks e verificacoes de registro.
- Uma build Android intermediaria so se justifica para confirmar uma ABI,
  crash, hook ou comportamento impossivel de validar em host.
- Nao mudar `versionCode`, `versionName` nem cache entre commits internos;
  incrementar apenas no checkpoint Android. Preservar formatos salvos.
- Nenhum sucesso de CI equivale a validacao de gameplay Android. Erros
  obrigatoriamente visiveis; logs normais C# compactos.

## Grupo grande A1 — tiles, estacoes e receitas originais

Agrupar em **uma entrega de teste**:
1. Identidade de classes C# distinta por categoria e namespace, incluindo
   item e tile chamados `ExampleWorkbench` sem colisao nos mapas nativos.
2. API publica original de `BunnyTile`, forma, frame, flags e estacao,
   com contratos de tipos e compilacao Roslyn.
3. Bridge nativa para o registro de tiles, `TileObjectData.Style2x1`,
   arranjos Main/TileID/RoomNeeds, mapa, sustentacao, adjacencia, dust e drops.
4. Port fiel do tile e item `ExampleWorkbench`, textura original,
   colocacao/remocao/persistencia e receita.
5. Receitas originais na bancada para as armaduras ExampleHelmet,
   ExampleBreastplate, ExampleLeggings e equipamento relacionado, preservando
   seus comportamentos e os assets originais.
6. Regressao com duas classes de mesmo nome em categorias diferentes;
   receitas entre C# e JS, falhas isoladas, saves e FPS nao degradado.

**Gate A1:** APIs e exemplos realmente implementados, compilacao de
fontes C#, checks nativos e oito suites verdes. So entao APK experimental.
Se a dependencia exigir mais tempo, manter trabalho e nao anunciar ports
como concluidos antes dos hooks correspondentes.

## Grupo grande A2 — equipamento, atributos e acessorios

Agrupar wings, sets e armaduras, escudo, botas, cosmeticos, tooltip e
prefixos pendentes, incluindo comportamentos com liberdade para codigo
C# personalizado, equip effects sem QuickJS como intermediario e assets.
Nao substituir mecanicas por flags cosmeticas.

## Grupo grande A3 — itens e ferramentas restantes

Agrupar consumiveis, ammo, golfe, drill, hook, fishing, questfish, raridades,
pickup, currency, summon e outros requisitos dos 33 arquivos do escopo.
Se precisar de gameplay callbacks adicionais, fornecer API geral e bridge.
Sem shaders personalizados ainda.

## Conclusao Lote A

Atualizar `tools/example_completion/lot-a-scope.json` e matriz so com
implementacao comprovada; 33 arquivos exigidos, todos os casos nao
implementados e parciais permanecem rastreados ate sua validacao.
Manter compatibilidade JS e isolamento MonoVM/C#. Testes de 60 FPS no
dispositivo sao obrigatorios antes de concluir, sem prometer resultado via CI.
