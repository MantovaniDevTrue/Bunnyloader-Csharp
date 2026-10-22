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
- O MonoVM retém o preload após trocar de mundo, evitando que uma sessão
  antiga descarregue um mod ativo. O snapshot é solicitado apenas uma vez
  por mod, no preload ou no primeiro callback válido. Ainda falta uma
  barreira de inicialização antes de criar itens: o preload é assíncrono.
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
- `DamageClassLoader.ExportDefaultStatSnapshot` versão V2 calcula e exporta
  os cinco canais, cada `StatModifier` e uma matriz de herança 5×5 para
  Generic, Melee, Ranged, Magic e Summon. O MonoVM publica os valores
  validados em registro C++ separado por mod, sem varredura por quadro.
  O ExampleDamageClass produz crítico 8 e armorPen 10; os testes C++
  incluem concorrência, unload e rejeição de entradas inválidas.
- Os hooks de GetWeaponDamage/Crit/Knockback preservam o resultado original e
  observam os campos reais meleeDamage, rangedDamage, magicDamage e minionDamage
  do jogador, com contrato IL2CPP opcional e validação dos números. A projeção
  considera herança parcial, negativa e ampliada de classes personalizadas.
  O diagnóstico separa uma parcela comum aproximada (mínimo dos quatro
  multiplicadores) antes de projetar as parcelas individuais, evitando
  contabilizar repetidamente bônus iguais em classes híbridas. Também
  lê opcionalmente meleeCrit/rangedCrit/magicCrit e meleeSpeed, com
  validação de tipo IL2CPP, para observar herança de crítico e velocidade.
  A parcela comum inferida não distingue exatamente os buffs individuais
  e por isso não deve ser usada como cálculo definitivo de dano.
  Os três hooks continuam retornando o resultado vanilla inalterado,
  limitam os logs por canal e não invocam Mono por disparo.
- Integrar de fato esses atributos aos resultados do Terraria.Player, incluindo
  equipamentos, buffs, reset por jogador, crítico, knockback, velocidade e
  penetração de armadura. A projeção atual é somente diagnóstico e não
  pode ser aplicada como soma ao vanilla sem validar o balanceamento. **O snapshot atual só contém
  valores padrão da classe**, não os totais de um jogador equipado.
  Somá-lo diretamente ao valor vanilla duplicaria modificadores.
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
