# ExampleMod C# — preparação de QA antes do APK

Base: PR #159 (`cc8c1f9`), preservando WorldNet e o TouchLoader já validado.
Reutiliza Random (#136/#137) e contratos de colisão (#144). Sem APK neste lote.

## Random

`ExampleRandomCompatProbePlayer.OnEnterWorld` executa uma vez por entrada:
32 comparações de seed, SetSeed, 32 escolhas com pesos zero/positivo, bool,
float, vetor unitário e lista. Esperado no relatório de entrada:
`Bunny C# Random Compat: PASS (seed/weights/bool/float/vector/list)`.
Ausência da mensagem exige investigar registro/callback; não conta como sucesso.
Não usa o estado de Main.rand e não deve rodar por frame.

## Colisão — singleplayer

Obtenha no catálogo de QA os itens `Example QA Collision Bounce Wand`,
`Example QA Collision Stop Wand` e `Example QA Collision KeepAlive Wand`.
Usam o sprite existente SourceWand e a bala existente, sem assets novos.

- Atire contra parede e chão: Bounce deve rebater; Stop deve parar;
  KeepAlive preserva a vida no impacto sem implementar ricochete.
- Todos expiram em 180 updates e têm limite de três impactos sobrevividos.
  Confira também plataformas sólidas e a caixa 6×6 do projétil visual 8×8.
- O callback OnTileCollide informa o impacto posteriormente no worker.
  A política imediata é nativa. O contador Impacts de cada classe corresponde
  aos callbacks desde o último spawn dessa classe; não é total por instância.
- A política de colisão da #144 é singleplayer. Não contar este teste como
  validação de colisão multiplayer; WorldNet tem seu roteiro próprio.
- Compare armas/projéteis vanilla e os exemplos anteriores, depois saia e
  reabra o mundo. Verifique os logs de binding: recusa de ABI não é aprovação.

A integração reserva eventos diferentes para tick de mundo (6) e colisão (7).
Preserva as categorias Wall/Dust e o registro de Rarity/Command/WorldSystem.

## Árvore/Gore — dependências ainda fora desta base

A PR #98 não é apenas um conjunto de amostras: seu delta contra a base antiga
abrange 277 arquivos, incluindo formatos TILE, callbacks, desenho de tiles,
registro TREE/GORE e runtime nativo. A base #159 não possui BunnyTree/BunnyGore
nem as amostras correspondentes. Copiar somente ExampleTree/ExampleSapling
impediria a compilação do ExampleMod.

Integrar esse runtime em lote próprio, com os testes `world_loaders`, antes de
ativar as três classes e seus cinco PNGs originais no Playable. O roteiro desse
lote deve cobrir crescimento, Shake, drops, folhas, save/load e multiplayer.
Este lote não declara TreeLoader/GoreLoader integrados ou prontos no aparelho.

## Evidências

Host: contratos Random, execução dos mesmos exemplos enviados ao mod,
serialização de colisão pelo Roslyn, políticas/ABI/geometria nativas,
lifecycle de entidades e 52 checks de WorldSystem/WorldNet.
CI inclui compilação/link ARM64 sem montar APK.
Nenhum desses resultados comprova gameplay, rede entre aparelhos ou FPS Android.
