# Roadmap C# — etapas propostas

Este é um plano de desenvolvimento, **não uma lista de funcionalidades já entregues**.

## Prioridade atual

**.76 validada funcionalmente (11/10/2026):** pausa do segundo mod com `paused=true`, `SecondBolt.enabled=false` e primeiro mod ativo; após saída e reentrada, `SecondBolt.enabled=true`. Ambos os mods `ready` e `cached=true`; 200/200 eventos globais, zero falhas/descartes, sem quedas de FPS relatadas. A .76 agora é baseline para esse fluxo; .75 fica como rollback. Ainda faltam hot-unload efetivo, alteração segura de mods ativos, medição de RSS/frametime e multiplayer.

**.76 — histórico do CI:** APK aprovada, com `worldEpoch`, FIFO64 e worker MonoVM preservados; Android foi validado posteriormente conforme registro acima. A pausa **não** equivale a unload gerenciado real.

**.75 validada funcionalmente no Android (11/10/2026):** dois mods C# ativos, 256/256 eventos processados e SecondBolt 100/100 sem falhas/descartes. **100 inimigos numa Blood Moon e 60 FPS sem quedas relatadas pelo usuário.** A .75 é a baseline para dois mods no cenário comprovado; a .74 permanece rollback. Testes de pausa, hot-unload real, reinício de mundo e multiplayer seguem pendentes.

- **.73** é referência funcional Android para dois projéteis e callbacks separados: 985/985 eventos totais e 879/879 secundários sem falhas/descartes, sem FPS medido nesta rodada.
- **.74** remove NativeAOT apenas do build e startup principais, preserva C++ e biblioteca de mods nativos legados; realizar regressão no celular antes de promover.
- Próximos passos após .75: pausa por mod, ciclo de vida de world unload/reload, benchmark RSS/CPU, multiplayer e métodos C# adicionais baseados no ExampleModBunny.

| Ordem | Etapa | Critério de avanço |
| --- | --- | --- |
| 1 — concluída | Validar **.62 auto live** no Android | **512 callbacks reais**, 0 falhas; jogo continuou funcional após o limite |
| 2 — concluída | Validar .64 com 25 mil callbacks reais em teste opt-in | **25.000 processados, 0 falhas/descartes, 12.336 ms, fila máx. 13/64; jogo seguiu funcionando** |
| 2b — concluída para a carga observada | Medir gameplay com profiler opt-in da .65 | 907 atualizações em 15 s (~60,50 Hz); 30 callbacks perfilados, média 50,2 µs fila e 195,8 µs execução, 0 falhas/descartes, gameplay fluido relatado |
| 2b.1 — próxima | Comparar cenários equivalentes e medir CPU/memória | Registrar FPS visual e ritmo de atualização sem profiler, em combate leve e pesado; avaliar QuickJS vs MonoVM sem inferências por linguagem |
| 2c — próxima | Reduzir gargalos confirmados por medição | Otimizar mutex/ponte com testes regressivos e verificar uso de memória/CPU sem remover efeitos |
| 3 — parcialmente validada | Generalizar registro por mod/projétil com isolamento | **Duas armas e duas curvas de projéteis no mesmo mod confirmadas no Android (.72 + ExampleModCSharp 0.1.8, 60 FPS relatados, zero erros); callbacks live por tipo confirmados na .73; dois mods simultâneos aprovados na .75; faltam descarregamento seguro e multiplayer** |
| 4 — em andamento | Ampliar o ExampleModCSharp por APIs confirmadas | ExampleModCSharp 0.1.9 e classes gerenciadas por tipo validados na .73; guias reorganizados na .74; ampliar somente pelas APIs realmente implementadas |
| 5 | Cobrir mais eventos gerenciados com segurança | Comparação entre nativo/precalculado e callback real; política de execução clara |
| 6 | Estudar sincronização multiplayer | Implementação e testes próprios; não ativar por suposição |
| 7 — concluída para o kernel isolado | Corrigir entrega do benchmark QuickJS × MonoVM (.68) | Android: `matched`, checksum idêntico, 1 milhão de iterações, 445,536 ms QuickJS contra 6,040 ms MonoVM; razão 73,764× válida apenas para o algoritmo testado |
| 7a — parcialmente concluída | Repetir benchmarks e obter distribuição | 500 mil, 1 milhão e 2 milhões de iterações aprovados; três repetições adicionais de 1M com 73,75×/74,27×/75,86× e paridade exata; faltam repetições de 500 mil e 2M e testes de integração em gameplay |
| 7b — próxima | Comparar gameplay e ponte completa | Mesmo evento/cena/carga, FPS/p90/RSS e overhead de integração por método, além de cálculos isolados |
| 8 | Preparar publicação para usuários | API documentada, compatibilidade declarada por recurso, estabilidade demonstrada |

## Princípios

- Respeitar o [Bunny Loader original](https://github.com/JonataOliveiraa/Bunny-Loader) e seu modelo de execução; não apresentar o projeto como oficial.
- No Bunny Loader, usar **ExampleModBunny e os fontes reais do próprio loader como referência**, não código de tModLoader transplantado.
- Preservar fidelidade, recursos e efeitos sem sacrificar desempenho.
- Manter a orientação/mira do jogador sincronizada antes dos disparos quando aplicável.
- Separar rigorosamente resultados simulados, Android confirmado e hipóteses.
- Cada etapa deve ter teste que um desenvolvedor consiga reproduzir.
