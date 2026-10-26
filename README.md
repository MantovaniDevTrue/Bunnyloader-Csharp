<p align="center"><img src="_icons/launcher/titulo.png" alt="Bunny Loader"></p>

# Bunny Loader — suporte C#

Integração C# para o [Bunny Loader original, de Jonata Oliveira](https://github.com/JonataOliveiraa/Bunny-Loader), desenvolvida por Mantovani para Terraria Android.

**Esta branch é a cópia de revisão para Jonata, não uma release oficial.** O projeto Android está completo nesta árvore de fontes, com MonoVM, Roslyn, SDK C#, JNI, C++/IL2CPP e exemplos. O Bunny original e o suporte a JavaScript foram preservados.

## Para começar

- **[Guia de revisão para Jonata](docs/JONATA-REVISAO.md)** — arquitetura, estado real e pontos onde preciso de ajuda.
- **[Mapa das PRs relevantes](docs/PR-CSHARP.md)** — o que está na base, o que permanece em desenvolvimento e o que é apenas experimental.
- **[README original do Bunny](README-BUNNY-ORIGINAL.md)** — documentação do projeto de Jonata.

Para baixar apenas esta cópia:

```bash
git clone --single-branch --branch handoff/jonata-csharp-20261026 \
  https://github.com/MantovaniDevTrue/Bunnyloader-Csharp.git
```

Abra o projeto Android da raiz no Android Studio. Não é necessário copiar classes separadamente para conhecer a implementação. Os componentes gerenciados e nativos de suporte estão nos caminhos usados pelo projeto, mas binários e dados proprietários do Terraria e a assinatura oficial não fazem parte desta entrega.

## Estado do projeto

| Área | Estado para revisão |
| --- | --- |
| MonoVM e Roslyn | Base implementada e exercitada em testes de build e no Android |
| SDK, compilação de mods-fonte e ponte C++/JNI | Implementados; há integrações nativas que ainda evoluem |
| QuickJS e mods JavaScript | Mantidos no projeto |
| Loaders e classes específicas do Terraria | Em desenvolvimento, com diferentes graus de cobertura |
| ExampleModBunny traduzido para C# | Port parcial; não declarar paridade completa |
| APK oficial / merge no Bunny de Jonata | Não realizado |

Os testes anteriores de MonoVM e Roslyn não precisam ser confundidos com a conclusão de cada loader. Um teste de CI aprovado comprova aquela compilação e seus testes, não todas as funcionalidades em gameplay. Esta cópia foi separada da branch de QA no commit `6d96539f749b2ac8736c08bc38244cf88ec6cda0`, em 26/10/2026, para uma revisão reproduzível; alterações posteriores da branch de QA não entram aqui automaticamente.

## Onde fica o código

| Caminho | Finalidade |
| --- | --- |
| [`experiments/csharp/BunnyCsSdk/`](experiments/csharp/BunnyCsSdk/) | API C# para autores de mods |
| [`experiments/csharp/BunnySourceSdk/`](experiments/csharp/BunnySourceSdk/) | SDK utilizado na compilação de fontes |
| [`experiments/csharp/BunnySourceCompiler/`](experiments/csharp/BunnySourceCompiler/) | Compilador Roslyn, cache e contratos |
| [`experiments/csharp/BunnyManagedProbe/`](experiments/csharp/BunnyManagedProbe/) | Componentes gerenciados do worker |
| [`app/src/main/cpp/experiments/csharp/`](app/src/main/cpp/experiments/csharp/) | MonoVM, host nativo, registros e hooks IL2CPP |
| [`app/src/main/kotlin/dev/bunnyloader/csharp/`](app/src/main/kotlin/dev/bunnyloader/csharp/) | Serviço Android/integração Kotlin |
| [`samples/ExampleModCSharp/`](samples/ExampleModCSharp/) | Exemplos editáveis em C# |
| [`tests/`](tests/) e [`.github/workflows/`](.github/workflows/) | Testes e CI |

## Como colaborar com segurança

O desenvolvimento dos loaders continua na branch [`qa/csharp-world-tree-gore-apk-20261024`](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/tree/qa/csharp-world-tree-gore-apk-20261024). Esta cópia é um ponto fixo para leitura e comparação. Não envie um merge direto desta branch para a `main` do Bunny original: a integração anterior foi comparada com o commit `a370a664` (15/10/2026), enquanto a `main` de Jonata já havia avançado para `c028c326` quando esta entrega foi preparada. É necessário revisar os arquivos compartilhados e resolver as diferenças antes de qualquer importação.

Os guias `POTATO-C-SHARP-HANDOFF.md` e `IMPORTAR-NO-GITHUB-DO-POTATO.md` ficam no histórico, mas **não** devem ser usados como procedimento atual de merge sem essa revisão.

## Créditos e licenças

Bunny Loader original: **Jonata Oliveira**. Integração C#: **Mantovani**. Créditos, licenças e avisos de terceiros permanecem em [LICENSING.md](LICENSING.md), [LICENSE.md](LICENSE.md) e [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Esta branch não altera a autoria nem representa aprovação oficial de Jonata.
