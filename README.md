<p align="center"><img src="_icons/launcher/titulo.png" alt="Bunny Loader"></p>

# Bunny Loader com suporte C#

**Código-fonte integrado — mantido por Mantovani, a partir do Bunny Loader de Jonata Oliveira.**

Integrei ao [Bunny Loader original](https://github.com/JonataOliveiraa/Bunny-Loader) a implementação C# desenvolvida para Terraria Mobile. Esta árvore contém o **projeto Android completo**, com as alterações já aplicadas ao núcleo: Roslyn, MonoVM, serviços Android, C++/IL2CPP, exemplos e QuickJS funcionando como motores independentes. Não é necessário reconstruir o port copiando classes isoladas.

A entrega permanece em branch própria, para que o projeto original continue intacto durante a revisão. Preservei [o README original](README-BUNNY-ORIGINAL.md), com as instruções de JavaScript, documentação do núcleo e créditos.

## Importação da fonte completa

**[Abrir o passo a passo de importação](IMPORTAR-NO-GITHUB-DO-POTATO.md)** · **[Ler minha explicação técnica completa](POTATO-C-SHARP-HANDOFF.md)** · **[Ver todas as diferenças com seu original](docs/POTATO-UPSTREAM-FILE-MAP.json)**

O projeto Android está **direto na raiz**, nos mesmos caminhos do Bunny original: `app/`, `experiments/csharp/`, `samples/` e `tools/`. Basta abrir a fonte no Android Studio. O código C# já está integrado nos arquivos de inicialização, build e conteúdo necessários.

Para manter o histórico do repositório original, a importação pode ser feita numa branch derivada do commit de base. Incluí um script de cópia com simulação e validação, sem `force push`, sem substituir `.git` e sem alterar a `main`.

## O que integrei

| Parte do projeto | O que eu acrescentei |
| --- | --- |
| Linguagem C# | SDK próprio, com classes e exemplos independentes de tModLoader |
| Compilação no Android | Roslyn compilando fontes `.cs` dos mods, com cache e carregamento automático |
| Execução gerenciada | MonoVM isolado com worker Android e integração nativa ao Terraria IL2CPP |
| Núcleo do loader | Hooks nativos, políticas, filas, lifecycle, registros e persistência necessários ao C# |
| Conteúdo | Itens, armas, receitas, acessórios, armaduras, buffs, NPCs, projéteis, hooks de combate e recursos visuais cobertos pelo SDK atual |
| Exemplos | `samples/ExampleModCSharp/` e outros mods C# de teste; mantive `samples/ExampleMod/` em JavaScript |
| Motor original | Mantive QuickJS e as classes JavaScript; os callbacks C# do caminho principal não dependem de adaptadores JS |

### Onde está cada implementação

- **C# para autores de mods:** [experiments/csharp/BunnyCsSdk/](experiments/csharp/BunnyCsSdk/).
- **Projeto do SDK/assemblies:** [experiments/csharp/BunnySourceSdk/](experiments/csharp/BunnySourceSdk/).
- **Roslyn e compilador:** [experiments/csharp/BunnySourceCompiler/](experiments/csharp/BunnySourceCompiler/).
- **Host C++ e MonoVM:** [app/src/main/cpp/experiments/csharp/](app/src/main/cpp/experiments/csharp/).
- **Serviço Android isolado:** [ManagedProbeService.kt](app/src/main/kotlin/dev/bunnyloader/csharp/ManagedProbeService.kt).
- **Build nativo:** [CMakeLists.txt](app/src/main/cpp/CMakeLists.txt).
- **Exemplo completo:** [samples/ExampleModCSharp/](samples/ExampleModCSharp/).

## Base usada e grau de validação

Eu comparei esta cópia com a `main` do seu repositório no commit **[a370a664](https://github.com/JonataOliveiraa/Bunny-Loader/commit/a370a664aa3b808c87e4e4f570b27504d20df723)**, de **15/10/2026**. Todos os arquivos rastreados nessa versão estão presentes. Os arquivos diferentes e os que acrescentei estão no [inventário](docs/POTATO-UPSTREAM-FILE-MAP.json), identificados pelo hash de cada conteúdo.

O ponto de partida C# é o **Bunny Loader C# `0.1.10.16.90.29.95.2`**, com **ExampleMod C# `0.1.54`**. Preservei os mesmos 753 arquivos de código de produção do commit que gerou a [build Android do CI](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39171952411). O build e as verificações automatizadas passaram, mas **a jogabilidade da correção de summons da `.95.2` ainda precisa ser validada no aparelho**. Eu não vou afirmar que esse último ajuste foi testado em jogo quando o registro disponível comprova apenas CI e testes controlados.

### Antes de compilar

Incluí **a biblioteca MonoVM ARM64 modificada** em [`tools/mono/prebuilt/arm64-v8a/`](tools/mono/prebuilt/arm64-v8a/), com SHA-256, licença e procedência. Também compartilhei [o patch de sinais](tools/mono/patch_mono_android_signals.py), [o script de recompilação](tools/mono/build_patched_mono_arm64.sh) a partir do .NET 8.0.23 e [a instalação com validação](tools/mono/install_patched_mono_arm64.sh). **Após preparar o framework Mono oficial, execute o instalador por último**, para não sobrescrever a biblioteca corrigida com a versão genérica. Veja [instruções completas](tools/mono/README.md) e [guia técnico](POTATO-C-SHARP-HANDOFF.md). Arquivos proprietários do Terraria/Unity e chaves de assinatura não fazem parte desta fonte.

**Identidade de aplicativo:** a fonte mantém o `applicationId = "com.bunnyloader.csharptest"` usado nos testes Android do C#. A adaptação para o ID e a assinatura oficiais do Bunny precisa de uma nova compilação e verificação dos caminhos que dependem do pacote.

## Estrutura

~~~text
Bunny-Loader/
├── app/
│   ├── build.gradle.kts
│   └── src/main/
│       ├── cpp/experiments/csharp/
│       ├── kotlin/dev/bunnyloader/csharp/
│       ├── java/net/dot/android/crypto/
│       ├── AndroidManifest.xml
│       └── ...
├── experiments/csharp/
│   ├── BunnyCsSdk/
│   ├── BunnySourceSdk/
│   ├── BunnySourceCompiler/
│   └── BunnyManagedProbe/
├── samples/
│   ├── ExampleMod/
│   └── ExampleModCSharp/
├── docs/
├── tools/
├── POTATO-C-SHARP-HANDOFF.md
└── IMPORTAR-NO-GITHUB-DO-POTATO.md
~~~

Eu mantive os arquivos e as licenças do Bunny original. **Jonata Oliveira é o autor do Bunny Loader original; a integração C# é meu trabalho de extensão.** Respeito a licença PolyForm Noncommercial e os avisos de terceiros do projeto. Nada aqui implica uma release oficial ou substitui sua validação como mantenedor.

*Extensão C# por Mantovani · Bunny Loader original por Jonata Oliveira.*
