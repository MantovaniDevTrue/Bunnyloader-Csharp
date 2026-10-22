# SceneEffectLoader: núcleo de ranking (parcial)

**Estado: SceneEffectLoader continua pendente.** Esta entrega é só o núcleo
puro de ranking, em C++, sem IL2CPP, sem hooks e sem Android.

## O que existe

- `app/src/main/cpp/content/scene/SceneEffects.{h,cpp}`: dado o conjunto de
  efeitos de cena de um jogador, devolve a escolha de cada canal (música, fundo
  de superfície, fundo de subsolo, água, fundo do mapa) e a lista ativa
  ordenada.
- `tests/scene_effects/`: teste de host (`bash tests/scene_effects/run.sh`),
  compilado com `-Wall -Wextra -Werror`.

## Contrato espelhado do JS original (`SceneEffectLoader.UpdateSceneEffect`)

- `rank = clamp(peso, 0, 1) + prioridade`; NaN vale 0 e os infinitos limitam
  em 0 ou 1.
- Empate de `rank`: chave `uid/Classe` em ordem crescente de bytes. O
  resultado não depende da ordem de carga (testado nas 120 permutações de 5
  efeitos).
- Cada canal vem do primeiro efeito ativo, na ordem do rank, que o preenche.
- Música: só `-1` conta como "não preenche"; `0` é silêncio e vale como valor.
- Peso de superfície/subsolo na escolha: `rank - prioridade`.
- Fundo do mapa: primeiro caminho não vazio.

## O que falta (nada disto está feito)

- Ponte gerenciada: `BunnySceneEffect` no SDK C# (`IsSceneEffectActive`,
  `GetWeight`, `SpecialVisuals`, `MapBackgroundColor`) chamando este núcleo.
- Hook de `Player.UpdateBiomes` com contratos IL2CPP verificados e a cena por
  jogador (o campo `CurrentSceneEffect` do original).
- Ligação com os consumidores: Surface, Underground, Water, Map e Music.
- `BiomeLoader` (flags por jogador, OnEnter/OnLeave/OnInBiome, sincronização
  em rede, localização, saída do mundo), que é quem dispara a cena a cada
  quadro.
- Fixtures com providers reais, exemplo C#, CI e validação no Android.

O ambiente desta entrega não tinha .NET: nada do lado C# foi compilado aqui.
