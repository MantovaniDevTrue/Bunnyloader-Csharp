# Bunny Loader C# 0.1.10.12.2 — diagnóstico persistente do Mono

## Resultado que motivou esta versão

O mod `Source C# Test 0.1.0` carregou normalmente com 2 arquivos `.cs`, 1 item e 1 projétil, usando o interpretador declarativo `SetDefaults-literals-v1`.

O comando `bl.csharp.managedInspect()` retornou `libraryLoaded: true` e `symbolsReady: true`. A tentativa posterior de `managedProbe()` parece ter terminado o processo do jogo com sinal 6 (SIGABRT), sem registros `BunnyCSharp` no relatório anterior.

A execução gerenciada real **não foi comprovada**.

## Mudanças

- A captura do logcat agora inclui mensagens da tag `BunnyCSharp`.
- Cada transição do `ManagedRuntime` é salva atomicamente em `filesDir/bunny-csharp/last-probe.txt`, com `fsync` antes de continuar.
- O checkpoint registra `timestampUnix`, PID, fase, resultado, flags de execução e erro.
- O relatório do Bunny exibe a seção `--- última tentativa MonoVM ---`, inclusive após um encerramento abrupto do processo.
- Os caminhos JavaScript e NativeAOT não foram alterados.
- O teste de CI simula o fim repentino do processo dentro de `mono_jit_init_version`, sem executar Mono real, e verifica a preservação de `phase=initializing-domain`.

## Passos no aparelho

1. Instale a versão experimental e abra o Terraria normalmente.
2. No console de desenvolvimento, rode `JSON.stringify(bl.csharp.managedInspect(), null, 2)`.
3. Salve e saia do mundo para o menu principal antes do segundo comando.
4. Rode `JSON.stringify(bl.csharp.managedProbe(), null, 2)`. Essa chamada ainda pode abortar o processo.
5. Depois do teste (mesmo se o jogo fechar), abra o diagnóstico do Bunny e copie integralmente `--- última tentativa MonoVM ---` e os trechos `BunnyCSharp mono phase=...`.

Se o último checkpoint for `initializing-domain`, a falha ocorreu dentro de `mono_jit_init_version` ou antes da transição seguinte. Se for `opening-assembly`, examine o carregamento das dependências e do assembly. Se for `invoking-method`, examine execução gerenciada/ABI. O checkpoint localiza uma janela, não prova sozinho a instrução responsável.

## Limites

Esta versão continua sem oferecer interpretação completa ou compilação interna de mods C#. O pacote `SourceCSharpMod` segue no motor declarativo. O runtime de teste usa um pacote Mono ARM64 oficial junto com `System.Private.CoreLib.dll`, mas sua compatibilidade com a inicialização no processo Unity/IL2CPP ainda está em validação.
