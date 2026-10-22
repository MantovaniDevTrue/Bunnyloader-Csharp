# ContentLoader C# — núcleo de loaders do tModLoader

Execute na raiz do repositório:

```sh
dotnet run --project Split/Bunny-Loader/samples/ContentLoaderCSharp -c Release
```

Resultado esperado:

```text
ConfiguredLife=240
LegacyAliasSameInstance=True
ManagedDefinitionId=4
```

[ExampleDefinitions.cs](ExampleDefinitions.cs) explica dentro do código categoria,
`Loader<T>`, autoload, `Load`, consulta em `SetStaticDefaults`, aliases, singleton e
dependências opcionais. [Program.cs](Program.cs) mostra bootstrap e unload em `finally`.

Este executável demonstra dados gerenciados. Não é um mod jogável: não cria um NPC,
não atribui IDs do Terraria e não depende de JavaScript. O consumidor do host precisa
chamar o bootstrap e o teardown; não instalar esta pasta como `.bmod` nem gerar APK.
[Contrato e limites de integração](../../../../docs/csharp-content-loader.md).
