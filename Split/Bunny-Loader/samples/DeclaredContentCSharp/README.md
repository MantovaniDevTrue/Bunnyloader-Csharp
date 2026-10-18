# Declared Content CSharp

Exemplo de conteúdo C# com metadados declarados e uma classe-base genérica reutilizável.

## Arquivos

- [Playable/manifest.json](Playable/manifest.json): UID próprio, versão, `entry: "main.cs"`, `blVersion` e `gameVersion`.
- [Playable/content/main.cs](Playable/content/main.cs): entrada declarada do pacote.
- [Playable/content/Content/](Playable/content/Content/): definições C# e conteúdo.

A classe `SharedMaterial<T>` no exemplo deriva de `BunnyItem` e demonstra `SetDefaults()`, `MaxStack` e `Value`. O pacote `Playable/` é independente do [Starter Mod](../StarterModCSharp/README.md).

Este é um exemplo de autoria/contratos do SDK, **não** prova por si só todos os comportamentos no Android. Consulte [SDK C#](../../../../docs/SDK-CSharp.md), [matriz de validação](../../../../docs/SDK-VALIDACAO.md) e [instruções de integração no Bunny original](../../../../docs/INTEGRACAO-BUNNY-ORIGINAL.md) antes de distribuir ou portar.
