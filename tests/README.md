# Validacao C#

Na raiz `Split/Bunny-Loader`:

```sh
dotnet run --project tests/sdk_foundation/SdkFoundation.csproj -c Release
dotnet run --project tests/basic_items/ItemBatch.csproj -c Release
dotnet run --project tools/sdk_reference/SdkReference.csproj -c Release -- --check
python3 tests/run_csharp_foundation.py
python3 tests/run_managed_lifecycle_regressions.py
python3 tests/run_managed_pinned_gc.py
python3 tests/run_managed_signal_ownership.py
python3 tests/test_create_csharp_mod.py
```

O [workflow desta etapa](../.github/workflows/check-csharp-sdk-foundation.yml)
executa essas verificacoes no SHA da branch/PR. O conjunto existente de testes
continua em `Split/Bunny-Loader/tests`; nao foi substituido pelo novo conjunto.

[Resultados, limites e roteiro Android](../docs/SDK-VALIDACAO.md).
Host nativo com metadata/VM simulada verifica contratos, concorrencia e
transicoes; nao comprova o Mono real em ARM64, estabilidade Android ou FPS.
