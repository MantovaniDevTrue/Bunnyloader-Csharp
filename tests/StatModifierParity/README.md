# Lote C# — StatModifier e StatInheritanceData

O lote adiciona ao **SDK C# nativo** as duas estruturas matemáticas da API do tModLoader,
em `BunnyLoader.Source`, preservando assinatura, semântica, nomes de membros e precisão
`float` das operações. A implementação é independente de JavaScript, JNI, MonoVM e
acesso à memória do Terraria: ela calcula valores de forma pura e determinística.

## Escopo fechado deste lote

- `StatModifier`: identidade explícita, construtores, `Base`, `Additive`,
  `Multiplicative`, `Flat`, operadores `+`, `-`, `*`, `/`,
  comparação, hashing, `ApplyTo`, `Undo`, `CombineWith`, `Scale`.
- `StatInheritanceData`: cinco fatores editáveis, `Full`, `None` e
  construtor com parâmetros opcionais idênticos à referência.
- Exemplo C# real compilado nos testes:
  `samples/ExampleModCSharp/Playable/content/Common/Recipes/09_StatModifier.cs`.
- Testes de paridade do cálculo e de assinatura por `dotnet run`:

```sh
dotnet run --project tests/StatModifierParity/StatModifierParity.csproj -c Release
```

O workflow `csharp-statmodifier-source-tests.yml` roda **somente dotnet**.
Ele não aciona NDK, Gradle, assinatura, artefatos Android ou APK.

## Limites claros

**Não é o DamageClassLoader completo.** Estas estruturas não modificam sozinhas
o dano ou os stats do jogador. A futura integração de `DamageClass` deverá
ler as instâncias e aplicar os modificadores nos hooks nativos comprovados,
sem depender do JavaScript. Não confundir o sucesso destes testes de matemática
com validação do Terraria Android.

`default(StatModifier)` conserva zeros, como no tModLoader.
A identidade deve ser `new StatModifier()` ou `StatModifier.Default`.

## Fonte e licença

Implementação matemática adaptada dos fontes do tModLoader:
- [StatModifier.cs](https://github.com/tModLoader/tModLoader/blob/stable/patches/tModLoader/Terraria/ModLoader/StatModifier.cs)
- [StatInheritanceData.cs](https://github.com/tModLoader/tModLoader/blob/stable/patches/tModLoader/Terraria/ModLoader/StatInheritanceData.cs)

O código original é distribuído sob a licença MIT do projeto tModLoader.
Preservar a atribuição e a licença nas distribuições do SDK.
