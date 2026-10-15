# Tooltip e preparo dos mods — relatorio Android 2026-10-15

Base observada: APK .90.29.23 e ExampleMod 0.1.39. A falta de assets foi
resolvida: 14 itens, 2 prefixos, 8 receitas, cache=1/compilados=0. O usuario
confirmou erro de tooltip em ferramentas, ExampleItem, armas e acessorios,
e que encerrou o jogo manualmente. SIGKILL no historico nao prova crash.

A branch codex/sync-jonata-20261015 ja implementou na .90.29.24 o hook
nativo Lang.GetTooltip, retornando o cache de tipos de mod sem passar
pela busca vanilla, conservando o caminho vanilla para seus itens.
Isso ainda precisa de teste no Android nos itens indicados pelo usuario.

A continuacao .90.29.25 preserva esse codigo e melhora o preparo:
- Resumo inicial nao e resetado na saida do mundo. Antes, a nova mensagem
  usava o timestamp do boot, incluindo gameplay em preparo=96276ms.
- Metadata de Main/netMode/gameMenu resolvida uma vez apos ficar completa,
  publicada de forma thread-safe; valores dinamicos continuam sendo lidos.
  Resolucao inicial incompleta nao envenena o cache.
- Uma linha Startup por mod mede espera de tabelas, cache, compilacao e
  requisicao de defaults/recuperacao. Nao e medicao de FPS nem promessa
  de carregamento instantaneo. ApplyAndRecoveryMs inclui recompilacao
  de recuperacao, quando houver; nao somar cegamente os tempos sobrepostos.

O teste auto_source_startup.cpp executa as funcoes de producao com metadata
IL2CPP de host. Verifica 10 mil leituras sem novas resolucoes, valores dinamicos,
getter e fallback de campo, recuperacao apos metadata indisponivel e um unico
resumo depois de pendencias, reentrada e passagem de tempo simulada. Nao
substitui o Terraria nem mede tempo no celular.

Manter ExampleMod 0.1.39 com o mesmo UID. Testar tooltips de ExampleItem,
ExamplePickaxe, ExampleHamaxe, armas, asas e acessorios, depois save/reload.
Comparar abertura fria e quente nas mesmas condicoes, anexando linhas Startup,
inicio rapido e resumo inicial. Confirmar ausencia de nova mensagem de preparo
na saida. Sem remover efeitos, mecanicas, saves ou invalidar caches compativeis.

As alteracoes de preparo sao mantidas na branch do Lote A e na branch integrada,
sem substituir a integracao Jonata nem reintroduzir o pacote 0.1.38 incompleto.
