# Bunny Loader C# .77 — pausa e retomada no mesmo mundo

**Estado:** experimental, aguardando compilação Android e teste no dispositivo. **Baseline validada:** .76, com dois mods C# em singleplayer, reentrada de mundo, pausa seletiva e 60 FPS sem quedas relatadas.

## Objetivo

Pausar os callbacks de um mod C# **adicional**, reativá-los no **mesmo mundo**, e continuar processando o outro mod com a mesma FIFO64 e worker MonoVM. Isso não envolve descarga da assembly gerenciada, retirada de itens do Terraria ou ativação/desativação de mods no gerenciador.

## Mudanças

- `sourceLiveModPause(modId, "CONFIRM_PAUSE_MOD")` permanece, mas agora pausa o tipo no **mesmo mutex da fila de eventos**, descartando os eventos pendentes apenas daquele vínculo. Isso impede que um produtor antigo insira eventos atrás do descarte e os faça reaparecer após uma reativação rápida.
- O novo `sourceLiveModResume(modId, "CONFIRM_RESUME_MOD")` reativa todos os vínculos pausados do mod adicional, **somente se** a sessão primária ainda estiver ativa, não houver limite de eventos atingido, e a identidade original dos vínculos e assemblies estiver preservada.
- A retomada é recusada se o mod já está ativo, se houver um vínculo em estado parcial ou com falhas, ou se a sessão foi encerrada. A reentrada no mundo da .76 permanece o modo seguro de recuperar uma sessão expirada.
- Os contadores de `sourceLiveProjectileStatus()` são mantidos durante pausa/retomada no mesmo mundo. Os eventos **intencionalmente removidos** aumentam `dropped`; isso é diferente de perda por fila cheia ou `lockDrops`.
- Não foram alterados os mods de teste, o movimento das armas, os efeitos ou o limite de 25.000 eventos compartilhados.

## Teste no Android (singleplayer)

1. Instale a APK .77 por cima da .76, sem apagar dados. Ative `ExampleModCSharp 0.1.9` e `SecondExampleModCSharp 0.1.0`.
2. Entre no mundo e use `ExampleWand`, `ExampleWaveWand` e `SecondWand` contra inimigos. Verifique ambos os mods `ready` e FPS.
3. Com os projéteis encerrados e a fila vazia, execute:

```javascript
bl.csharp.sourceLiveModPause(
  "2e508d4f-f192-4a21-83b2-496e6c482c75",
  "CONFIRM_PAUSE_MOD"
)
```

4. Confirme `{"paused":true,"error":""}` e `SecondBolt.enabled=false`. Atire com a arma do **primeiro** mod e confirme que continua processando callbacks.
5. **Sem sair do mundo**, execute:

```javascript
bl.csharp.sourceLiveModResume(
  "2e508d4f-f192-4a21-83b2-496e6c482c75",
  "CONFIRM_RESUME_MOD"
)
```

6. Confirme `{"resumed":true,"error":""}` e `SecondBolt.enabled=true`, depois ataque inimigos com `SecondWand` e observe **novos aumentos** de `processed` e `lastResult` na faixa 3000+ ou 4000+.
7. Repita a pausa e a retomada algumas vezes, sem sair do mundo, e depois faça também um teste de salvar/sair/reentrar.
8. Captura resumida:

```javascript
JSON.stringify({
  auto: bl.csharp.sourceAutoStatus(),
  geral: bl.csharp.sourceLiveHitStatus(),
  projeteis: bl.csharp.sourceLiveProjectileStatus()
}, null, 2)
```

**Critérios de aprovação:** os dois mods `ready`, secundário alternando `enabled` falso/verdadeiro, callbacks do primeiro nunca interrompidos, `processed` do segundo voltando a subir depois da retomada, nenhum erro de MonoVM, nenhum `lockDrops`, jogo fluido e FPS estável.

## Limites

Esta versão **não** oferece hot-unload, limpeza real de memória de DLL, multiplayer ou troca de mods ativos em tempo real. O comando atua **apenas** nos vínculos de callbacks adicionais já registrados. A .76 permanece como rollback até a .77 ser validada no Android.
