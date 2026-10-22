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
| BunnyPlayer.MoveSpeedPercent e UpdateEquips | Common/Players/ExampleSwiftPlayer.cs e Content/Items/Accessories/ExampleSwiftAccessory.cs | **experimental**, +8% velocidade enquanto equipado (efeito nativo sem callback MonoVM por quadro) |
| BunnyPlayer.OnEnterWorld / ResetEffects / UpdateEquips | ExampleWelcomePlayer, ExampleFocusPlayer | .90.16 em validação no Android; retorno C# de OnEnterWorld virá como aviso na thread do jogo |
| BunnyPlayer.UpdateMovement(bool) | ExampleMomentumPlayer | .90.16 em validação; +6% aceleração nativa com acessório equipado |
| BunnyPlayer.FrameEffects(bool) | ExampleShadowPlayer | .90.16 em validação; sombra do jogador enquanto equipado |
| BunnyLiveNpc.AI(int,int) | Common/Live/ExampleLiveNpcAI.cs | protótipo experimental, não AI por frame |
| BunnyPrefix.SetDefaults / SetStats / ModifyValue / ApplyAccessoryEffects | Content/Prefixes/ExampleMightyPrefix.cs, ExampleGuardedPrefix.cs | CI .90; **ainda não validado no Android** |

## Diferença entre três rotas

1. **Definição do conteúdo:** classes BunnyWeapon e BunnyProjectile declaram sprites, dano, Shoot, TimeLeft e demais propriedades através de SetDefaults.
2. **Movimento pré-calculado:** a curva TurnAtTick é avaliada em 120 amostras pelo compilador Roslyn, e o código nativo aplica as amostras sem chamar MonoVM a cada tick. O movimento do segundo projétil foi validado com 2.393 atualizações em .72 + mod 0.1.8.
3. **Eventos reais no MonoVM:** métodos estáticos OnHitNPC e OnKill recebem (call, damage) no worker. BunnyLiveEvents é a classe primária; cada tipo adicional pode implementar uma classe NomeDoProjetilLiveEvents em content/Common/Live/ com métodos públicos estáticos retornando int.

Os novos hooks de BunnyPlayer da .90.16 são avaliados pelo Roslyn durante a compilação para gerar descritores. Depois, OnEnterWorld é despachado pela thread principal ao entrar no mundo, e os efeitos de movimento e sombra são aplicados por hooks C++ nativos por quadro. Não se trata ainda de executar arbitrariamente o método C# em cada quadro; a API suporta somente esses retornos limitados, com verificação de ativação e desativação.

Os métodos de instância do BunnyProjectile que geram políticas nativas não são os mesmos callbacks estáticos do worker MonoVM. Não confundir políticas nativas com execução gerenciada.

A fila compartilhada permanece com capacidade 64 e limite agregado de 25.000 eventos por sessão. Há um tipo principal e até sete vínculos adicionais, compartilhados entre mods e projéteis. Dois mods independentes foram validados na .75; pausa/retomada na .77 e reentradas de mundo na .78. Esses resultados cobrem os cenários descritos em [STATUS](../../../../../docs/STATUS.md), sem garantir isolamento completo de falhas, multiplayer ou hot-unload.

## Diagnóstico (.74 em diante)

Use **bl.csharp.managedAvailable()** para verificar inicialização do MonoVM e **bl.csharp.hostInfo()** para consultar runtimeMode, monoVmReady, sourceModCount, nativeAotPrototypeAvailable e nativeAotExternalCount.

**bl.csharp.available() é uma API legada**: verifica o protótipo NativeAOT libbunnycs.so, que não será embutido nem inicializado por padrão na .74. Um resultado false não significa que o MonoVM ou a compilação C# falhou.

Para eventos e movimento, consulte bl.csharp.sourceAutoStatus(), bl.csharp.sourceLiveHitStatus(), bl.csharp.sourceLiveProjectileStatus() e bl.csharp.sourceProjectileAIStatus(modId, nome).

## Limites importantes

A .73 foi validada funcionalmente no Android para 985 callbacks processados de 985 aceitos, incluindo 879/879 de ExampleWaveBolt. Zero falhas, descartes e lockDrops. O FPS da .73 não foi informado nesta rodada. A .74 também foi validada em gameplay; o histórico atualizado está em [STATUS](../../../../../docs/STATUS.md). A telemetria adicional da .79 ainda precisa da validação específica indicada ali e nas notas dessa versão.

O tModLoader pode servir de referência para **forma das classes e semântica dos métodos**. Toda API adicionada ao Bunny precisa ser implementada e testada no SDK, no compilador e na ponte Android; não presumir compatibilidade automática de `ModItem`, `ModNPC`, netcode ou assemblies do PC.


## Contrato atual para novos exemplos

- Callbacks estáticos `OnHitNPC(int call, int damage)` e `OnKill(int call, int damage)` executam no worker Mono, fora da thread do jogo. `call` é uma sequência da sessão compartilhada, não o ID do NPC nem do projétil. O retorno `int` é diagnóstico; ele não altera automaticamente dano ou objetos do Terraria.
- O caminho de políticas nativas de `BunnyProjectile.OnHitNPC/OnKill` é separado desses callbacks. Cada exemplo deve dizer qual dos dois utiliza.
- A fila de 64 posições pode recusar eventos quando cheia. Não prometer entrega garantida. O teto agregado de 25.000 eventos continua sendo uma limitação experimental por sessão; não é um limite por mod.
- Pausar um mod adicional descarta eventos pendentes desse mod; não descarrega sua assembly nem reinicializa os campos estáticos C#. A pausa é recusada se um callback desse mod estiver em execução. O mod principal ainda não possui pausa independente.
- A troca de mundo refaz os vínculos, mas mantém o runtime e os caches. O estado estático de um exemplo deve considerar essa duração de vida.
- A API de mod não deve chamar comandos `bl.csharp.*` do console para se inicializar. Esses comandos servem a diagnóstico e controle do loader.

Veja a [base para a API de modders](../../../../../docs/csharp-api-foundation-v79.md) para as correções de lifecycle e os próximos contratos a estabilizar.

## BannerLoader C# — fonte já integrada, gameplay ainda bloqueado

O `BunnyNPC.SetDefaults()` pode declarar `Banner` e `BannerItem` como nomes locais:

```csharp
Banner = "ExampleSlimeNPC";
BannerItem = "ExampleSlimeBanner";
```

É o equivalente semântico a `Banner = Type` e `BannerItem = ModContent.ItemType<T>()` no tModLoader: o Terraria Android atribui o ID numérico depois da carga, então usar números fixos seria incorreto. Variantes podem referenciar a classe do mesmo NPC representante e compartilhar o contador de mortes. O ExampleMod usa `ExampleSlimeNPC` e `ExampleInvasionEliteSlimeNPC` como exemplos.

O Roslyn emite `NPC_BANNER`, o SourceLoader valida referências locais e o registro C++ publica um snapshot imutável. **Ainda não existe hook de jogo para contar abates, conceder banners, salvar no mundo ou ativar `NearbyEffects`**. A textura do item é exatamente a do ExampleModBunny, mas o tile 1×3 original aguarda suporte de formato e ancoragem no Bunny C#.

O código de base com testes e os bloqueios obrigatórios estão em `docs/csharp-banner-loader-source-gate.md`. Não distribuir APK nem descrever o item como funcional nesta fase.

### BunnyBannerTile — classe-base do ExampleModBunny

A classe `BunnyBannerTile : BunnyTile` aplica os defaults originais (tile 1×3, `FrameImportant`, `NoAttach`, `LavaDeath`, `DisableSmartCursor`, cores de mapa). O host usa `TileObjectData.Style1x2Top`, `AnchorData` de blocos sólidos e a alternativa para plataformas sem precisar de JavaScript.

```csharp
public sealed class ExampleEnemyBannerTile : BunnyBannerTile
{
    public override void SetStaticDefaults()
    {
        base.SetStaticDefaults();
        Texture = "Content/Tiles/Banners/ExampleEnemyBannerTile.png";
        DisplayName = "Example Slime Banner";
    }
}
```

O item declara `DefaultToPlaceableTile("ExampleEnemyBannerTile", 1)`. O estilo **1** é obrigatório para esta textura original: o host busca o item pelo `frameX` e não pressupõe estilo 0. O Roslyn e o parser inicial nativo reconhecem subclasses diretas de `BunnyBannerTile` como `BunnyTile`, mesmo sem atributo adicional.

**Estado atual:** ancoragem e persistência vanilla usam contratos nativos estritos. O registro pode rejeitar o tile se uma assinatura IL2CPP divergir no Android; isso é preferível a colocação incorreta. O desenho por vento `DrawMultiTileVinesInWind` e a entrega/multiplayer ainda NÃO estão finalizados. Não gerar APK até fechar os gates.
