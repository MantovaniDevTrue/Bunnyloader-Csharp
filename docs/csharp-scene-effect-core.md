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

## Ponte C# (escrita, sem validação no jogo)

- `BunnySceneEffect` (SDK): `Priority`, `GetWeight`, `IsSceneEffectActive`,
  `SpecialVisuals`, `SurfaceBackgroundStyle` e `UndergroundBackgroundStyle`
  (o `Type` do estilo; resolvido por UID do mod + nome da classe).
- O registro liga em `Terraria.Player.UpdateBiomes` pelo mecanismo de hook que
  o SDK já tem (`HookVoid`), roda depois do original e só para o jogador local.
- O vencedor de cada canal é ativado pelos serviços nativos que já existem
  (`BunnySurfaceBackgrounds.Set` e `BunnyUndergroundBackgrounds.Set`) e o
  anterior é desligado na troca. Sem JavaScript no caminho.
- O ranking gerenciado (`BunnySceneRanking`) repete o do núcleo nativo, com o
  mesmo conjunto de casos de teste (`tests/scene_effects/Program.cs`). Os dois
  precisam andar juntos. A comparação de chaves do C# é por unidades UTF-16 e a
  do C++ é por bytes UTF-8: só diferem para chaves com caracteres fora do plano
  básico.
- Música, água e fundo do mapa não existem nesta ponte: não há consumidor C#
  para esses canais, e a PR não adiciona API vazia.

## O que falta (nada disto está feito)

- Validar no Android: o contrato de `Player.UpdateBiomes` (assinatura e quadro
  em que roda) e o efeito dos `Set` a cada troca de vencedor.
- Canais de música, água e mapa, quando os loaders deles existirem.
- Exemplo C# com um `BunnySceneEffect` ligando os dois fundos do ExampleMod.
- `BiomeLoader` (flags por jogador, OnEnter/OnLeave/OnInBiome, rede,
  localização, saída do mundo) e a cena de jogadores remotos.
- Fixtures com providers reais e a matriz atualizada para "implementado".

Este código foi escrito sem compilador .NET local: quem compila é o CI.
