# Bunny Loader C# 0.1.10.16.2 — correção experimental SHA-256

A versão 0.1.10.16.1 inicia o compilador Roslyn no worker Android isolado, mas o usuário relatou falhas SIGABRT no processo `:mono_probe`. O método gerenciado `Preflight.Ping` foi executado com sucesso, e `sourceCompileStatus()` mostrou:

```text
ROSLYN_STAGE=sources-found-2
phase=compiling-source
```

O próximo ponto no código anterior era a criação de `IncrementalHash.CreateHash(HashAlgorithmName.SHA256)`, que pode depender da inicialização da biblioteca criptográfica nativa. Isso é uma **hipótese**, não um diagnóstico confirmado por backtrace.

## Alterações

- Substituído `IncrementalHash` por SHA-256 puro C# (`PortableSha256.cs`).
- Nenhuma dependência de biblioteca de criptografia nativa é necessária para gerar os hashes de cache das fontes.
- Nova versão de chave do cache (`roslyn-net8-source-v2-portable-sha256`) para invalidar uma entrada antiga se houver.
- Mantidos todos os checkpoints do Roslyn, com etapas extras `initializing-portable-sha256` e `portable-sha256-ready`.
- O teste automatizado valida SHA-256 contra vetores conhecidos: string vazia, `abc`, um milhão de caracteres `a`, com alimentação em blocos variáveis e `GetHashAndReset()`.
- SDK, carregador JavaScript/NativeAOT, lógica do SourceBolt e caminho de IA que funcionou na 0.1.10.15.2 não foram alterados.

## Como testar no Android

Com `Source C# Test 0.1.0` ativado, execute somente uma vez:

```js
JSON.stringify(bl.csharp.sourceCompile(), null, 2)
```

Aguarde alguns segundos e execute:

```js
JSON.stringify(bl.csharp.sourceCompileStatus(), null, 2)
```

Se a falha for realmente na dependência criptográfica nativa, esperamos ver o worker avançar até `portable-sha256-ready`, `parsing-source-0` e além. A confirmação completa requer `state=finished`, `phase=compile-verified`, `assemblyReady=1` e `COMPILE_OK`.

Se o worker abortar novamente, envie o resultado de `sourceCompileStatus()` e o relatório completo, inclusive os checkpoints. Não execute `sourceCompile()` repetidamente enquanto um worker estiver em execução.

O teste continua restrito à compilação isolada: não carrega o assembly no gameplay e não converte mods tModLoader.
