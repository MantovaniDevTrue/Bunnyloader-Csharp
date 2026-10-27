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

## Combate no Terraria Android (integração na QA, PR #180)

Os hooks deixaram de só observar. Para itens cujo `DamageType` é uma classe do
mod, `GetWeaponDamage`, `GetWeaponCrit` e `GetWeaponKnockback` devolvem o valor
da classe (`StatModifier` e `Player.TML.cs` do tModLoader), e
`ApplyItemAnimation`/`ApplyItemTime` usam a velocidade total. Itens vanilla não
mudam; qualquer contrato IL2CPP ausente mantém o vanilla.

- **Generic rastreado na origem.** O Terraria mobile não tem `allDamage`: bônus
  genéricos somam o mesmo valor nos quatro campos de dano (e nos três de
  crítico). `ApplyEquipFunctional`, `GrantPrefixBenefits`, `GrantArmorBenefits`,
  `UpdateArmorSets` e `UpdateBuffs` são medidos antes/depois; a parte comum vira
  Generic, o resto fica com a classe. `ResetEffects` zera o quadro do jogador.
  Fontes não rastreadas ficam com a classe (o Generic nunca é inflado).
- **Padrões da classe.** Vêm do snapshot V2 (totais num jogador zerado); o
  crítico 4 do Generic já incluso é descontado para não contar duas vezes.
- **Knockback.** Medido no `GetWeaponKnockback` original com a flag de cada
  classe (mapa afim), então regras vanilla (luva, buffs, `minionKB`) passam.
- **Bônus de equipamento.** `BunnyItem.AddClassBonusOnEquip(new BunnyClassBonus(classe) { ... })`
  → linha `CLASS_BONUS` → registro nativo por item, aplicado no equip sem Mono
  por frame. Generic/Melee/Ranged/Magic/Summon escrevem nos campos vanilla
  (itens vanilla também se beneficiam); classes do mod vão para o canal do
  jogador. Combinações sem campo no jogo mobile são recusadas no carregamento.
- **Barreira antes do mundo.** A entrada no mundo espera o snapshot de cada mod
  com DamageClass (até 10 s; depois libera com aviso).

Testes de host: `native_damageclass_combat.cpp` e `native_damageclass_tracking.cpp`
(ASan/UBSan, no workflow unified), além dos testes nativos e gerenciados
anteriores. **Ainda falta validação no Android.**

### Limites conhecidos

- Bônus de classe em buffs de mod ainda não: o parser antecipado de `BunnyBuff`
  só aceita atribuições.
- `MeleeNoSpeed`, `SummonMeleeSpeed`, `MagicSummonHybrid`, `Throwing` e `Default`
  não têm campo próprio no jogo mobile; bônus de equipamento para elas são
  recusados (como `DamageType` de arma continuam funcionando pela herança).
- Penetração de armadura de classe customizada vale só pelo padrão da classe e
  pelo Generic vanilla; bônus por jogador de penetração para classe do mod não
  alteram o acerto.
- Rede (multiplayer) e clones de item não foram validados.

## Fontes de referência

- [DamageClass.cs](https://github.com/tModLoader/tModLoader/blob/1.4.5/patches/tModLoader/Terraria/ModLoader/DamageClass.cs)
- [DamageClassLoader.cs](https://github.com/tModLoader/tModLoader/blob/1.4.5/patches/tModLoader/Terraria/ModLoader/DamageClassLoader.cs)
- [VanillaDamageClasses.cs](https://github.com/tModLoader/tModLoader/blob/1.4.5/patches/tModLoader/Terraria/ModLoader/VanillaDamageClasses.cs)
- [Player.TML.cs](https://github.com/tModLoader/tModLoader/blob/1.4.5/patches/tModLoader/Terraria/Player.TML.cs)

Código original tModLoader sob licença MIT, com atribuição preservada
em `THIRD_PARTY_NOTICES.md`.
