# SDK C# — autoria sem JavaScript

O desenvolvedor implementa:

- `BunnyContentModule`, com `Info` e `Register(BunnyContentRegistry)`
- Classes de comportamento C# como `BunnyNpcBehavior` e `BunnyProjectileBehavior`
- Sprites PNG em `Assets/Content/`

O `BunnyCsPackager` carrega a DLL C# gerenciada para extrair os descritores,
gera os arquivos internos equivalentes ao ExampleModBunny e os coloca no mesmo
`.bmod` que contém `native/arm64-v8a/libmod.so`.

A DLL gerenciada só é usada no computador durante a compilação. No Android o
comportamento C# roda em NativeAOT. O JavaScript interno é uma camada de registro
e compatibilidade do Bunny, não uma linguagem que o autor precise escrever.

## Exemplo

```csharp
public sealed class ContentDefinition : BunnyContentModule
{
    public override BunnyModInfo Info => new(
        "SEU-UUID", "meumod", "Meu Mod", "1.0.0", "Autor", "Descrição");

    public override void Register(BunnyContentRegistry content)
    {
        content.Add(new BunnyProjectile("MinhaFlecha"));
        content.Add(new BunnyWeapon("MeuArco", "MinhaFlecha")
        {
            Damage = 30,
            ShootSpeed = 12f
        });
    }
}
```

A ligação de IA C# aos nomes de projéteis continua sendo feita pelos exports
NativeAOT existentes. O exemplo inicial mantém ABI 1 / Host API 1 e o APK 0.1.10.5.

Limite importante: isto demonstra *autoria C# apenas*, não elimina a engine
JavaScript do Bunny. Para remover completamente o JavaScript em execução
precisaremos implementar o registro de conteúdo diretamente na Host API nativa.
