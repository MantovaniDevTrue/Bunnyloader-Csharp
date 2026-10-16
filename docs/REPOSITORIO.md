# Mapa do repositório

`Split/Bunny-Loader/` é a fonte canônica. Os caminhos históricos foram mantidos
porque CMake, Gradle, projetos .NET e workflows os utilizam. `src/`, `examples/`
e `tests/` na raiz oferecem navegação; não são outras implementações.

| Área | Local canônico | Regra de edição |
| --- | --- | --- |
| App Android/Kotlin | [app](../Split/Bunny-Loader/app/) | Launcher, importação e processos |
| Native C#/MonoVM | [experiments/csharp C++](../Split/Bunny-Loader/app/src/main/cpp/experiments/csharp/) | Worker, filas, GC, hooks e policies |
| Conteúdo compartilhado | [content C++](../Split/Bunny-Loader/app/src/main/cpp/content/) | Registros e serviços usados por C# e JS |
| Classes C# | [BunnyCsSdk](../Split/Bunny-Loader/experiments/csharp/BunnyCsSdk/) | O csproj BunnySourceSdk define a superfície compilada |
| Projeto do SDK fonte | [BunnySourceSdk](../Split/Bunny-Loader/experiments/csharp/BunnySourceSdk/) | Fonte de verdade para mods-fonte; não confundir com projetos históricos NativeAOT |
| Compilador | [BunnySourceCompiler](../Split/Bunny-Loader/experiments/csharp/BunnySourceCompiler/) | Roslyn, caches e descritores |
| Exemplos | [samples](../Split/Bunny-Loader/samples/) | Cada mod independente mantém manifest/content e UID próprios |
| Testes | [tests canônicos](../Split/Bunny-Loader/tests/) | Testar o módulo de produção com doubles quando o jogo não está disponível |
| Ferramentas | [tools](../Split/Bunny-Loader/tools/) | Gerador, empacotador, referência e auditorias |
| Documentação C# | [docs](README.md) | Guias atuais aqui; investigações em history/ |
| Referências geradas | [docs/generated](../Split/Bunny-Loader/docs/generated/) | Regenerar com a ferramenta correspondente; não editar números manualmente |
| Documentação do loader base | [docs do Bunny](../Split/Bunny-Loader/docs/README.md) | Contratos JS e histórico upstream identificados |

## Regras de organização

1. Edite a fonte canônica. Não crie espelhos do SDK, compilador ou ponte JS.
2. Coloque novas responsabilidades em seu módulo e conserve contratos entre threads.
3. Referencie a documentação atual pelo índice, não pelo relatório de uma versão antiga.
4. Relatórios por versão ficam em [history/](history/README.md), com a evidência original.
5. Atualize checkpoint, changelog, exemplos e referência gerada quando mudar a API.
6. Outputs, runtime proprietário, saves/logs e assinatura ficam fora do Git público.

`src/csharp-api` aponta para as classes C#, não para `script/api`.
A antiga cópia de `src/javascript-bridge` foi substituída por um índice para a
ponte original, evitando duas versões editáveis dos mesmos arquivos.

## Branches e PRs

A consolidação atual reúne a cadeia de PRs C# até #34. `main` é a entrada de
código integrada; o checkpoint do APK é registrado separadamente em
[STATUS.md](STATUS.md). Uma atualização só de documentação não gera outro APK.
PRs posteriores devem partir da `main` integrada ou declarar sua dependência.
Não apague branches/checkpoints necessários para reproduzir testes antigos.

[Contribuição](../CONTRIBUTING.md) · [Build/CI](BUILD.md) · [Testes](../tests/README.md)
