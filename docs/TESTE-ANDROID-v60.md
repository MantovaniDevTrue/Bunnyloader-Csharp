# Teste no Android — v0.1.10.16.60

**Status:** aguardando validação real da integração automática de `OnHitNPC` e `OnKill`. A .59 (ativação manual) teve 10/10 eventos confirmados; não transferir esse resultado automaticamente para a .60.

**Pacotes corretos:** [release .60](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/releases/tag/bunny-csharp-managed-runtime-0.1.10.16.60) — `BunnyLoader-CSharp-Test-v0.1.10.16.60-AutoSourceLoader.apk` + `ExampleModCSharp-v0.1.7.bmod`.

1. Faça backup do personagem e mundo de teste. Substitua o ExampleModCSharp 0.1.6 pelo **0.1.7**.
2. Abra um mundo **singleplayer** com o mod ativado. Aguarde a compilação automática e entre no editor.
3. Consulte os três estados, **sem habilitar callbacks manualmente**:

```javascript
JSON.stringify({
  mono: bl.csharp.managedRuntime(),
  live: bl.csharp.sourceLiveHitStatus(),
  auto: bl.csharp.sourceAutoStatus()
}, null, 2)
```

4. O esperado no começo é `mono.domainInitialized = true`, `mono.methodExecuted = true`, `mono.result = 42`; `live.enabled = true`, `live.automatic = true`, `live.phase = "armed"`.
5. Use **ExampleWand / ExampleBolt** para atingir inimigos vivos; aguarde também a destruição de projéteis (evento `OnKill`). Repita a consulta do passo 3.
6. **Critérios para aprovar os callbacks:** `hitProcessed > 0`, `killProcessed > 0`, `failures = 0`; confirme os valores de `queued`, `processed`, `dropped`, `lastEvent`, `lastDamage` e `lastResult`.
7. Teste movimento, múltiplos inimigos, pausa/retomada e salvar/sair do mundo. Registre FPS, especialmente durante os disparos, e qualquer travamento.
8. Em caso de problemas, conserve o relatório do Bunny Loader, os estados do comando, modelo/versão do Android e os passos para reproduzir. O desligamento de emergência é `bl.csharp.sourceLiveHitPause()`.

**Importante:** aumentar `processed` prova que o método gerenciado foi executado ao receber evento. **Não prova** por si só que o retorno do C# modificou o dano ou que APIs arbitrárias sejam suportadas. O exemplo foi desenhado para não alterar o gameplay nessa etapa.

Resultados reais deste teste devem ser incluídos em [STATUS.md](STATUS.md), diferenciando teste Android de simulação/CI.
