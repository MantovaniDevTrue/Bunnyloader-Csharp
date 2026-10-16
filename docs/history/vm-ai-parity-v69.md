# Bunny Loader C# .69 — paridade de simulações de IA QuickJS × MonoVM

## O que o teste realmente faz

A partir dos hooks de `ModProjectile.AI` e `ModNPC.AI` presentes nos arquivos-fonte do Bunny Loader, foram construídos **dois kernels determinísticos de lógica de jogo**, comparáveis entre QuickJS e MonoVM sem usar a implementação C++ `projectileBatch`/`npcScan` como se fosse C#:

- **projectile**: simula 64 projéteis com posição e velocidade inteiras, alvo móvel, perseguição, arrasto e limites/rebote; checksum de todas as atualizações.
- **npc**: simula 64 NPCs com alvo móvel, movimento limitado, decisão de ataque e cooldown; checksum de todas as atualizações.
- Ambas as implementações recebem mesmo seed `0x12345678`, mesmo número de ticks, estado inicial idêntico, fazem um aquecimento e exigem igualdade de checksum final.
- QuickJS roda no editor e MonoVM na thread gerenciada persistente, usando DLL exclusiva `BunnyVmGameplay69.dll`; mede tempos de parede e CPU da thread. `rssBefore*` e `rssAfter*` são do **processo inteiro**, não memória isolada da linguagem.
- O benchmark executa **sem alterar NPCs/projéteis reais, mundo, colisões, áudio ou efeitos visuais**. Não mede diretamente FPS de gameplay, marshaling de Terraria ou custo de um callback C# no jogo. O profiler real `sourcePerfStart()` da .65 cobre parte dessa última questão.
- Há um teto de **quatro execuções por inicialização**, compartilhado com `vmCompareStart` da .68. Para AI, 64–5.000 ticks (64 estados por tick). O limite protege o editor contra benchmarks longos.

## Resultado Android .69 — 2.000 ticks de simulação (11/10/2026)

**Ambos aprovados:** `started=true`, `finished=true`, `passed=true`, `phase="matched"`, `error=""`. Cada carga percorreu 64 estados por 2.000 ticks = 128.000 atualizações. Checksums idênticos entre QuickJS e MonoVM.

| Simulação | QuickJS tempo / CPU thread | MonoVM tempo / CPU thread | Razão tempo | Razão CPU |
| --- | ---: | ---: | ---: | ---: |
| Projéteis | 126.129 / 125.043 µs | 4.598 / 4.546 µs | **27,431×** | **27,506×** |
| NPCs | 141.222 / 136.653 µs | 6.566 / 6.489 µs | **21,508×** | **21,059×** |

- Projéteis: `jsResult=monoResult=-639270423`, RSS total (KB) JS 718.456 → 718.464, MonoVM 719.208 → 719.208.
- NPCs: `jsResult=monoResult=-698099503`, RSS total (KB) JS 786.628 → 786.940, MonoVM 786.940 → 786.940.
- Os valores RSS incluem todo o processo Android. Estes são **kernels de lógica simulada**, não entidades ou callbacks reais do Terraria. A vantagem medida não se traduz diretamente em FPS. Outras duas repetições de cada tipo foram obtidas e estão detalhadas abaixo.

## Repetibilidade no Android — três execuções de cada modo (11/10/2026)

Foram enviados mais quatro resultados do mesmo APK .69, totalizando **três execuções para projéteis e três para NPCs**, sempre `ticks=2000`, `entities=64` e `updates=128000`. **Todos os seis testes** passaram com `passed=true`, `phase="matched"`, `error=""`, checksum constante por modo:

- Projéteis: `jsResult=monoResult=-639270423`
- NPCs: `jsResult=monoResult=-698099503`

| Modo e execução | QuickJS parede | MonoVM parede | Razão parede | QuickJS CPU thread | MonoVM CPU thread | Razão CPU |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Projéteis #1 | 126.129 µs | 4.598 µs | 27,43× | 125.043 µs | 4.546 µs | 27,51× |
| Projéteis #2 | 223.459 µs | 6.074 µs | 36,79× | 151.928 µs | 5.245 µs | 28,97× |
| Projéteis #3 | 131.655 µs | 4.811 µs | 27,37× | 127.437 µs | 4.782 µs | 26,65× |
| NPCs #1 | 141.222 µs | 6.566 µs | 21,51× | 136.653 µs | 6.489 µs | 21,06× |
| NPCs #2 | 165.476 µs | 7.737 µs | 21,39× | 159.860 µs | 7.339 µs | 21,78× |
| NPCs #3 | 162.301 µs | 7.388 µs | 21,97× | 159.429 µs | 7.280 µs | 21,90× |

**Resumo de três execuções da mesma carga:** projéteis tiveram médias de tempo decorrido de 160,414 ms (QuickJS) e 5,161 ms (MonoVM), razão de médias **31,08×**; pela CPU da thread, razão das somas **27,75×**. NPCs tiveram médias de 156,333 ms (QuickJS) e 7,230 ms (MonoVM), razão de médias **21,62×**; pela CPU da thread, **21,60×**.

O #2 de projéteis teve `jsWallUs=223459` mas `jsThreadCpuUs=151928`, diferença de **71,531 ms** entre tempo decorrido e tempo de CPU da thread. Isso aponta para tempo não contabilizado como CPU dessa thread (p.ex. espera/escalonamento), mas **não identifica a causa por si só**. Portanto a razão extrema de 36,79× não deve ser interpretada como ganho invariável. Para projéteis, a razão de CPU foi mais concentrada entre 26,65× e 28,97×. Para NPCs, razão de tempo total permaneceu entre 21,39× e 21,97×.

**Escopo:** três repetições são validação inicial de consistência, não um estudo estatístico completo. Essas são simulações computacionais equivalentes em QuickJS e MonoVM; não incluem os efeitos, as colisões, a renderização ou o custo do callback real da API do Terraria. Não é benchmark de FPS. A próxima fase precisa medir o caminho completo dos eventos em gameplay, inclusive latência, fila, CPU e impacto em quadros.

## Teste no Android

Após o workflow publicar a .69, instalar o APK e manter ExampleModCSharp 0.1.7 ativado. Entrar em singleplayer, aguardar o status live `enabled=true`.

No editor, primeiro testar os projéteis:

```javascript
bl.csharp.vmGameplayCompareStart("projectile", 2000)
```

Após aguardar e consultar:

```javascript
JSON.stringify(bl.csharp.vmCompareStatus(), null, 2)
```

Depois, com benchmark anterior concluído, testar NPCs:

```javascript
bl.csharp.vmGameplayCompareStart("npc", 2000)
```

E consultar `vmCompareStatus()` novamente.

Esperado nos dois modos: `phase="matched"`, `passed=true`, `error=""`, `jsResult === monoResult`, tempos `jsWallUs`/`monoWallUs` maiores que zero e `updates=128000` em cada teste (2.000 ticks × 64 estados). A relação de tempo deve ser interpretada **somente** para estes kernels; sem inventar melhoria de FPS geral.

Depois de registrar as amostras no Android, a próxima fase compara latência de callbacks reais, carga de combate e taxas de atualização no gameplay.

**Estado:** build e ambos os modos aprovados no Android, com três execuções válidas por modo (6/6 paridades, nenhum erro). Próxima etapa: medir a ponte de callbacks em gameplay real.
