# API C# suportada: Bunny Loader Roslyn + MonoVM

Fonte principal de assinaturas: [BunnySourceScriptApi.cs](../../../experiments/csharp/BunnyCsSdk/BunnySourceScriptApi.cs). O ExampleModBunny é a referência de estrutura e comportamento, não as classes do tModLoader.

| API do Bunny | Arquivo do ExampleModCSharp | Prova no Android |
| --- | --- | --- |
| BunnyWeapon.SetDefaults | ExampleWand.cs, ExampleWaveWand.cs | duas armas registradas na .72/.73 |
| BunnyProjectile.SetDefaults | ExampleBolt e ExampleWaveBolt | dois tipos ativos na .72/.73 |
| ProjetoMotion.TurnAtTick(int) | ExampleBoltMotion e ExampleWaveBoltMotion | duas curvas independentes, 120 amostras |
| BunnyProjectile.OnHitNPC / OnKill | ExampleBolt, efeito nativo OnFire 24 | efeitos e callbacks preservados |
| BunnyLiveEvents.OnHitNPC(int,int) / OnKill(int,int) | Common/Live/BunnyLiveEvents.cs | callback primário .72/.73 |
| ProjetoLiveEvents.OnHitNPC(int,int) / OnKill(int,int) | Common/Live/ExampleWaveBoltLiveEvents.cs | callback independente .73: 879/879 |
| BunnyBuff, BunnyBuffPotion, BunnyAccessory, BunnyPlayer, BunnyNPC | Content/ e Common/Players | exemplos presentes, validação específica por API |
| BunnyLiveNpc.AI(int,int) | Common/Live/ExampleLiveNpcAI.cs | protótipo experimental, não AI por frame |

## Diferença entre três rotas

1. **Definição do conteúdo:** classes BunnyWeapon e BunnyProjectile declaram sprites, dano, Shoot, TimeLeft e demais propriedades através de SetDefaults.
2. **Movimento pré-calculado:** a curva TurnAtTick é avaliada em 120 amostras pelo compilador Roslyn, e o código nativo aplica as amostras sem chamar MonoVM a cada tick. O movimento do segundo projétil foi validado com 2.393 atualizações em .72 + mod 0.1.8.
3. **Eventos reais no MonoVM:** métodos estáticos OnHitNPC e OnKill recebem (call, damage) no worker. BunnyLiveEvents é a classe primária; cada tipo adicional pode implementar uma classe NomeDoProjetilLiveEvents em content/Common/Live/ com métodos públicos estáticos retornando int.

Os métodos de instância do BunnyProjectile que geram políticas nativas não são os mesmos callbacks estáticos do worker MonoVM. Não confundir políticas nativas com execução gerenciada.

A fila compartilhada permanece com capacidade 64 e limite agregado de 25.000 eventos por sessão. Atualmente há suporte experimental a um tipo principal e até sete adicionais **do mesmo mod**. Vários mods independentes no mesmo worker ainda não foram validados.

## Diagnóstico na .74

Use **bl.csharp.managedAvailable()** para verificar inicialização do MonoVM e **bl.csharp.hostInfo()** para consultar runtimeMode, monoVmReady, sourceModCount, nativeAotPrototypeAvailable e nativeAotExternalCount.

**bl.csharp.available() é uma API legada**: verifica o protótipo NativeAOT libbunnycs.so, que não será embutido nem inicializado por padrão na .74. Um resultado false não significa que o MonoVM ou a compilação C# falhou.

Para eventos e movimento, consulte bl.csharp.sourceAutoStatus(), bl.csharp.sourceLiveHitStatus(), bl.csharp.sourceLiveProjectileStatus() e bl.csharp.sourceProjectileAIStatus(modId, nome).

## Limites importantes

A .73 foi validada funcionalmente no Android para 985 callbacks processados de 985 aceitos, incluindo 879/879 de ExampleWaveBolt. Zero falhas, descartes e lockDrops. O FPS da .73 não foi informado nesta rodada. A .74 precisa de nova validação de gameplay porque muda a inicialização do APK.

Não copiar ModItem, ModProjectile, ModNPC, ModBuff, AI e outros métodos do tModLoader, nem afirmar compatibilidade automática com mods de PC, netcode ou APIs .NET não testadas.
