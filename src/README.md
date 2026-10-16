# Fontes canônicos

Edite os arquivos em `Split/Bunny-Loader/`. Esta pasta contém atalhos e índices,
sem cópias paralelas da implementação.

| Área | Caminho |
| --- | --- |
| Classes públicas C# | [BunnyCsSdk](../Split/Bunny-Loader/experiments/csharp/BunnyCsSdk/) |
| Projeto SDK de mods-fonte | [BunnySourceSdk](../Split/Bunny-Loader/experiments/csharp/BunnySourceSdk/) |
| Roslyn | [BunnySourceCompiler](../Split/Bunny-Loader/experiments/csharp/BunnySourceCompiler/) |
| Worker/ponte nativa | [Host C++](../Split/Bunny-Loader/app/src/main/cpp/experiments/csharp/) |
| Android | [app](../Split/Bunny-Loader/app/) |
| Exemplos | [Catálogo](../examples/README.md) |
| JS do loader original | [script/api](../Split/Bunny-Loader/app/src/main/cpp/script/api/) |

`csharp-api` agora aponta para as classes C#, não para `script/api`.
Os atalhos são conveniência; em checkouts sem suporte a symlinks, use os links
canônicos da tabela. [Mapa completo](../docs/REPOSITORIO.md).
