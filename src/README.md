# Fontes do C# — índice sem duplicação

A árvore canônica do projeto Android foi **mantida no caminho original** `Split/Bunny-Loader/`. Alterá-lo quebraria scripts Gradle, CMake e GitHub Actions já usados pelas branches de teste. Os itens desta pasta são **links simbólicos Git** para o mesmo código: editar o destino canônico, nunca manter uma cópia paralela.

| Atalho | Conteúdo | Caminho canônico |
| --- | --- | --- |
| [`csharp-host/`](csharp-host/) | Runtime MonoVM, loader, hooks, conteúdo e ponte gerenciada | `Split/Bunny-Loader/app/src/main/cpp/experiments/csharp/` |
| [`roslyn-compiler/`](roslyn-compiler/) | Compilador C# e checagens de assembly | `Split/Bunny-Loader/experiments/csharp/BunnySourceCompiler/` |
| [`csharp-api/`](csharp-api/) | Exposição de APIs do Bunny e testes de integração | `Split/Bunny-Loader/app/src/main/cpp/script/api/` |
| [`examplemod-csharp/`](examplemod-csharp/) | Código C# real do ExampleModCSharp 0.1.7 | `Split/Bunny-Loader/samples/ExampleModCSharp/Playable/content/` |
| [`android-app/`](android-app/) | Fontes Android (Kotlin, Java, C++, recursos) | `Split/Bunny-Loader/app/src/main/` |

O projeto continua tendo código **C++/JNI/Android**, **C# gerenciado** e suporte **QuickJS** do Bunny Loader base. Não afirmar que tudo foi migrado para C#.

Veja [ARQUITETURA](../docs/ARQUITETURA.md) e [STATUS](../docs/STATUS.md).
