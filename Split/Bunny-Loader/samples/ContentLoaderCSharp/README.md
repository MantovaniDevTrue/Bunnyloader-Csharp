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

O console demonstra embedding explícito. A pasta [Playable](Playable/manifest.json)
é um mod-fonte de dados/serviços gerenciados com entrada comentada
[content/main.cs](Playable/content/main.cs): o launcher descobre os loaders e
executa `Load/Register/SetupContent` automaticamente na thread MonoVM antes de
liberar o mundo. O resumo resolve o alias e calcula Life=240; unload limpa o estado.
O SDKFoundation compila essa entrada com o Roslyn real e verifica o ciclo completo.

O exemplo não cria entidades nem reserva IDs do Terraria. A integração ainda
precisa de QA Android. Não empacotar `.bmod`, não gerar APK nem publicar release.
[Contrato e limites de integração](../../../../docs/csharp-content-loader.md).
