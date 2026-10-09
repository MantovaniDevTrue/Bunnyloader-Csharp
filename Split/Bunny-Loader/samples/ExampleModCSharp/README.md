# ExampleModCSharp 0.1.0 — Bunny Loader

Exemplos C# baseados nos arquivos que passaram nos testes reais do Bunny Loader Android. O objetivo e ensinar os recursos suportados sem inventar APIs.

Este diretorio agora inclui o prototipo executavel em `Playable/`, com UUID e classes proprias. A compilacao independente chegou a branch experimental v0.1.10.16.35, ainda aguardando validacao no aparelho. O antigo `samples/SourceCSharpMod` continua como fixture estavel de regressao.

Os seis arquivos em `Examples/` sao copias exatas dos fontes verificados. `Playable/` e um pacote diferente, com ExampleWand, ExampleBolt e ExampleBoltMotion. Para testar o pacote independente, utilize somente o `.bmod` gerado na Release e os comandos do novo tutorial.

## Organizacao

- `Playable/`: ExampleModCSharp 0.1.0 independente, em validacao Android
- `Examples/Items/`: SourceWand e SourceStaff, com `BunnyWeapon.SetDefaults()`
- `Examples/Projectiles/`: SourceBolt e SourceSpark, com `BunnyProjectile.SetDefaults()`
- `Examples/Projectiles/AI/`: politica homing e curvas `TurnAtTick(int)`
- `Docs/GettingStarted.md`: rodar o mod C# validado no Android
- `Docs/SupportedAPI.md`: somente as APIs presentes nos fontes
- `Docs/Compatibility.md`: status de cada recurso e limitacoes
- `Tests/ManualAndroid.md`: verificacoes de gameplay e FPS
- `Tests/check_examples.py`: garante sincronismo entre SDK, fontes, manifest e catalogo

## Como usar

Comece por `Docs/GettingStarted.md`. Na branch 0.1.10.16.35, a nova API experimental aceita a compilacao por UUID. Veja o tutorial antes de testar e nao copie APIs de tModLoader.

Cada recurso novo deve passar pelos testes do compilador e depois ser validado no Terraria Android antes de mudar seu estado para `device_verified`.

Referencia estavel confirmada: 0.1.10.16.34. Prototipo independente: 0.1.10.16.35, ainda nao validado no aparelho.
