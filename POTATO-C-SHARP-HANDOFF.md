> **DOCUMENTO HISTÓRICO.** Esta explicação registra a integração preparada sobre o Bunny original de 15/10/2026. O pacote de revisão atual está em [docs/JONATA-REVISAO.md](docs/JONATA-REVISAO.md), baseado no snapshot C# de 26/10/2026. Não tratar versões e instruções de importação abaixo como estado atual do repositório oficial.

---

# Integração C# no Bunny Loader — fontes e funcionamento

Integrei ao código-fonte original do Bunny Loader o sistema C# que desenvolvi para o Terraria Mobile. Esta cópia contém **o projeto Android completo com as alterações já aplicadas**, incluindo o MonoVM, Roslyn, os hooks nativos, a ponte JNI e os exemplos. Mantive os componentes QuickJS e os exemplos JavaScript originais, sem transformar o C# numa camada de callbacks JS.

Use esta entrega junto com:
- [README principal](README.md), onde apresento o projeto;
- [passo a passo para colocar a fonte no seu GitHub](IMPORTAR-NO-GITHUB-DO-POTATO.md);
- [README original que preservei](README-BUNNY-ORIGINAL.md);
- [inventário exato de arquivos e hashes](docs/POTATO-UPSTREAM-FILE-MAP.json).

## 1. Base de código e versões

Eu usei sua `main` de **15 de outubro de 2026**, commit `a370a664aa3b808c87e4e4f570b27504d20df723`. Essa ainda era a ponta da `main` que consultei em 18/10/2026.

A entrega está identificada como **Bunny Loader C# 1.0**, mantendo o código Android e C# do desenvolvimento mais recente. Atualizei também o **ExampleMod C#** com armas, munições, áudio personalizado e novos contratos do SDK, preservando os exemplos anteriores. O código da linha de desenvolvimento foi verificado pelo CI, mas isso não constitui validação de jogabilidade no Android físico. A documentação interna conserva o histórico de commits e testes para rastreabilidade.

**O que considero validado:** build e assinatura do APK pelo CI, entrega privada, várias suítes independentes de testes de C#, C++ e integração, além de resultados anteriores de jogabilidade que fui validando durante o desenvolvimento.

**O que ainda precisa de validação:** o comportamento dos minions, armas novas, sons personalizados, carregamento automático e preservação de desempenho em Android físico. As verificações automatizadas testam contratos e compilação, mas não substituem uma sessão de gameplay. A integração permanece disponível para revisão e não representa uma homologação final.

## 2. Preservação do Bunny original

Eu comparei **todas as entradas da árvore Git** e restaurei os sete arquivos que faltavam na minha cópia (um recurso visual e dois pequenos mods de teste), conferindo os hashes originais.

A comparação registrada no [JSON de inventário](docs/POTATO-UPSTREAM-FILE-MAP.json) contém o resumo quantitativo atualizado e os arquivos exatos, com blobs `sourceBlob` e `upstreamBlob`. A diferença inclui fontes, documentação, exemplos, recursos e testes; não se limita às classes C#.

Preservei a estrutura Android, launcher, exemplos JavaScript e recursos do projeto. **O QuickJS-ng v0.17.0 agora também está incluído integralmente na fonte** (`app/src/main/cpp/third_party/quickjs/`), com MIT LICENSE, e o CMake habilita automaticamente o engine real sem precisar clonar dependências. As atualizações que eu já tinha incorporado em branches anteriores incluem modo Jornada com `ResearchUnlockCount`, tratamento de bolsas `PreHardmodeLikeBossBag` e correção de nomes de linhas de tooltip. **Os arquivos compartilhados que são diferentes não significam necessariamente que ficaram desatualizados**; são, em grande parte, pontos nos quais o C# se conecta ao núcleo original.

Esta é uma cópia de fontes compatível **estruturalmente com o commit comparado**, não uma promessa de que todo hook e ABI foi testado contra cada aparelho ou futura atualização sua.

## 3. Arquitetura da integração C#

Eu preservei os caminhos da sua raiz, sem criar um projeto paralelo:

| Arquivo ou pasta no seu projeto | O que acrescentei |
| --- | --- |
| `experiments/csharp/BunnyCsSdk/` | API pública C#, classes de itens, entidades, hooks, eventos e efeitos |
| `experiments/csharp/BunnySourceSdk/` | Assembly do SDK usado na compilação de mods-fonte |
| `experiments/csharp/BunnySourceCompiler/` | Roslyn, cache, verificação de código e descritores |
| `experiments/csharp/BunnyManagedProbe/` | Entrada gerenciada do worker |
| `app/src/main/cpp/experiments/csharp/` | Runtime MonoVM, políticas nativas, hooks do Terraria, filas e integração |
| `app/src/main/kotlin/dev/bunnyloader/csharp/ManagedProbeService.kt` | Serviço Android em processo separado |
| `app/src/main/java/net/dot/android/crypto/DotnetProxyTrustManager.java` | Suporte Java usado pelo Mono |
| `samples/ExampleModCSharp/` | Exemplo editável para criar e testar mods reais em C# |
| `tools/stage_mono_runtime.py` | Preparação das DLLs e bibliotecas do Mono para o APK |

São **dois** diretórios `experiments/csharp/`: um na raiz para os projetos .NET e outro em `app/src/main/cpp/` para o host C++. Não dá para trocar um pelo outro.

### Inicialização, compilação e carregamento

1. Na descoberta de mods, eu mantenho `mods::loadAll(...)` do seu QuickJS e acrescento `csharpmods::loadAll()` para o C#; o arquivo central é `app/src/main/cpp/boot/Probe.cpp`.
2. O `csharp::startAutoSourceRuntime()` prepara a fonte, cache, descritores e registro C#. Também instalo o gate `installCSharpWorldEntryGate()`.
3. Em `QuickStart.cpp`, o fluxo consulta `preWorldCSharpContentReady()` para evitar entrar no mundo antes da preparação necessária.
4. O `ManagedProbeService.kt` roda em `:mono_probe`; `jni_entry.cpp` entrega `appContext` à ponte `bindMonoWorkerContext(...)` e precisa concordar com os campos da configuração Kotlin.
5. Roslyn compila os `.cs`; os callbacks e os descritores compatíveis seguem para o MonoVM/host nativo. Não preciso gerar uma `libmod.so` por mod-fonte C#, nem rotear seus callbacks pela engine QuickJS.

Mantive verificações de tipo, largura, offset e assinatura nas ligações IL2CPP. O worker é isolado para reduzir o risco de comprometer o processo principal do Terraria; filas limitadas, descarte de eventos atrasados e os contratos síncronos/assíncronos continuam importantes para a estabilidade.

### Onde mexi nos arquivos originais

Eu já fiz a integração nesses arquivos nesta cópia; estou listando para facilitar sua revisão, **não** para você repetir manualmente todos esses passos:

| Arquivo | Mudança e cuidado |
| --- | --- |
| `app/src/main/cpp/CMakeLists.txt` | Mantive `libbunny.so` e acrescentei `libbunnymonoprobe.so`; `MonoWorkerJni.cpp` não pode ser compilado nos dois targets |
| `app/src/main/cpp/boot/Probe.cpp` e `QuickStart.cpp` | Loader C# junto do loader JS e preparação antes do mundo |
| `app/src/main/cpp/boot/jni_entry.cpp` e `core/Config.h` | `appContext`, `managedProbePath`, `managedCheckpointPath` |
| `app/src/main/cpp/mods/ModLoader.cpp` | Descoberta de mods C# sem substituir os mods JS |
| `app/src/main/cpp/content/`, `il2cpp/Api.*` | Conteúdo e persistência nativos, políticas, registries e validações de ABI |
| `app/src/main/AndroidManifest.xml` | Serviço separado do MonoVM |
| `app/src/main/kotlin/dev/bunnyloader/` | Inicialização do jogo, importação de mods, logs e ponte nativa |
| `app/src/main/cpp/graphics/` e `script/` | Pontos de efeitos e integração; mantive o motor JS |

Também deixei no repositório os exemplos e as ferramentas de testes que uso. Não considero que todo exemplo do tModLoader foi implementado ou que multiplayer e hot-unload sejam universais: a matriz de cobertura do SDK é parcial.

### Atualização dos exemplos C# e do áudio nativo

Atualizei nesta cópia os fontes do **ExampleMod C#** para a revisão mais recente disponível no desenvolvimento. O pacote de exemplos mantém a estrutura editável, agora com novas armas de curto e longo alcance, munições, projéteis e o áudio personalizado do ExampleGun. O `BunnySoundStyle`, `CustomUseSound`, defaults nativos e holdouts são implementados no SDK e no host C++/Android, não por uma dependência externa de JavaScript.

Mantive os PNGs e o arquivo OGG correspondentes nas pastas do mod e os testes de assets. O carregamento nativo de sons inclui validação de caminho, registro idempotente e controle de instâncias. Novas definições de áudio podem exigir reinício da sessão para preservar referências de itens já registrados.

O port integral do ExampleMod ainda está **em desenvolvimento**. Não estou apresentando os exemplos novos como comprovadamente funcionais em jogo ou declarando que todas as APIs do exemplo de PC já estão disponíveis.

## 4. Compilação e dependências externas

Os arquivos-fonte desta entrega já foram alterados para o C#. Os binários externos não vêm no GitHub público.

| Fonte ou dependência | Saída esperada |
| --- | --- |
| `experiments/csharp/BunnySourceSdk/` | `app/src/main/assets/managed/BunnySourceSdk.dll` |
| `experiments/csharp/BunnySourceCompiler/` | `BunnySourceCompiler.dll` + `Microsoft.CodeAnalysis.dll` + `Microsoft.CodeAnalysis.CSharp.dll` no mesmo diretório |
| `experiments/csharp/BunnyManagedProbe/` | `app/src/main/assets/managed/BunnyManagedProbe.dll` |
| Framework .NET Mono Android ARM64 | DLLs como `System.Runtime.dll` e `System.Private.CoreLib.dll` |
| Manifest gerado das DLLs | `app/src/main/assets/managed/framework.sha256`, cada linha como nome, TAB, hash SHA-256 |
| CMake | `lib/arm64-v8a/libbunny.so` + `lib/arm64-v8a/libbunnymonoprobe.so` no APK |
| Mono runtime ARM64 com sinais compatíveis | `lib/arm64-v8a/libmonosgen-2.0.so` no APK |

**MonoVM e Roslyn já incorporados:** incluí todas as DLLs necessárias do framework Mono Android 8.0.23, SDK, compilador e Roslyn diretamente em [`app/src/main/assets/managed/`](app/src/main/assets/managed/), com o arquivo `framework.sha256`. As bibliotecas nativas correspondentes e o Mono ARM64 modificado estão instalados em [`app/src/main/jniLibs/arm64-v8a/`](app/src/main/jniLibs/arm64-v8a/). A preparação automática foi [executada e aprovada pelo GitHub Actions](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39185587613). **Nenhuma instalação separada do Mono ou compilação prévia de DLLs é necessária** para abrir o projeto e fazer o Gradle consumir o runtime já presente. Também compartilhei [o patch-fonte](tools/mono/patch_mono_android_signals.py), [a biblioteca de referência](tools/mono/prebuilt/arm64-v8a/libmonosgen-2.0.so) e [os scripts de recompilação](tools/mono/README.md) para manutenções futuras. A integração atual ainda precisa de testes em aparelho para minions, áudio e novas armas.

As alterações que fiz no **host C++/MonoVM, SDK, compilador, bootstrap, patch ARM64 e biblioteca Mono recompilada** estão compartilhadas neste Git. Os binários de Terraria (`libil2cpp.so`, `libunity.so`), assets do jogo, classes Unity proprietárias e chaves de assinatura são componentes de terceiros ou credenciais, não modificações nossas; continuam fora do controle de versão. O [workflow histórico de compilação do APK C#](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/blob/handoff/potato-bunny-csharp-20261018/.github/workflows/build-bunny-csharp-source-loader.yml) continua disponível para consulta, mas sua seção de build do Terraria usa inputs privados. Para reproduzir **o Mono modificado**, o novo script aberto nesta fonte não precisa desses inputs.

A árvore já usa `applicationId="com.bunnyloader"`, igual ao projeto original. O nome da versão ainda identifica o checkpoint C#, e a assinatura oficial do aplicativo não está no Git. Antes de publicar uma release oficial, é necessário validar o carregamento e a compatibilidade com os dados do app.

## 5. Como importar a fonte completa

A cópia do Android está **direto na raiz** da branch `entrega/potato-bunny-original-com-csharp-20261018`, com as alterações já encaixadas. O [guia de importação](IMPORTAR-NO-GITHUB-DO-POTATO.md) e o script incluso permitem copiar os arquivos rastreados para uma branch do repositório original, preservando o histórico e sem substituir a `main`.

Nenhum commit ou push foi realizado no repositório oficial do Bunny. O script de importação copia os fontes e não força merges, não limpa diretórios, não altera a assinatura, não gera binários e não publica APKs.

Se quiser manter as duas linguagens oficialmente, eu seguiria essa sequência de verificação após importar: abrir launcher; carregar mods JS; carregar mod C#; carregar JS e C# juntos; conferir preparação antes de entrar no mundo; receitas, acessórios, buffs, NPCs, projéteis, save/reentrada; e, por último, invocação de minions, cancelamento de buff, reinvocação, respawn e reprodução dos sons personalizados. Mediria FPS e comportamento no Android ARM64, sem inferir desempenho só pelos testes do CI.

**Resumindo:** entreguei minha implementação de C# já acoplada ao seu projeto original, na estrutura que você usa. Mantive suas fontes JS e documentação original, registrei todas as diferenças com hashes e deixei claro o que foi testado e o que precisa de validação. A integração utiliza a API e o runtime C# próprios do Bunny, sem exigir a instalação de outras plataformas de mods.

## 6. Créditos

A autoria de **Jonata Oliveira** permanece associada ao Bunny Loader original e os termos do [LICENSE.md](LICENSE.md), [LICENSING.md](LICENSING.md), [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) e [TRADEMARKS.md](TRADEMARKS.md) foram preservados. O suporte C# é a extensão que desenvolvi. O patch e o script de compilação do Mono estão incluídos, sem redistribuir recursos proprietários do Terraria ou Unity.

*Mantovani*
