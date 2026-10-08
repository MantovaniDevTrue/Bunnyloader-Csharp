# Source C# Test 0.1.0

Experimento para importar um mod com `main.cs` e `Content/Items/Weapons/SourceWand.cs`, sem bibliotecas nativas ou JavaScript no `.bmod`.

O carregador atual interpreta um **subconjunto declarativo de C#**, não executa qualquer programa C# completo: ele reconhece classes `BunnyProjectile` e `BunnyWeapon` e lê atribuições literais em `SetDefaults()`. A criação e o disparo são realizados pela Host API C++/IL2CPP já existente. A IA é a nativa do Terraria indicada por `AiStyle`.

Não há suporte inicial a `AI()`, `Update()`, blocos condicionais, expressões, chamadas de métodos, classes de NPC, C# dinâmico ou bibliotecas .NET. Essas funcionalidades exigirão a próxima etapa, com um runtime/interpretador C# capaz de executar código, e não apenas declarações.

Instale o APK de teste 0.1.10.11 e importe `SourceCSharpMod-v0.1.0.bmod`. Teste a arma **Source C# Wand** e o projétil **Source C# Bolt**.

Diagnóstico no console (console do aplicativo, não código JS dentro do mod):

```js
JSON.stringify(bl.csharp.sourceMods(), null, 2)
```

Esperado: `loaded: true`, `files: 2`, `items: 1`, `projectiles: 1`, `engine: "SetDefaults-literals-v1"`.
