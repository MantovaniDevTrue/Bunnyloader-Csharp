# Código-fonte Bunny Loader C#

**O código está aqui e as pastas abaixo agora abrem de verdade no GitHub.** Foram removidos os atalhos simbólicos que mostravam apenas uma linha.

| Diretório | Arquivos reais |
| --- | --- |
| [roslyn-compiler](roslyn-compiler/) | Compilador Roslyn C#: Compiler.cs, PortableSha256.cs e demais fontes |
| [csharp-api](csharp-api/) | SDK de modding C#: BunnySourceScriptApi.cs, métodos de NPCs, projéteis e conteúdo |
| [csharp-host](csharp-host/) | Runtime MonoVM e ponte JNI/IL2CPP em C++ |
| [examplemod-csharp](examplemod-csharp/) | ExampleMod C# completo: fontes .cs, manifest, testes e documentação |
| [android-app](android-app/) | App Android do Bunny Loader (Kotlin, Java, C++) |
| [javascript-bridge](javascript-bridge/) | Ponte QuickJS, para distinguir JavaScript do C# |

### Origem e atualização

Esta src/ é um **espelho para navegação** da versão experimental .87 (baseada na .86 validada). Foi criada reutilizando os objetos Git das pastas originais, sem duplicar os arquivos em armazenamento. O código **canônico e usado nos builds da versão .86/.87** permanece na [branch de desenvolvimento .87, em Split/Bunny-Loader/](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/tree/csharp-api-discovery-catalog-v87-experiment/Split/Bunny-Loader).

Não use a base Android antiga presente na main para compilar a versão .86; builds atuais partem da branch específica. Ao alterar código, edite os caminhos canônicos e atualize este espelho.

- [Catálogo de APIs C#](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/blob/csharp-api-discovery-catalog-v87-experiment/Split/Bunny-Loader/docs/generated/csharp_api_catalog.md)
- [Exemplos C# reais](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/blob/csharp-api-discovery-catalog-v87-experiment/Split/Bunny-Loader/docs/generated/csharp_api_examples.md)
- [Bunny Loader original do Jonata](https://github.com/JonataOliveiraa/Bunny-Loader)
