# Bunny Loader C# .64 — benchmark real de 25.000 callbacks

A versão .63 foi validada no Xiaomi Android 14 com 1.156 callbacks gerenciados e 0 falhas; houve 12 descartes por contenção, pico de fila 2/64 e 60 FPS relatados. Ela permanece como baseline estável.

## Escopo e proteção

O benchmark é sintético mas **executa métodos C# reais**: um produtor C++ dedicado envia 25 mil eventos alternados de `OnHitNPC` e `OnKill` pelo mesmo FIFO64 da .63, para execução na thread proprietária MonoVM. Não gera NPCs, projéteis, buffs, danos nem outros efeitos nativos. Não é benchmark de combate de chefe.

O comando só é aceito com ExampleModCSharp 0.1.7, auto-loader habilitado em singleplayer, MonoVM pronto, fila vazia e string de confirmação. Uma execução por processo. O dispatcher de callbacks gerenciados de combate normal é desarmado durante o teste; **reiniciar o aplicativo** para retomá-lo após o fim. Políticas de gameplay nativas continuam inalteradas.

A produção usa intervalo mínimo de 400 microssegundos (máximo nominal 2.500 eventos/s, dependendo do agendamento e da capacidade), sem aguardar no frame do Terraria. Se a fila estiver cheia, o produtor espera sem descartar callbacks sintéticos. Duração máxima 120 segundos. Ao completar 25 mil callbacks, aguarda drenagem e fim controlado `limit-reached`. Cancelamento, timeout e falhas preservam o estado de segurança.

## Como testar no Android

Instalar o APK .64, ativar ExampleModCSharp 0.1.7, entrar num mundo singleplayer e aguardar `bl.csharp.sourceLiveHitStatus().enabled === true`.

**Iniciar apenas uma vez por processo:**

```javascript
bl.csharp.sourceLiveBenchmarkStart('CONFIRM_25000_CSHARP_CALLBACKS')
```

**Consultar progresso quantas vezes quiser:**

```javascript
JSON.stringify({
  benchmark: bl.csharp.sourceLiveBenchmarkStatus(),
  live: bl.csharp.sourceLiveHitStatus(),
  mono: bl.csharp.managedRuntime()
}, null, 2)
```

**Cancelar, se necessário:**

```javascript
bl.csharp.sourceLiveBenchmarkStop()
```

Aprovação exige `benchmark.phase="completed"`, `passed=true`, `accepted=25000`, `processed=25000`, `hits=12500`, `kills=12500`, `live.failures=0`, `maxQueueDepth<=64` e nenhuma exceção. O contador `elapsedMs` é a duração no relógio, não o consumo de CPU. Durante editor aberto o Terraria congela, portanto medir FPS numa sessão separada de combate real.

**Pendente:** CI/compilação Android e teste no dispositivo. Um teste unitário de FIFO não prova execução C# em Android.
