# Bunny Loader C# 0.1.10.16.73 — múltiplos callbacks live por projétil

**Estado:** implementação experimental e testes host em CI; **ainda sem validação Android**. Base da implementação: .72 Managed Ingress (já testada), ExampleModCSharp .018 (duas curvas validadas no Android).

## O que muda

- Uma nova vinculação de callbacks **por tipo de projétil** seleciona classes estáticas gerenciadas independentes, preservando `BunnyLiveEvents` e seus métodos para o projétil principal.
- A convenção opcional é `content/Common/Live/<NomeDoProjetil>LiveEvents.cs` contendo `BunnyLoader.Source.<NomeDoProjetil>LiveEvents` com `public static int OnHitNPC(int call,int damage)` e `public static int OnKill(int call,int damage)`.
- Os eventos continuam no mesmo `ManagedLiveIngressQueue` (FIFO64, limite agregado 25.000) e no mesmo worker MonoVM da .72. O evento recebe `bindingSlot` para que o worker escolha a classe adequada. O hook nativo continua executando a curva pré-calculada: **não há callback C# por frame**.
- Cada vinculação secundária possui seus próprios contadores `queued, processed, hits, kills, failures, lastResult, error`. A API de diagnóstico é `bl.csharp.sourceLiveProjectileStatus()`. O status global `sourceLiveHitStatus()` continua disponível e seus totais incluem os eventos secundários.
- A `ExampleWaveBolt` permanece sem buff OnFire; os hooks só enviam seus eventos para o MonoVM. `ExampleBolt` mantém o buff, dano, movimento e seus callbacks originais.
- Por segurança nesta etapa, permite **até sete tipos adicionais no mesmo mod** da sessão automática principal (oito no total). Isolamento multi-mod e descarregamento seguro ainda não foram liberados.

## Teste Android recomendado

Instalar APK .73 por cima da .72 (sem limpar dados) e **substituir** ExampleModCSharp 0.1.8 pelo 0.1.9. Usar singleplayer e um mundo de testes. Verificar que as duas armas continuam disponíveis e disparam na direção da mira. Atacar inimigos com as duas durante pelo menos 20 segundos, observando FPS e efeitos. Executar:

```javascript
JSON.stringify({
  auto: bl.csharp.sourceAutoStatus(),
  original: bl.csharp.sourceProjectileAIStatus(
    "68c3115f-62f4-49f2-a56b-388256781d5f", "ExampleBolt"),
  wave: bl.csharp.sourceProjectileAIStatus(
    "68c3115f-62f4-49f2-a56b-388256781d5f", "ExampleWaveBolt"),
  live: bl.csharp.sourceLiveHitStatus(),
  perProjectile: bl.csharp.sourceLiveProjectileStatus()
}, null, 2)
```

**Resultado esperado:**

1. `sourceAutoStatus` com `phase=ready`, `curves=2` e `enabledCurves=2`, sem erro.
2. `sourceLiveHitStatus` com `enabled=true`, `automatic=true`, `phase=ready` e `dropped=0`, `lockDrops=0`, `failures=0`.
3. `perProjectile` tem uma entrada com `projectile="ExampleWaveBolt"`, `enabled=true`, classe `ExampleWaveBoltLiveEvents`, e `queued=processed > 0` após combate com a segunda arma. Conferir `hits` e `kills` conforme a duração do projétil e os inimigos atingidos.
4. Com ambas as armas, `sourceLiveHitStatus().queued` e `processed` crescem e a fila volta a `queueDepth=0`. O `projectileType` global indica apenas o tipo primário; a entrada secundária tem seu ID separado.
5. `lastResult` da classe secundária deve ser da ordem de **1000+** para hits ou **2000+** para kills, pois seus métodos C# usam estados estáticos próprios (não são os cálculos de `BunnyLiveEvents`).
6. Verificar FPS visual próximo de 60 e nenhum fechamento, inclusive com rajadas simultâneas.

**Importante:** `queued` e `processed` no status secundário podem diferir momentaneamente enquanto houver eventos pendentes; esperar a fila drenar antes da captura. Uma compilação CI bem-sucedida não prova comportamento ou FPS real do Android.

## Regressões que devem permanecer aprovadas

- Status da .72, quando apenas ExampleModCSharp 0.1.7 estiver instalado.
- Limite sintético de 25 mil callbacks (opt-in, com interrupção controlada; o benchmark pode desarmar vínculos secundários).
- Efeitos `OnFire` de `ExampleBolt`, item original e buff.
- Gameplay de 60 FPS conforme medido visualmente nas versões .72 + ExampleModCSharp 0.1.8.
- Nenhum registro automático no multiplayer sem validação específica.

**Não substituir o estado estável .72 até a .73 ser testada no dispositivo.**
