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

O segundo conjunto inclui cache externo com fontes/runtime/DLL/descritores
verificados por SHA-256 e efeitos nativos de acessorio. Depois de executar
`tests/sdk_foundation` e `tests/basic_items/ItemBatch.csproj`, verifique sem QuickJS:

```sh
cd Split/Bunny-Loader
python3 tests/run_native_equip_effects.py build/basic-items/full-source-runtime-values.txt build/sdk-foundation/native-accessory-wire.txt
python3 tests/run_csharp_foundation.py
```

O primeiro comando usa wire real de Roslyn e mocks IL2CPP/hooks. O WaspNest tem
apenas StrongBees isolado nesse teste; o visual Back continua no teste JS legado.
NativeCharm e ExampleStatAccessory nao precisam de policy JS. A regressao do
cache usa as funcoes de I/O e fingerprint reais de AutoSourceRuntime.
