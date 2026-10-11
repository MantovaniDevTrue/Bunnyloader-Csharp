# API C# comprovável do Bunny Loader

Fonte canônica do SDK: `experiments/csharp/BunnyCsSdk/BunnySourceScriptApi.cs`. Estes exemplos usam o Bunny Loader e seu ExampleModBunny como base; não usam classes do tModLoader.

## BunnyWeapon

`public override void SetDefaults()` define `DisplayName`, `Texture`, `Damage`, `UseTime`, `ShootSpeed` e `Shoot`. O valor de `Shoot = nameof(ClasseDoProjetil)` vincula a arma a um `BunnyProjectile` **do mesmo mod**.

Exemplos do pacote 0.1.8: `ExampleWand` e `ExampleWaveWand`. O teste com duas armas foi compilado no CI; gameplay Android ainda depende da validação no aparelho.

## BunnyProjectile

`public override void SetDefaults()` define `DisplayName`, `Texture`, `Width`, `Height`, `AiStyle`, `Friendly`, `Penetrate` e `TimeLeft`.

`OnHitNPC(int damage)`, `OnKill(int damage)` e `OnKillDuration(int damage)` são contratos opcionais: as implementações reconhecidas podem gerar tabelas de efeito nativo durante a compilação; **não** são chamadas gerenciadas a cada quadro. O exemplo `ExampleBolt` usa tabelas para buff 24. O novo `ExampleWaveBolt` não sobrescreve esses métodos, isolando o teste de movimento da aplicação de efeitos.

Os métodos estáticos `BunnyLiveEvents.OnHitNPC(int call, int damage)` e `OnKill(int call, int damage)` pertencem a uma **convenção experimental separada**, ligada automaticamente a um único tipo de projétil no processo atual. Não afirmar que o segundo projétil já possui callbacks live independentes.

## Movimento C# com curva por tipo

A classe `<NomeDoProjetil>Motion` com método `public static float TurnAtTick(int tick)` gera 120 amostras no Roslyn. O hook nativo seleciona a curva pelo tipo registrado, sem invocar o MonoVM a cada tick. No teste 0.1.8:

- `ExampleBoltMotion`: curva já validada como baseline no Android .72.
- `ExampleWaveBoltMotion`: curva senoidal independente de `0.018f` radianos no máximo por amostra, compilada e verificada pelo teste automático.

A arquitetura atual permite até oito políticas de curva ativas por sessão. Isso não elimina o limite independente de **um vínculo de eventos live por processo** no fluxo automático atual.

## Outros contratos presentes no SDK

`BunnyBuff`, `BunnyBuffPotion`, `BunnyAccessory`, `BunnyPlayer` e `BunnyNPC` possuem implementações de exemplo. Seus registros e propriedades são experimentais; `BunnyNPC` expõe propriedades de registro e `SetDefaults()`, não uma API C# genérica `AI()` por frame. O callback estático `BunnyLiveNpc.AI(int,int)` é um experimento separado com limites de segurança.

## Limites

- A .72 foi validada no Android para o ExampleModCSharp 0.1.7 com um único projétil principal de callbacks live, sem perdas nos cenários observados.
- O ExampleModCSharp 0.1.8 amplia para duas armas e dois projéteis. **CI aprovado não equivale a gameplay aprovado.**
- Modos multiplayer, compatibilidade irrestrita com mods C# de PC, IA Mono arbitrária por quadro e callbacks live separados por tipo/mod não estão validados.
- `ModItem`, `ModProjectile`, `ModNPC` e `ModBuff` do tModLoader não pertencem à API do Bunny Loader.
