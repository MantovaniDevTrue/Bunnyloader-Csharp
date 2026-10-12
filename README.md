# Bunny Loader C# — código-fonte e desenvolvimento

Extensão experimental de suporte a mods em **C# para Terraria Mobile**, baseada no [Bunny Loader original de Jonata Oliveira](https://github.com/JonataOliveiraa/Bunny-Loader).

## Abrir o código-fonte

**As pastas são navegáveis diretamente no GitHub (não são links simbólicos):**

| Onde entrar | O que contém |
| --- | --- |
| [src/roslyn-compiler](src/roslyn-compiler/) | Compilador C# Roslyn, incluindo Compiler.cs |
| [src/csharp-api](src/csharp-api/) | SDK C# (BunnySourceScriptApi.cs, BunnyNPC, BunnyProjectile etc.) |
| [src/csharp-host](src/csharp-host/) | Integração MonoVM, Terraria e hooks nativos C++ |
| [src/examplemod-csharp](src/examplemod-csharp/) | Exemplo C# com conteúdo real e testes |
| [src/android-app](src/android-app/) | Projeto Android |
| [src/javascript-bridge](src/javascript-bridge/) | Ponte JavaScript/QuickJS |

**[Índice e detalhes dos fontes](src/README.md)**

## Versão mais recente validada em Android

**Bunny Loader C# .86:** 473 callbacks V4, 87 empurrões confirmados visualmente, debuffs reais e 60 FPS relatados. [Branch da .86](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/tree/csharp-managed-npc-impulse-v86-experiment).

**Descoberta de API `.87`:** catálogo automático de métodos e exemplos compiláveis extraídos dos fontes reais. É ferramenta de desenvolvimento, **não outra APK**. [Branch .87](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/tree/csharp-api-discovery-catalog-v87-experiment).

## Atenção sobre branches e builds

A pasta src/ desta main é um **espelho navegável do estado .87**, formado por árvores Git originais. O código canônico usado para desenvolvimento e compilação da .86/.87 está sob [Split/Bunny-Loader/ na branch .87](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/tree/csharp-api-discovery-catalog-v87-experiment/Split/Bunny-Loader). A base Split/Bunny-Loader/ da main é histórica e pode ser mais antiga: **não compile a .86 usando apenas a main**.

Os snapshots históricos e tags de APK continuam preservados.

## Limitações

Não é um port completo do tModLoader e ainda não executa qualquer mod C# de PC automaticamente. O C# gerenciado usa Roslyn/MonoVM em conjunto com um host C++/Android; QuickJS permanece separado.

Somente testes reais em Android confirmam estabilidade e FPS. O projeto não representa uma versão oficial do Bunny Loader. Direitos de Terraria permanecem com seus respectivos titulares.
