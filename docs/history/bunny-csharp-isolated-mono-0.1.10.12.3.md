# Bunny C# v0.1.10.12.3 — worker MonoVM isolado

## Diagnóstico anterior

A versão 0.1.10.12.2 detectou `phase=initializing-domain` no mesmo processo Unity/IL2CPP do Terraria; a tentativa terminou com sinal 6 (SIGABRT). A biblioteca Mono 8 ARM64 carregou, os símbolos estavam disponíveis e o `System.Private.CoreLib.dll` estava no pacote, mas `mono_jit_init_version` não retornou domínio.

A versão 0.1.10.12.3 não afirma que o runtime já executa C#: separa essa tentativa em um **segundo processo Android**, para isolar possíveis abortos do runtime e capturar informação real sem interromper o jogo.

## Estrutura

- O Terraria permanece na `GameActivity`, processo `:game`.
- `ManagedProbeService` usa o processo `:mono_probe`, iniciado apenas sob comando explícito.
- `libbunnymonoprobe.so` contém `ManagedRuntime.cpp` e JNI da própria service, sem carregar o script engine, Unity ou hooks IL2CPP.
- `libbunny.so` envia um `Intent` Android para o processo isolado pela ponte `MonoServiceDispatch.cpp`.
- `bl.csharp.managedProbe()` **não inicializa mais Mono no processo do Terraria**; solicita o trabalho à service e retorna `queued`. O resultado aparece depois em `bl.csharp.managedWorkerStatus()` ou no relatório de diagnóstico.
- O worker salva o checkpoint em `filesDir/bunny-csharp/last-isolated-probe.txt`, separadamente do checkpoint legado `last-probe.txt`.
- O relatório inclui estado e logcat do worker.
- A inicialização da service não acontece durante o boot do jogo.

## Teste

Abra o console do Bunny e execute:

```javascript
JSON.stringify(bl.csharp.managedProbe(), null, 2)
```

Resultado esperado da solicitação: `queued: true` e `isolated: true`. Isso indica **apenas que o Android aceitou o pedido**.

Após alguns segundos:

```javascript
JSON.stringify(bl.csharp.managedWorkerStatus(), null, 2)
```

Se o Mono abortar, o jogo deve continuar aberto. A saída exibirá `state=calling-mono` com o checkpoint `phase=initializing-domain` ou outra fase. Se a chamada gerenciada finalmente funcionar, o estado será `state=finished`, `methodExecuted=1` e `result=42`.

Se o worker falhar antes de abrir o Mono, o campo `state=failed` conterá uma exceção do Kotlin/JNI. Também verificar `--- worker MonoVM isolado ---` no relatório do Bunny.

## Próxima correção

Caso o worker abortar em `initializing-domain`, corrigir o bootstrap para o host Mono/.NET 8 para Android, incluindo as dependências e configuração de runtime apropriadas. A presença de `libmonosgen-2.0.so` e `System.Private.CoreLib.dll` por si só não comprova que o host .NET 8 está inicializado corretamente.

O interpretador declarativo `SetDefaults-literals-v1` para mods fonte `.cs` segue inalterado e não executa `AI()`. JavaScript e NativeAOT continuam preservados.

## Risco e performance

O isolamento é por processo Android, não isolamento de segurança de mod. O worker usa memória adicional somente enquanto estiver ativo. O Bunny não executa compilação nem Mono no loop de renderização. A service `:mono_probe` é `exported=false`.
