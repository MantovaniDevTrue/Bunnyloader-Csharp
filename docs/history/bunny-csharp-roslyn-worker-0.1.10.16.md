# Bunny Loader C# 0.1.10.16 — Roslyn compilando fontes do mod no Android

A 0.1.10.15.2 foi validada no aparelho com 2.400 callbacks Mono no Projectile.AI do SourceBolt e 2.400 modificações de velocidade, 39 contagens de spawn e zero erros. Essa integração anterior executa o Probe.AI embutido no APK; não é a lógica compilada do mod.

A 0.1.10.16 inicia uma etapa **separada e testável**: compilar os arquivos de um mod .bmod contendo apenas fontes `.cs`, sem libmod.so por mod.

## Estrutura

- `BunnySourceSdk` é um assembly net8.0 que usa o código `BunnySourceScriptApi.cs` já presente nos exemplos do Bunny.
- `BunnySourceCompiler` é um assembly net8.0 com Microsoft.CodeAnalysis.CSharp (Roslyn).
- `ManagedProbeService`, no processo Android `:mono_probe`, extrai os assemblies e solicita a compilação através da Mono Embedding API.
- O compilador procura até 32 fontes `.cs` em `content/`, com limite de 256 KiB por arquivo e 2 MiB de fonte total.
- O compilador gera `files/bunny-csharp/compiled/sourcecsharp.dll`, sem distribuir artefato `.so`.
- O cache SHA-256 incorpora código-fonte, nomes relativos, versão do compilador e `BunnySourceSdk.dll`; a segunda compilação não precisa recompilar nada caso os fontes não mudem.
- Erros Roslyn são salvos com arquivo/linha/coluna em `source-compile-diagnostics.txt`. O processo de compilação não participa dos updates nem da renderização do Terraria.

## Teste (primeiro apenas compilação)

Instale o APK 0.1.10.16 experimental e ative Source C# Test 0.1.0. No editor execute:

```javascript
JSON.stringify(bl.csharp.sourceCompile(), null, 2)
```

O pedido deve retornar `queued: true`, `isolated: true`. Isso indica somente que a solicitação foi enviada.

Após alguns segundos consulte:

```javascript
JSON.stringify(bl.csharp.sourceCompileStatus(), null, 2)
```

Esperado: `state=finished`, `phase=compile-verified`, `result=1` (compilou) e `assemblyReady=1`. O texto `diagnostics` deve começar por `COMPILE_OK`. Quando repetir a compilação sem mudar os arquivos, o código esperado é `result=2` e `CACHE_HIT`.

Se falhar, envie a saída de `sourceCompileStatus()` e o relatório do Bunny. Uma falha de compilação não deve parar o processo do Terraria.

## Limite desta primeira versão

O código `.dll` compilado ainda não é usado no hook `Projectile.AI`; o hook estável continua chamando `BunnyManagedProbe.Probe.AI`. Nesta versão a compilação está limitada ao mod de exemplo identificado pelo UID `57a98f20-7a35-4d19-8ce8-079e25bb36a1`, para evitar mudar os outros mods antes de validar o compilador no Android.

Após a compilação no celular estar confirmada, faremos o passo 2: adicionar `AI()` à classe fonte e chamar o método do assembly compilado no `SourceBolt`, com cache por tipo, sem recompilar o APK e mantendo o caminho anterior como fallback.

Mods JavaScript e NativeAOT não são modificados. Não foi importado código tModLoader.
