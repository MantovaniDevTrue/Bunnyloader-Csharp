# Versão .63 — fila gerenciada e limite experimental

A .62 foi validada em singleplayer Android: 512 callbacks gerenciados (187 OnHitNPC, 325 OnKill), zero falhas, nove descartes e encerramento controlado ao chegar ao limite.

A .63 mantém os callbacks, os efeitos do jogo e a inicialização MonoVM da .62; altera somente a entrega dos eventos ao C#.

| Recurso | .62 | .63 experimental |
| --- | --- | --- |
| Total de callbacks aceitos | 512 | 25.000 |
| Fila pendente | 1 | 64, FIFO |
| Thread do jogo | Não espera por Mono | Continua sem esperar; `try_to_lock` |
| Ao atingir limite | Desativa | Deixa os já aceitos terminarem antes de desativar |
| Sobrecarga | Descarta quando ocupado | Descarta apenas se fila cheia ou contenção do mutex |
| Falhas gerenciadas | Desativa após 3 | Mantém e descarta os pendentes ao desativar |
| Isolamento | Um tipo de projétil, singleplayer | Inalterado |

Os contadores em `bl.csharp.sourceLiveHitStatus()` agora incluem `queueDepth`, `maxQueueDepth`, `queueCapacity`, `eventLimit`, `inFlight` e `lockDrops`. `queued` é o acumulado aceito, não o número atualmente aguardando; `dropped` contabiliza fila cheia, callbacks pendentes abortados e contenção observada.

Quando o evento 25.000 é aceito, o dispatcher bloqueia novas entradas e muda para `draining`. A thread MonoVM executa o que já estava na fila e só então termina com `limit-reached`. O código que invoca o método C# também aceita `tick` até 25.000. Nenhuma chamada Mono ocorre dentro do frame do jogo.

## Validação

Os testes automatizados `tests/run_managed_live_queue.py` comprovam em C++ FIFO, a capacidade 64, wrap-around, limpeza e 25 mil operações; o workflow revisa guards do dispatcher. **Não simulam MonoVM real nem FPS.**

No Android, testar com `ExampleModCSharp 0.1.7`:

```javascript
JSON.stringify({
  mono: bl.csharp.managedRuntime(),
  live: bl.csharp.sourceLiveHitStatus(),
  auto: bl.csharp.sourceAutoStatus()
}, null, 2)
```

Confirmar `eventLimit=25000`, `queueCapacity=64`, `automatic=true`, `hitProcessed>0` e `killProcessed>0`, `failures=0`, `queueDepth<=64`, FPS e memória no celular. Repetir em combate intenso, depois ultrapassar 512 para comprovar que a limitação antiga caiu. Se possível, avançar até 25 mil e confirmar drenagem completa sem travar o jogo. `dropped` não deve aumentar sem causa, mas pode crescer durante excesso real de produção.

**Estado:** alteração experimental aguardando build e teste Android; não considerar 25 mil eventos validados apenas pelo CI. 
