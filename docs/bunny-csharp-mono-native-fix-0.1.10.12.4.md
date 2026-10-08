# Bunny Loader C# 0.1.10.12.4 — inicialização do Mono ARM64

O diagnóstico da versão 0.1.10.12.3 confirmou `phase=initializing-domain` e um encerramento nativo apenas no processo `:mono_probe`. O logcat do worker registrou:

```
Encountered infinite recursion while looking up resource 'Arg_DllNotFoundException'
in System.Private.CoreLib.
```

O pacote anterior trazia apenas `libmonosgen-2.0.so`, `System.Private.CoreLib.dll` e `BunnyManagedProbe.dll`. Uma exceção de biblioteca nativa ausente durante o bootstrap do framework é compatível com a mensagem observada, mas não identifica qual biblioteca foi solicitada.

## Mudanças

- Distribuir todos os arquivos `.so` correspondentes a Android ARM64 do pacote oficial `Microsoft.NETCore.App.Runtime.Mono.android-arm64` 8.0.23, excluindo `libc++_shared.so` para preservar a cópia preexistente do Terraria.
- Distribuir os assemblies do diretório `runtimes/android-arm64/lib/net8.0` do mesmo pacote, junto com a versão correspondente de `System.Private.CoreLib.dll`.
- Validar `libSystem.Native.so` antes de criar o domínio; em caso de falha, mostrar `phase=system-native-missing` e `error` com a mensagem de `dlerror`, sem tentar inicializar o Mono.
- Invocar a API padrão `mono_jit_init` para o runtime Mono atual, em vez da seleção explícita da versão clássica `v4.0.30319`.
- Extrair os assemblies somente durante o teste pedido ao worker isolado; nunca durante a abertura do Terraria. O worker armazena a versão do framework e reutiliza os arquivos nos próximos testes.
- Continuar a registrar checkpoints, erros e o logcat do processo `:mono_probe`. Manter JS, NativeAOT e o interpretador declarativo intactos.

## Teste

No Editor do Bunny:

```javascript
JSON.stringify(bl.csharp.managedProbe(), null, 2)
```

Aguardar alguns segundos e consultar:

```javascript
JSON.stringify(bl.csharp.managedWorkerStatus(), null, 2)
```

O resultado desejado de execução gerenciada real é `state=finished`, `phase=verified`, `domainInitialized=1`, `methodExecuted=1`, `result=42`.

Se o worker falhar, copiar o relatório inteiro, sobretudo `--- worker MonoVM isolado ---`; o jogo deve seguir independente do crash do runtime.

## Escopo

O teste automatizado com uma biblioteca simulada verifica as chamadas, mas não prova a inicialização Android em dispositivo. O reconhecimento de mods `.cs` continua pelo interpretador limitado `SetDefaults-literals-v1`. A compilação e execução geral de mods C# ainda não estão implementadas.
