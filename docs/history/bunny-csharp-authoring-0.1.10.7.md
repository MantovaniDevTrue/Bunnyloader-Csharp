# Bunny C# Authoring 0.1.10.7

Esta etapa amplia o SDK de autoria C# para gerar registros do ExampleModBunny sem exigir código JavaScript do desenvolvedor. A camada de execução NativeAOT continua igual à 0.1.10.5, mantendo ABI 1 e Host API 1.

## Registros adicionados

- `BunnyAccessory`: equipamento com bônus percentual de dano corpo a corpo, distância e magia.
- `BunnyArmor`: peças de cabeça, peitoral e pernas, defesa, mana, minions e movimento.
- `BunnyArmorSet`: bônus de dano para o conjunto completo.
- `BunnyBuff`: bônus ou redução de defesa e sinalização de debuff.
- `BunnyBuffPotion`: item consumível com duração definida e aplicação de buff.
- `BunnyMaterial`: item material empilhável.
- `BunnyNpcDrop`: drops comuns de NPC com chance e quantidade configuráveis.

A geração usa diretamente os padrões de `UpdateAccessory`, `UpdateEquip`, `AddArmorSets`, `UpdateArmorSet`, `ModBuff.UpdatePlayer`, `Item.buffType` e `ItemDropRule.Common` dos exemplos do ExampleModBunny.

## Exemplo em C#

```csharp
content.Add(new BunnyAccessory("NativeStatCharm")
{
    DamagePercent = 10
});
content.Add(new BunnyBuff("NativeDefenseBuff")
{
    DefenseBonus = 10
});
content.Add(new BunnyBuffPotion("NativeDefensePotion", "NativeDefenseBuff")
{
    DurationTicks = 3600
});
```

O desenvolvedor só precisa do código C#, dos arquivos PNG e do workflow de compilação. Durante a geração, o empacotador valida nomes, referências e valores, gerando automaticamente os arquivos JavaScript internos do Bunny e o pacote com a biblioteca NativeAOT.

## Testes

Utilizar o Bunny C# Test 0.1.10.5 e importar `ExampleCSharpMod 0.1.7`.

1. Verificar as três armas e o slime da versão anterior.
2. Equipar `NativeStatCharm` e verificar o bônus de dano.
3. Equipar `NativeArmorHelmet`, `NativeArmorBreastplate` e `NativeArmorLeggings`; conferir sprites, defesa, mana, minions, velocidade e bônus de conjunto.
4. Consumir `NativeDefensePotion`; verificar o buff e +10 de defesa por 3600 ticks.
5. Derrotar `NativePhaseSlime`; conferir o drop de `NativeSlimeShard` (50% de chance).
6. Repetir em um cenário com vários projéteis e NPCs para avaliar os 60 FPS.

O compilador e o validador de ZIP não provam que todos os efeitos funcionam no jogo; a validação definitiva depende dos testes no Android.

## Compatibilidade e limites

- Mods JavaScript existentes continuam usando a API original, sem conversão obrigatória.
- Mods C# publicados anteriormente continuam suportados pela ABI 1.
- A experiência de desenvolvimento é C#; o formato interno do pacote ainda contém JavaScript gerado para registros e eventos do Bunny.
- Buffs e itens de equipamento atualmente usam hooks do ExampleModBunny; a lógica de IA de projéteis e NPCs continua em NativeAOT.
- Este exemplo não implementa sincronização multiplayer para comportamento C#.
- As primeiras APIs são tipadas e limitadas. Não prometem equivalência com todo o conteúdo e todas as classes do tModLoader.

Arquivos principais: `BunnyCsSdk/BunnyContentAuthoring.cs`, `BunnyCsPackager/Program.Advanced.cs`, `ExampleCSharpMod/ContentDefinition.cs` e `build-bunny-csharp-authoring.yml`.
