<p align="center"><img src="_icons/launcher/titulo.png" alt="Bunny Loader"></p>

# Bunny Loader — código original com suporte C# integrado

**Fonte completa para o Potato, preparada por Mantovani.**

Potato, preparei esta cópia a partir do [seu Bunny Loader original](https://github.com/JonataOliveiraa/Bunny-Loader) e acrescentei todo o sistema C# que venho desenvolvendo e testando no Terraria Mobile. **Não é um SDK solto, um patch incompleto ou um aplicativo novo criado do zero.** O projeto Android inteiro já está organizado nos mesmos caminhos do seu repositório, com QuickJS e C# coexistindo.

Deixei esta versão numa branch separada para você importar para o seu Git sem afetar sua versão oficial antes dos seus testes. Também preservei [o README original](README-BUNNY-ORIGINAL.md) com as instruções de JavaScript, documentação do núcleo e créditos.

## Para colocar no seu GitHub

**[Abrir o passo a passo de importação](IMPORTAR-NO-GITHUB-DO-POTATO.md)** · **[Ler minha explicação técnica completa](POTATO-C-SHARP-HANDOFF.md)** · **[Ver todas as diferenças com seu original](docs/POTATO-UPSTREAM-FILE-MAP.json)**

O projeto Android está **direto na raiz**: abra esta pasta no Android Studio. Os arquivos já estão nos diretórios finais, como `app/`, `experiments/csharp/`, `samples/` e `tools/`. Você **não precisa** procurar os arquivos C# e juntá-los manualmente com seu Bunny original.

Para conservar seu histórico de commits, recomendo copiar este snapshot para uma branch nova criada a partir do seu repositório, revisar as mudanças e só então fazer o merge. No guia acima deixei os comandos exatos. Não precisa dar force push nem substituir a sua `main`.

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

Eu incluí os fontes, não redistribuí os binários proprietários do Terraria ou da Unity. O build completo também requer o runtime Mono ARM64 com a **correção de sinais** usada na nossa linha C#, os assemblies gerados, o QuickJS e os inputs autorizados do jogo. Nossa receita de CI original usa dependências privadas, portanto **baixar este GitHub sozinho não garante um APK jogável**. Detalhei o empacotamento e as dependências no [guia técnico](POTATO-C-SHARP-HANDOFF.md).

**Atenção à identidade do aplicativo:** mantive `applicationId = "com.bunnyloader.csharptest"` e a versão de teste na cópia para não confundir este build experimental com o Bunny oficial. Você pode usar a identidade e a assinatura do seu aplicativo quando decidir integrar e validar a versão oficial.

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

*Mantovani — entrega de código-fonte para integração no Bunny Loader original.*
