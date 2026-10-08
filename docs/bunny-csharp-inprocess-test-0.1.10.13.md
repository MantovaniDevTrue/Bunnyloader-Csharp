# Bunny Loader C# 0.1.10.13 — teste opcional no processo do Terraria

## Base confirmada

O aparelho ARM64 confirmou na versão 0.1.10.12.4:

- Mono 8.0.23 inicializado dentro do processo `:mono_probe`.
- Framework gerenciado extraído (169 assemblies).
- `BunnyLoader.ManagedProbe.Probe.Run(19, 23)` invocado e retornando `42`.
- `phase=verified`, `domainInitialized=1`, `methodExecuted=1`, `result=42`.

Isto **não** significa que os scripts fonte `.cs` dos mods já sejam compilados e executados; o SourceLoader ainda interpreta apenas declarações literais de `SetDefaults()`.

## Motivação

A IA de NPCs/projéteis e callbacks por quadro precisam de latência baixa. O processo separado é útil para recuperar erros, mas a comunicação IPC em todo quadro e por entidade pode ser cara. A próxima pergunta técnica é se o mesmo Mono 8 que passou no worker isolado consegue iniciar dentro do processo `:game`, com as dependências agora presentes.

## Novo comando

`bl.csharp.managedGameProbe("CONFIRM_IN_PROCESS")` é intencionalmente separado de `managedProbe()`. Ele:

1. Exige confirmação exata.
2. Verifica se a última execução do worker isolado terminou com método executado e resultado 42.
3. Verifica se o assembly gerenciado do worker está disponível na área privada.
4. Faz um único teste de `Probe.Run(19,23)` dentro do processo `:game`, reutilizando a implementação já validada em `:mono_probe`.
5. Registra a última etapa no checkpoint antigo `last-probe.txt`, exibido no relatório após eventual falha.

**AVISO**: a checagem do worker não torna a inicialização dentro do jogo segura. O runtime pode causar SIGABRT no `:game`; faça backup e saia do mundo para o menu antes de testar. Se o usuário não chamar `managedGameProbe`, não há carregamento do Mono por esse novo comando. As APIs JavaScript, NativeAOT e `managedProbe()` anteriores permanecem intactas.

## Sequência para testes

Depois de instalar:

```javascript
JSON.stringify(bl.csharp.managedProbe(), null, 2)
```

Aguarde o worker completar e confirme:

```javascript
JSON.stringify(bl.csharp.managedWorkerStatus(), null, 2)
```

Somente com `phase=verified` no worker e `result=42`, **saia do mundo para o menu principal, salve o progresso**, e caso aceite o risco faça:

```javascript
JSON.stringify(bl.csharp.managedGameProbe("CONFIRM_IN_PROCESS"), null, 2)
```

Se vier `phase=verified` e `result=42` novamente, aí teremos C# gerenciado funcionando no processo do Terraria. Se ocorrer crash, coletar `--- última tentativa MonoVM ---` e `BunnyCSharp mono phase=...` do logcat.

## Próximos passos

Após o teste in-process passar, integrar execução C# ao SDK Bunny/Host API com despacho e cache de métodos por tipo (arma, projétil, NPC etc.). Somente então implementar compilação C# interna e execução de arquivos `.cs` dos mods, preservando a direção do jogador sincronizada com a mira antes do disparo. A compilação não deve rodar durante o gameplay.
