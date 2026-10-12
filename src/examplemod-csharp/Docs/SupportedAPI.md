# API C# realmente disponivel

Referencia: `experiments/csharp/BunnyCsSdk/BunnySourceScriptApi.cs`, nao classes do tModLoader.

## BunnyWeapon

Metodos: `public override void SetDefaults()`.

Propriedades: `DisplayName`, `Texture`, `Damage`, `UseTime`, `ShootSpeed` e `Shoot`.

Exemplos: `Examples/Items/SourceWand.cs` e `SourceStaff.cs`. O valor de `Shoot` identifica o nome da classe de projetil registrada no mesmo mod.

## BunnyProjectile

Metodos: `public override void SetDefaults()`, `public override int OnHitNPC(int damage)`, `public override int OnKill(int damage)` e `public override int OnKillDuration(int damage)`.

Propriedades: `DisplayName`, `Texture`, `Width`, `Height`, `AiStyle`, `Friendly`, `Penetrate` e `TimeLeft`.

**Limite atual:** os metodos `OnHitNPC` e `OnKill` do exemplo alimentam tabelas de efeito antecipadas e uma integracao Native especifica. Nao equivalem a callbacks gerenciados generalizados chamados em todos os mods.

## Movimento C# calculado

`SourceBoltMotion.HomingTurn(float)`, `HomingRange()` e `RetargetInterval()` demonstram a politica de homing especifica do SourceBolt.

A convencao `<NomeDoProjetil>Motion.TurnAtTick(int)` gera uma tabela de ate 120 curvas de direcao para cada projetil descoberto. Na versao 0.1.10.16.34, a ativacao por `sourceEnableProjectileAI` ja usa um dispatcher Native compartilhado, mas o compilador experimental ainda valida o UUID do mod de teste.

O sistema guarda ate oito politicas nativas por sessao nessa implementacao. Cada uma e associada ao mod e ao nome do projetil.

## Nao ensinar como suportado

`ModItem`, `ModProjectile`, `ModNPC`, `ModBuff` e outros tipos do tModLoader **nao** fazem parte desta API.

NPCs personalizados, buffs personalizados, acessorios C#, classes de jogador C# e carregamento arbitrario de mods continuam como etapas futuras ate terem API real e teste no Android.
