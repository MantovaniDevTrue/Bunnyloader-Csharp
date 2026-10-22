# DamageClass / DamageClassLoader — C# (tModLoader 1.4.5)

## Implementado nesta primeira etapa

- API gerenciada `DamageClass` com as dez classes vanilla, herança
  de modificadores, efeitos, prefixos e linhas de tooltip.
- `DamageClassLoader` com IDs vanilla 0–9, registro tipado,
  cache estável de efeito (apenas na carga), busca e descarregamento.
- `DamageClassPlayer` com `ref` para os cinco atributos e cálculos
  `GetTotal*` correspondentes a Terraria.Player.TML.cs.
- ExampleMod: `Common/DamageClasses/ExampleDamageClass.cs`, aplicado como
  identidade em `ExampleWaveWand` e `ExampleWaveBolt`.
- 26 testes incluindo Custom DamageClass, herança negativa e parcial,
  reset de atributos, unload por mod, IDs não reutilizados, testes com
  dois assemblies e rollback de registro.
- O compilador completo do ExampleMod registra ExampleDamageClass automaticamente
  pelo mesmo BunnyEntitySourceDispatcher dos outros conteúdos do mod.
- Parser C++ agora reconhece classes `DamageClass` mesmo sem SetDefaults,
  e mods compostos apenas por essas classes podem ser compilados e registrados;
  teste dedicado executa compilação Roslyn e descarregamento do assembly mínimo.
- O MonoVM agenda uma inicialização de assembly antes do primeiro TickWorld;
  não depende mais de receber AI de NPC para iniciar o registro.
- O CI compila o SDK Android net8, executa 26 testes e o ExampleMod via Roslyn,
  além de verificar a sintaxe de ManagedRuntime.cpp, SourceLoader.cpp e
  AutoSourceRuntime.cpp, executar testes nativos do parser e registro
  por mod, o wire `DAMAGE_TYPE`, o mapeamento vanilla de sinalizadores
  e a sintaxe de NativeContent.cpp, sem Gradle ou APK.

## O que falta para o DamageClassLoader ficar completo no Terraria Android

**A ponte nativa ainda é parcial. Não descrever como loader de combate
completo nem autorizar geração de APK.**

- A revogação por mod já preserva IDs: remove classes do catálogo ativo,
  não reutiliza o ID antigo e limpa no worker MonoVM. O parser e o carregador
  também reconhecem mods sem entidade que contenham apenas DamageClass;
  ainda falta verificar esse processo em arm64 no jogo.
- Garantir um bloqueio de inicialização que aguarde o registro antes de
  criar itens; a fila de preload é assíncrona e não satisfaz essa garantia.
- `DamageType` agora existe em `BunnyItem`, `BunnyWeapon` e `BunnyProjectile`.
  O compilador emite `DAMAGE_TYPE` com ID vanilla ou nome completo da classe,
  e o runtime nativo valida ownership, cacheia por tipo e revoga por mod.
  Os hooks nativos de Item.SetDefaults/Projectile.SetDefaults agora consultam
  o binding em snapshot sem copiar strings por projétil: Melee, Ranged, Magic
  e Summon usam seus sinalizadores compatíveis do Terraria mobile.
  NativeDefaults roda antes da classificação, e Item.RebuildTooltip depois.
  Generic e classes customizadas não são classificadas incorretamente como
  Ranged; MeleeNoSpeed é neutra até existir cálculo nativo que preserve a
  ausência de bônus de velocidade. IDs V0, híbridos e Throwing mantêm os
  defaults existentes onde não há correspondência exata.
  Ainda falta preservar a classificação nos clones, verificar a rede e,
  principalmente, aplicar os cinco atributos personalizados ao combate.
  Esse mapeamento parcial não representa paridade completa de DamageClass.
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
