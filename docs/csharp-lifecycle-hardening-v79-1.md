# Bunny Loader C# .79.1 — endurecimento do ciclo de vida

**Origem:** integração isolada das correções técnicas do [PR #15](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/15) do Codex sobre a `.79` validada no Android. Branch `csharp-managed-lifecycle-v79-1-experiment`. A `.79` permanece como rollback. **Ainda é necessária validação Android específica da .79.1.**

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
