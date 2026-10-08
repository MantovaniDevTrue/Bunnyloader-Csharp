# Bunny C# — exemplo sem JavaScript escrito pelo desenvolvedor

Este exemplo mostra o fluxo de autoria inteiramente em C# no Bunny Loader.

O código dos itens, NPCs, projéteis e receitas está em
`experiments/csharp/ExampleCSharpMod/ContentDefinition.cs`.
A IA, o movimento e os ataques continuam nas classes C# e são compilados em NativeAOT.

A pasta `Assets/Content/` contém as texturas. Não existe nenhum arquivo `.js`
na pasta deste exemplo. Durante o build, o `BunnyCsPackager` converte as declarações
C# em uma camada de compatibilidade JavaScript interna ao `.bmod`.

Essa camada não exige que o autor aprenda JavaScript. Ela usa as APIs reais
do ExampleModBunny, preservando o mecanismo já testado para registrar conteúdo.
O JavaScript é gerado, não executado pelo programador manualmente.

## Como criar o pacote

No PC ou GitHub Actions, usando .NET 10 e Android NDK 30:

```sh
dotnet build experiments/csharp/ExampleCSharpMod/ExampleCSharpMod.csproj -c Release -p:PublishAot=false
dotnet publish experiments/csharp/ExampleCSharpMod/ExampleCSharpMod.csproj -c Release -r linux-bionic-arm64 -p:PublishAot=true
dotnet run --project experiments/csharp/BunnyCsPackager/BunnyCsPackager.csproj -c Release -- \
    experiments/csharp/ExampleCSharpMod/bin/Release/net10.0/mod.dll \
    CAMINHO/DA/BIBLIOTECA/mod.so \
    samples/ExampleCSharpOnlyMod/Assets \
    build/ExampleCSharpMod-v0.1.6.bmod
```

O workflow `build-bunny-csharp-authoring.yml` faz esses passos automaticamente e
publica o arquivo pronto. Use o APK C# 0.1.10.5 já instalado; não precisa de APK novo.

Neste primeiro protótipo o empacotador suporta armas ranged, projéteis simples,
NPCs, itens de invocação e receitas de madeira. Buffs, acessórios, outros tipos de
itens, fontes nativas de NPC e multiplayer ainda requerem mais APIs.

Não foi usado código do tModLoader no runtime. Os registros são gerados a partir
dos exemplos do próprio Bunny. A validação de compilação não substitui o teste
no Android, sobretudo dos ataques do slime.
