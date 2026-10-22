# Testes C#

Na raiz do repositório, sem toolchain Android:

```sh
python3 tools/check_repository.py
```

As suítes canônicas ficam em `Split/Bunny-Loader/tests`. Para os comandos
seguintes, entre **uma vez** em `Split/Bunny-Loader`. Requer .NET SDK 8,
Python 3 e g++/C++20; suites legadas específicas também usam Node.js.

```sh
cd Split/Bunny-Loader
dotnet run --project tests/sdk_foundation/SdkFoundation.csproj -c Release
dotnet run --project tests/basic_items/ItemBatch.csproj -c Release
dotnet run --project tests/entity_lifecycle/EntityLifecycle.csproj -c Release
dotnet run --project tests/entity_source/EntitySource.csproj -c Release
dotnet run --project tests/hooks/HookBatch.csproj -c Release
dotnet run --project tools/sdk_reference/SdkReference.csproj -c Release -- --check
python3 tests/run_csharp_foundation.py
python3 tests/run_managed_lifecycle_regressions.py
python3 tests/run_managed_pinned_gc.py
python3 tests/run_managed_signal_ownership.py
python3 tests/test_create_csharp_mod.py
python3 tests/test_csharp_independent_mod_loading.py
python3 tests/test_csharp_no_javascript_dependency.py
python3 samples/ExampleModCSharp/Tests/check_examples.py
```

## Policies nativas sem QuickJS

SDKFoundation e ItemBatch geram os arquivos wire consumidos abaixo. Ainda a
partir de `Split/Bunny-Loader`:

```sh
python3 tests/run_native_equip_effects.py build/basic-items/full-source-runtime-values.txt build/sdk-foundation/native-accessory-wire.txt
python3 tests/run_native_armor_policies.py build/basic-items/armor-source-runtime-values.txt
python3 tests/run_native_item_policies.py build/basic-items/full-source-runtime-values.txt samples/ExampleModCSharp/Playable
```

As suítes usam os módulos de produção com doubles IL2CPP/hooks e QuickJS
desligado. Equipamento, textura Back, armaduras/conjuntos, luz, Color,
animação e renderização C# implementados têm caminho nativo. Fixtures JS em
`tests/fixtures/legacy_csharp_policies` preservam regressões do adaptador antigo,
fora do bundle de produção; sua presença não cria dependência JS no mod C#.

| Suíte | O que verifica |
| --- | --- |
| SDKFoundation/ItemBatch | Roslyn real, cache, herança, identidade, defaults/receitas e wire |
| EntityLifecycle/EntitySource | Registro, estado por entidade, IA, combate, slot/geração e falhas |
| HookBatch | Ref/out, unhook e isolamento de callbacks |
| run_csharp_foundation | Parser/cache/fila contínua e scheduler de dano com prazo |
| run_native_* | Equipamento/armadura/item, metadata, retry e limites sem JS |
| run_managed_* | Ciclo de vida, GC, sinais e resultados atrasados |
| check_examples e auditoria | Fontes, assets e evidências de exemplos; não comprovam todos os ports no Android |
| check_repository | Links locais dos guias mantidos, atalhos, projetos, versões e integridade dos pacotes do checkpoint |

[Mapa do CI](../docs/BUILD.md#workflows). A referência gerada deve corresponder
ao SDK: para regenerar, rode SdkReference sem `--check` e revise o diff.
Não edite arquivos gerados manualmente.

Host não comprova ABI/renderização real, estabilidade ou 60 FPS.
[Protocolo Android](../docs/SDK-VALIDACAO.md) · [Evidências atuais](../docs/STATUS.md).

## Núcleo de loaders tModLoader (sem APK)

```sh
python3 Split/Bunny-Loader/tests/content_loader/check_upstream.py
python3 Split/Bunny-Loader/tests/content_loader/run_native.py
dotnet run --project Split/Bunny-Loader/tests/content_loader -c Release
dotnet run --project Split/Bunny-Loader/samples/ContentLoaderCSharp -c Release
```

Inclui fontes oficiais pinadas em um host de teste para comparar algoritmos,
fault injection de carga/unload, verificação de assemblies de serviços/mistos e
execução do exemplo. O runner nativo exercita a fila de produção (timeout, lotação,
rollback e shutdown), SourceLoader sem APIs de jogo e ambos os gates de entrada
no mundo. Usa doubles Mono/IL2CPP; não comprova o runtime no aparelho.
[Contrato e limites de integração](../docs/csharp-content-loader.md).

## BiomeConversionLoader

```sh
python3 Split/Bunny-Loader/tests/biome_conversion/run.py
```

Confere fontes tModLoader pinadas, executa paridade, Roslyn e P/Invoke real com
o engine C++ de produção. Testa o adaptador IL2CPP com setters copy-on-write e
assinaturas simuladas, limites, mundo/menu/multiplayer, unload e falhas isoladas.
[Contrato do loader](../docs/csharp-biome-conversion-loader.md). Sem APK.
