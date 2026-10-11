# Roadmap C# — etapas propostas

Este é um plano de desenvolvimento, **não uma lista de funcionalidades já entregues**.

| Ordem | Etapa | Critério de avanço |
| --- | --- | --- |
| 1 — concluída | Validar **.62 auto live** no Android | **512 callbacks reais**, 0 falhas; jogo continuou funcional após o limite |
| 2 — concluída | Validar .64 com 25 mil callbacks reais em teste opt-in | **25.000 processados, 0 falhas/descartes, 12.336 ms, fila máx. 13/64; jogo seguiu funcionando** |
| 2b — concluída para a carga observada | Medir gameplay com profiler opt-in da .65 | 907 atualizações em 15 s (~60,50 Hz); 30 callbacks perfilados, média 50,2 µs fila e 195,8 µs execução, 0 falhas/descartes, gameplay fluido relatado |
| 2b.1 — próxima | Comparar cenários equivalentes e medir CPU/memória | Registrar FPS visual e ritmo de atualização sem profiler, em combate leve e pesado; avaliar QuickJS vs MonoVM sem inferências por linguagem |
| 2c — próxima | Reduzir gargalos confirmados por medição | Otimizar mutex/ponte com testes regressivos e verificar uso de memória/CPU sem remover efeitos |
| 3 | Generalizar registro por mod/projétil com isolamento | Identidade correta, vários tipos sem interferência, descarregamento seguro |
| 4 | Ampliar o ExampleModCSharp por APIs confirmadas | Métodos pequenos, compilação automática, exemplos reproduzíveis e testes |
| 5 | Cobrir mais eventos gerenciados com segurança | Comparação entre nativo/precalculado e callback real; política de execução clara |
| 6 | Estudar sincronização multiplayer | Implementação e testes próprios; não ativar por suposição |
| 7 — concluída para o kernel isolado | Corrigir entrega do benchmark QuickJS × MonoVM (.68) | Android: `matched`, checksum idêntico, 1 milhão de iterações, 445,536 ms QuickJS contra 6,040 ms MonoVM; razão 73,764× válida apenas para o algoritmo testado |
| 7a — parcialmente concluída | Repetir benchmarks e obter distribuição | 500 mil, 1 milhão e 2 milhões de iterações aprovados; três repetições adicionais de 1M com 73,75×/74,27×/75,86× e paridade exata; faltam repetições de 500 mil e 2M e testes de integração em gameplay |
| 7b — validada, aguardando repetições | Comparar IA computacional QuickJS × MonoVM (.69) | Android, 64 estados × 2.000 ticks: projéteis 126,129 ms JS / 4,598 ms MonoVM (27,43×); NPCs 141,222 ms / 6,566 ms (21,51×); paridade exata; não representa IA real do Terraria |
| 7c — próxima | Comparar callbacks e gameplay reais | Mesmo evento/cena/carga, FPS/p90/RSS e overhead de integração por método, sem confundir simulação com mundo Terraria |
| 8 | Preparar publicação para usuários | API documentada, compatibilidade declarada por recurso, estabilidade demonstrada |

## Princípios

- Respeitar o [Bunny Loader original](https://github.com/JonataOliveiraa/Bunny-Loader) e seu modelo de execução; não apresentar o projeto como oficial.
- No Bunny Loader, usar **ExampleModBunny e os fontes reais do próprio loader como referência**, não código de tModLoader transplantado.
- Preservar fidelidade, recursos e efeitos sem sacrificar desempenho.
- Manter a orientação/mira do jogador sincronizada antes dos disparos quando aplicável.
- Separar rigorosamente resultados simulados, Android confirmado e hipóteses.
- Cada etapa deve ter teste que um desenvolvedor consiga reproduzir.
