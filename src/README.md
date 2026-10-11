# Código-fonte C# — pasta de acesso

O código da extensão C# encontra-se na **[branch `csharp-development`](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/tree/csharp-development)**, baseada na .60. Os atalhos nessa branch apontam para as fontes canônicas sem duplicá-las.

| Parte | Fonte |
| --- | --- |
| Host MonoVM e hooks C++ | [`src/csharp-host`](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/tree/csharp-development/src/csharp-host) |
| Compilador Roslyn | [`src/roslyn-compiler`](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/tree/csharp-development/src/roslyn-compiler) |
| API/ponte com o Bunny | [`src/csharp-api`](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/tree/csharp-development/src/csharp-api) |
| Exemplo C# 0.1.7 | [`src/examplemod-csharp`](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/tree/csharp-development/src/examplemod-csharp) |
| Aplicativo Android | [`src/android-app`](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/tree/csharp-development/src/android-app) |

Para construir o projeto, os caminhos canônicos continuam sob `Split/Bunny-Loader/`. Essa decisão evita regressões nos builds históricos.
