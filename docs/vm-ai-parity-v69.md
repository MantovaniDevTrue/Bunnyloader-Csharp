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
- Os valores RSS incluem todo o processo Android. Estes são **kernels de lógica simulada**, não entidades ou callbacks reais do Terraria. A vantagem medida não se traduz diretamente em FPS. Só há uma execução por tipo; repetir para avaliar variação.

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

**Estado:** build e dois modos de simulação aprovados no Android. Repetições e medição de gameplay real permanecem pendentes.
