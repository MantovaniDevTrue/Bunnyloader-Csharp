# ExampleModCSharp 0.1.0 — Bunny Loader

Exemplos C# baseados nos arquivos que passaram nos testes reais do Bunny Loader Android. O objetivo e ensinar os recursos suportados sem inventar APIs.

Este diretorio e a **primeira versao da documentacao e da suite de compatibilidade**. Ainda nao e um .bmod independente. O executavel de teste continua sendo `samples/SourceCSharpMod` (v0.1.5), porque a compilacao e a aplicacao gerenciada nesta fase ainda possuem dependencias do ID e das classes desse mod.

Os seis arquivos em `Examples/` sao copias exatas das fontes verificadas, mantidas por teste automatico. Nao instale estas copias como um segundo mod junto do Source C# Test.

## Organizacao

- `Examples/Items/`: SourceWand e SourceStaff, com `BunnyWeapon.SetDefaults()`
- `Examples/Projectiles/`: SourceBolt e SourceSpark, com `BunnyProjectile.SetDefaults()`
- `Examples/Projectiles/AI/`: politica homing e curvas `TurnAtTick(int)`
- `Docs/GettingStarted.md`: rodar o mod C# validado no Android
- `Docs/SupportedAPI.md`: somente as APIs presentes nos fontes
- `Docs/Compatibility.md`: status de cada recurso e limitacoes
- `Tests/ManualAndroid.md`: verificacoes de gameplay e FPS
- `Tests/check_examples.py`: garante sincronismo entre SDK, fontes, manifest e catalogo

## Como usar

Comece por `Docs/GettingStarted.md`. Para criar seu primeiro mod C# por enquanto, use `SourceCSharpMod` como referencia exata, nao copie API do tModLoader e nao presuma que um ID diferente ja e compilavel.

Cada recurso novo deve passar pelos testes do compilador e depois ser validado no Terraria Android antes de mudar seu estado para `device_verified`.

Referencia estavel: Bunny Loader C# 0.1.10.16.34, com o Source C# Test 0.1.5.
