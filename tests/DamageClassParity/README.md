# DamageClass / DamageClassLoader — C# (tModLoader 1.4.5)

## Implementado nesta primeira etapa

- API gerenciada `DamageClass` com as dez classes vanilla, herança
  de modificadores, efeitos, prefixos e linhas de tooltip.
- `DamageClassLoader` com IDs vanilla 0–9, registro tipado,
  cache estável de efeito (apenas na carga), busca e descarregamento.
- `DamageClassPlayer` com `ref` para os cinco atributos e cálculos
  `GetTotal*` correspondentes a Terraria.Player.TML.cs.
- ExampleMod: `Common/DamageClasses/ExampleDamageClass.cs`.
- 22 testes incluindo Custom DamageClass, herança negativa e parcial,
  reset de atributos, unload, IDs e rollback de registro.
- O CI compila o SDK Android net8 e o ExampleMod completo via Roslyn,
  sem Gradle, NDK, APK ou assets de jogo.

## O que falta para o DamageClassLoader ficar completo no Terraria Android

**Este código não é a ponte nativa. Não deve ser descrito como loader de
combate completo nem usado para autorizar geração de APK.**

- Registrar subclasses `DamageClass` automaticamente ao carregar mods
  C# no dispatcher de montagem de assemblies e descarregá-las por mod.
- Resolver `DamageType` dos itens/projéteis C# e manter o tipo no
  clone, no spawn e na sincronização de rede.
- Ler e aplicar `GetTotalDamage`, crítico, knockback, armorPen e
  attackSpeed aos campos reais de Terraria.Player no tempo correto.
- Encaminhar efeitos, prefixos e tooltips pelos pontos nativos exatos
  do jogo, sem invocar Mono por frame quando não houver mod.
- Fazer testes de contrato C++/C# e smoke QA em Android/arm64; somente
  depois desse fechamento considerar artefatos Android.

## Fontes de referência

- [DamageClass.cs](https://github.com/tModLoader/tModLoader/blob/1.4.5/patches/tModLoader/Terraria/ModLoader/DamageClass.cs)
- [DamageClassLoader.cs](https://github.com/tModLoader/tModLoader/blob/1.4.5/patches/tModLoader/Terraria/ModLoader/DamageClassLoader.cs)
- [VanillaDamageClasses.cs](https://github.com/tModLoader/tModLoader/blob/1.4.5/patches/tModLoader/Terraria/ModLoader/VanillaDamageClasses.cs)
- [Player.TML.cs](https://github.com/tModLoader/tModLoader/blob/1.4.5/patches/tModLoader/Terraria/Player.TML.cs)

Código original tModLoader sob licença MIT, com atribuição preservada
em `THIRD_PARTY_NOTICES.md`.
