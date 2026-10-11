# Bunny Loader C# .79.1 — endurecimento do ciclo de vida

**Origem:** integração isolada das correções técnicas do [PR #15](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/15) do Codex sobre a `.79` validada no Android. Branch `csharp-managed-lifecycle-v79-1-experiment`. A `.79` permanece como rollback. **CI e APK assinada aprovados:** [workflow #38166213994](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/38166213994) terminou com **success**. [Release .79.1](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/releases/tag/bunny-csharp-managed-runtime-0.1.10.16.79.1-lifecycle-hardening), versionCode 105, APK 223296221 bytes. **Validada em Android com 3 armas, 2 mods C#, Blood Moon (~100 inimigos), pausa/retomada, reentrada em mundos e 60 FPS relatados (resultados abaixo).**


## Validação Android .79.1 com três armas — 11/10/2026

**Resultado:** usuário testou as **três armas / três tipos de projéteis** dos **dois mods C#** durante Blood Moon com aproximadamente **100 inimigos**, observando **60 FPS**. Fez análise com ambos ativos, pausou o segundo mod, disparou durante a pausa, analisou o estado, reativou o mod, disparou novamente e conferiu o estado; também salvou, saiu e retornou ao mundo. Dois mods em `ready` e callbacks em funcionamento após a reentrada. **A avaliação inicial de que faltava testar as outras armas estava errada; todos os três foram usados.**

| Evidência dos prints | Antes / durante / após |
| --- | --- |
| Fila principal (`projectileType=1137`) | `queued=processed`: 340/340 → 563/563 → 885/885 |
| `hitProcessed / killProcessed` fila principal | 187/153 → 322/241 → 504/381 |
| Fila, `lockDrops`, falhas | `queueDepth=0`, `dropped=0`, `lockDrops=0`, `failures=0`; pico de fila 2–3 de capacidade 64 |
| `ExampleWaveBolt` (tipo 1136, primeiro mod) | 137/137 → 286/286 → 455/455 eventos; no final 295 hits + 160 kills; zero perdas/falhas |
| `SecondBolt` (tipo 1138, segundo mod) | 141/141 antes da pausa, `enabled=false` enquanto pausado, **205/205 após reativação** (97 hits, 108 kills), `enabled=true`; sem falhas ou descartes |
| Projéteis ativos | 3 tipos ativos → **2 durante a pausa** → **3 após retomada** |
| Cache de métodos | hits 1702 → 1925 → 2247, misses **6** constantes |
| Cache de assemblies | caminhos **2**, `cachedCallbackEntries=3`, `liveAssemblyOpenAttempts=6`, `liveAssemblyOpenSuccesses=6`, estáveis nos prints |
| Memória gerenciada MonoVM | `monoGcStatsAvailable=true`, heap 21495808 bytes, usados 4199752 bytes, constantes nas capturas |
| Ciclo de mundo final | `worldLoads=3`, `worldExits=2`, `epoch=5`, `rebindSchedules=3`, `configuredBindings=2`, `worldReady=true` |

Os snapshots de RSS variam consideravelmente (e.g., 324224, 331136 e 556140 KiB) acompanhados de swap variável, enquanto os números da heap gerenciada se mantêm estáveis; RSS/swap **não** indicam por si um vazamento, mas as amostras também não comprovam ausência de leaks de longo prazo. `mods[].phase=ready` não se confunde com a habilitação do callback: durante a pausa, `SecondBolt.enabled=false` foi intencional, e a contagem de tipos ativos caiu para 2. Após retomar, `SecondBolt.enabled=true` e os eventos aumentaram de 141 para 205. A sequência testa o funcionamento do ciclo de vida e da entrega em uso normal, porém **não injeta falhas C# intencionais nem produz necessariamente a condição de corrida rara**; a suíte de regressão do Codex cobre essas situações em ambiente controlado.

**Decisão:** aprovar a **.79.1 como nova referência funcional validada em Android para os cenários testados**, preservando a .79 como rollback. Nenhum motivo para repetir teste com três armas, Blood Moon ou pausa/retomada apenas por causa da análise anterior.

## Mudanças no código

- `ManagedLiveIngressQueue` captura token de sessão e geração por vínculo **antes** de procurar o tipo do projétil. A admissão é validada sob o mutex da FIFO; pause invalida somente o vínculo afetado, enquanto clear/reset invalida toda a sessão. Um produtor suspenso antes da pausa não deve reenfileirar eventos antigos depois do resume.
- Falhas do mod secundário mantêm contadores próprios. O callback principal mantém o seu limite de 3 exceções, isolado da contagem agregada dos vínculos adicionais. **Não** é isolamento completo de falhas de todos os mods.
- Mantidos MonoVM e worker, FIFO64, teto agregado de 25.000 eventos, os dois ExampleMods C#, os controles de pausa/retomada, reentrada no mundo, heap MonoVM e caches da .79.
- Testes host incluídos no GitHub Actions reproduzem produtor suspenso, troca de mundo, pausa seletiva, capacidade da fila e exceções simuladas no dispatcher. Comentários extras foram retirados dos arquivos C++ integrados.
- **Não** implementa hot-unload de DLL, garbage collection forçado, multiplayer completo, netcode ou API tModLoader.

## Roteiro de teste Android

1. Instalar a APK experimental `.79.1` **por cima** da `.79`, sem limpar dados. Os ExampleMods C# existentes permanecem nas versões 0.1.9 e 0.1.0.
2. Entrar num mundo de teste com os dois mods ativados. Atirar contra inimigos com as duas armas, conferir FPS, dano e efeitos.
3. Enquanto permanece no mundo, pausar e reativar o segundo mod várias vezes:

```javascript
bl.csharp.sourceLiveModPause(
  "2e508d4f-f192-4a21-83b2-496e6c482c75",
  "CONFIRM_PAUSE_MOD"
)
```

```javascript
bl.csharp.sourceLiveModResume(
  "2e508d4f-f192-4a21-83b2-496e6c482c75",
  "CONFIRM_RESUME_MOD"
)
```

4. Salvar e sair; entrar novamente no mundo e atacar. Confirmar ambos os mods em `ready`, `enabled=true`, zero falhas involuntárias e 60 FPS.
5. Fazer pelo menos uma Blood Moon com muitos inimigos e registrar:

```javascript
JSON.stringify({
  memoria: bl.csharp.sourceMemoryStatus(),
  mods: bl.csharp.sourceAutoStatus(),
  eventos: bl.csharp.sourceLiveHitStatus(),
  projeteis: bl.csharp.sourceLiveProjectileStatus()
}, null, 2)
```

**Critérios:** `queued=processed` após drenar a fila; `lockDrops=0`, `dropped=0` em testes sem saturação e sem descartes intencionais; `failures=0`; ausência de efeitos faltantes e FPS igual à .79. Atenção: `dropped` pode ser maior que zero **intencionalmente** se houver eventos pendentes cancelados durante a pausa, e isso não é necessariamente bug.

**Limite do teste:** eventos que já estavam sendo executados antes da pausa não são cancelados à força. O Android não injeta exceções gerenciadas só para reproduzir o teste de isolamento; essa parte já é coberta pela suíte mock no CI. As mudanças de proteção acrescentam leituras atômicas leves no ingresso de cada evento, cuja repercussão real em FPS precisa ser medida no aparelho.
