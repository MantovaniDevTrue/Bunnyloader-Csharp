# Código-fonte C# — pastas reais no GitHub

Esta pasta contém **árvores Git navegáveis**, não links simbólicos. Clique em cada diretório para abrir os arquivos diretamente no GitHub.

| Abrir | Conteúdo |
| --- | --- |
| [roslyn-compiler/](roslyn-compiler/) | Compilador Roslyn C# (Compiler.cs e outros) |
| [csharp-api/](csharp-api/) | SDK C# (BunnySourceScriptApi.cs e classes de conteúdo) |
| [csharp-host/](csharp-host/) | Ponte nativa C++/JNI/MonoVM, callbacks e efeitos |
| [examplemod-csharp/](examplemod-csharp/) | ExampleModCSharp completo, fontes .cs, manifests e testes |
| [android-app/](android-app/) | Projeto Android do Bunny Loader |
| [javascript-bridge/](javascript-bridge/) | API JavaScript e integração entre os runtimes |

**Os fontes canônicos para editar e compilar o Bunny Loader continuam em [Split/Bunny-Loader/](../Split/Bunny-Loader/).** A pasta src/ é um **espelho de navegação** feito reutilizando árvores Git sem duplicar blobs. O espelho pode precisar ser atualizado quando mudarem os arquivos canônicos.

O compilador e o SDK são código C# verdadeiro. O host MonoVM e os hooks de Terraria são C++ nativo.

- [Catálogo C# gerado](../Split/Bunny-Loader/docs/generated/csharp_api_catalog.md)
- [Exemplos C# reais](../Split/Bunny-Loader/docs/generated/csharp_api_examples.md)
- [Bunny Loader original do Jonata](https://github.com/JonataOliveiraa/Bunny-Loader)

A .86 foi validada no Android com 473 callbacks V4, 87 empurrões visíveis, debuffs funcionais e 60 FPS. O catálogo .87 é ferramenta de desenvolvimento, não outra APK.
