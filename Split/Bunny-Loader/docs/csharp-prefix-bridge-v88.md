# Bunny Loader C# — ItemPrefix, etapa experimental .88

Esta etapa prepara o contrato C# usando exclusivamente o ExampleModBunny, sem prometer suporte no Android.

## Referencias existentes

- `samples/ExampleMod/content/Content/Prefixes/ExamplePrefix.js`: categoria AnyWeapon, RollChance 5, +20% de dano e +5% de valor.
- `samples/ExampleMod/content/Content/Prefixes/ExampleDerivedPrefix.js`: heranca, Power 2, +40% de dano e +10% de valor.
- `samples/ExampleMod/content/Content/Prefixes/ExampleAccessoryPrefix.js`: categoria Accessory, +4 de defesa ao equipar e +20% de valor.
- `app/src/main/cpp/script/js/mod/ModPrefix.js` e `Loaders/PrefixLoader.js`: registro, rolagem, aplicacao e callbacks JS.
- `app/src/main/cpp/content/items/ModPrefixes.cpp`: IDs de prefixo e lookup por chave de mod.
- `app/src/main/cpp/experiments/csharp/SourceParser.cpp` e `SourceLoader.cpp`: carregador C# atual; BunnyPrefix ainda nao esta registrado.

## Implementado neste experimento

1. Um **rascunho isolado** de tipos C# em `experiments/csharp/BunnyPrefixResearch/PrefixDraft.cs`, com categorias, multiplicadores e callbacks de exemplo.
2. Testes C# de atributos, heranca, valor, bonus de acessorio e peso de rolagem.
3. Testes JavaScript que executam os arquivos originais do ExampleModBunny para verificar os valores de referencia.
4. Teste negativo existente do SDK oficial permanece valido: `BunnyPrefix` nao deve compilar enquanto a ponte de gameplay nao estiver implementada.

**Atenção:** `PrefixDraft` **nao** faz parte do `BunnySourceSdk` distribuido. Este experimento nao registra prefixos reais, nao altera APK e nao habilita ItemPrefix para mods de usuario.

## Integracao necessaria para suporte real

1. Registrar tipos gerenciados C# de prefixo no ciclo do mod, validando categoria, nome e limite nativo do ID (item.prefix e byte).
2. Reaproveitar o `PrefixLoader` existente, sem criar tabela de rolagem independente, nem executar callbacks MonoVM durante acesso concorrente ao estado do Terraria.
3. Encaminhar `CanRoll`, `RollChance`, `SetStats`, `ModifyValue` e `ApplyAccessoryEffects` com parametros tipados e isolados por mod. Acrescentar `Apply` e tooltip apenas quando houver ponte de item segura.
4. Garantir que a chave persistida de prefixo identifique mod/nome e sobreviva a salvar/sair/entrar, reabrir inventario, baus e alternar loadout, sem depender da ordem dos IDs na sessao.
5. Atualizar `BunnySourceSdk`, carregador, exemplos, compilador negativo e catalogo **juntos**, somente apos o registro e os hooks funcionarem.
6. Validar no Android: reforjar arma, reforjar acessorio, stats, tooltip, efeitos ao equipar, save/load, multiplayer e desempenho ate 60 FPS.

## Como rodar o experimento

```sh
node tools/tests/prefix_js_reference.mjs
dotnet run --project experiments/csharp/BunnyPrefixResearch/Tests/PrefixContractTests.csproj -c Release
python3 tools/csharp_api_negative_compile.py --prepare
python3 tools/csharp_api_negative_compile.py --build
python3 tools/csharp_api_negative_compile.py --check
```

Nenhum resultado de compilacao ou simulacao equivale a teste no Terraria Android.
