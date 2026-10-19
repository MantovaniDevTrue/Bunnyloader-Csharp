# Meu Bunny Loader C# integrado ao seu código-fonte original

Potato, deixei aqui o projeto Android completo do Bunny Loader com a extensão C# que venho desenvolvendo. Quis entregar uma **cópia do seu código-fonte, com todas as alterações C# já nos caminhos finais**, e não mandar apenas uma pasta para você tentar adaptar. Mantive também o motor QuickJS e os seus exemplos JavaScript.

Use esta entrega junto com:
- [README principal](README.md), onde apresento o projeto;
- [passo a passo para colocar a fonte no seu GitHub](IMPORTAR-NO-GITHUB-DO-POTATO.md);
- [README original que preservei](README-BUNNY-ORIGINAL.md);
- [inventário exato de arquivos e hashes](docs/POTATO-UPSTREAM-FILE-MAP.json).

## 1. Qual versão eu entreguei

Eu usei sua `main` de **15 de outubro de 2026**, commit `a370a664aa3b808c87e4e4f570b27504d20df723`. Essa ainda era a ponta da `main` que consultei em 18/10/2026.

Na parte C#, usei minha linha `0.1.10.16.90.29.95.2-summon-native-lifecycle`, versionCode `272`, com `ExampleMod CSharp 0.1.54`. Os fontes de produção correspondem ao commit `018b477a5a90115f399aed57ff84811c020d7a3f`, usado no [build Android do CI 39171952411](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39171952411). O relatório dessa build está [registrado aqui](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/blob/handoff/potato-bunny-csharp-20261018/docs/validation/csharp-v95-2-ci-2026-10-18.json).

**O que considero validado:** build e assinatura do APK pelo CI, entrega privada, várias suítes independentes de testes de C#, C++ e integração, além de resultados anteriores de jogabilidade que fui validando durante o desenvolvimento.

**O que ainda não considero validado:** a correção da versão `.95.2` para o minion/summon no Android físico. A `.95.1` apresentou falha real ao tentar resolver `Projectile.identity`, um campo ausente na versão Android usada. Corrigi o contrato na `.95.2` e os testes de host/CI passaram, mas ainda preciso de uma rodada nova no aparelho para confirmar o gameplay. Não quis passar isso como perfeito sem essa comprovação.

## 2. O que preservei do seu Bunny

Eu comparei **todas as entradas da árvore Git** e restaurei os sete arquivos que faltavam na minha cópia (um recurso visual e dois pequenos mods de teste), conferindo os hashes originais.

A comparação registrada no [JSON de inventário](docs/POTATO-UPSTREAM-FILE-MAP.json) contém o resumo quantitativo atualizado e os arquivos exatos, com blobs `sourceBlob` e `upstreamBlob`. A diferença inclui fontes, documentação, exemplos, recursos e testes; não se limita às classes C#.

Eu preservei os arquivos de QuickJS, a estrutura Android, launcher, exemplos JavaScript e recursos atuais do seu código. As atualizações que eu já tinha incorporado em branches anteriores incluem modo Jornada com `ResearchUnlockCount`, tratamento de bolsas `PreHardmodeLikeBossBag` e correção de nomes de linhas de tooltip. **Os arquivos compartilhados que são diferentes não significam necessariamente que ficaram desatualizados**; são, em grande parte, pontos nos quais o C# se conecta ao núcleo original.

Esta é uma cópia de fontes compatível **estruturalmente com o commit comparado**, não uma promessa de que todo hook e ABI foi testado contra cada aparelho ou futura atualização sua.

## 3. Como eu encaixei o C# no projeto Android

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

## 4. Como compilar e o que está faltando fora do Git

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

**Dependência especialmente importante:** o meu build da `.95.2` utiliza Mono Android ARM64 `8.0.23` com um ajuste de política de sinais (símbolo `mono_bunny_android_signal_policy_v1`). Esse binário foi recuperado de um build anterior validado e armazenado **privadamente**. O Mono genérico do NuGet não possui necessariamente essa alteração. Por isso, apesar de os fontes estarem completos, ainda é preciso fornecer o runtime corrigido por um meio permitido, ou reproduzir o ajuste e validá-lo.

Os arquivos de jogo `libil2cpp.so`, `libunity.so`, assets do Terraria, classes Unity proprietárias, chave de assinatura e outros inputs protegidos também não são publicados no Git. Deixei como referência o [workflow com a receita de build do C#](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/blob/handoff/potato-bunny-csharp-20261018/.github/workflows/build-bunny-csharp-source-loader.yml). Esse workflow usa meus caminhos e segredos privados, então é necessário adaptá-lo à sua infraestrutura; não basta copiá-lo literalmente.

Deixei `applicationId="com.bunnyloader.csharptest"` e versionamento experimental em `app/build.gradle.kts` para preservar o checkpoint de testes e evitar sobrepor sua instalação oficial. Eu recomendo que você só mude para o ID e a assinatura oficiais **depois de compilar e testar**. Mudar o ID também exige revisar paths/persistência e configurações vinculadas ao pacote.

## 5. O que eu te entrego para subir no seu Git

Eu deixei o Android **direto na raiz** da branch `entrega/potato-bunny-original-com-csharp-20261018`. Os arquivos já estão encaixados. Você pode abrir e compilar o projeto, ou seguir o [guia de importação](IMPORTAR-NO-GITHUB-DO-POTATO.md) para copiá-los para uma branch do seu próprio repositório com um único comando, sem perder seu histórico.

Não fiz commit nem push no seu GitHub, porque essa etapa deve acontecer a partir do seu repositório e você precisa revisar o resultado. O script de importação não força merges, não apaga nada, não limpa diretórios, não altera a assinatura, não gera binários e não publica um APK.

Se quiser manter as duas linguagens oficialmente, eu seguiria essa sequência de verificação após importar: abrir launcher; carregar mods JS; carregar mod C#; carregar JS e C# juntos; conferir preparação antes de entrar no mundo; receitas, acessórios, buffs, NPCs, projéteis, save/reentrada; e, por último, o minion `.95.2` com invocação, ataque, cancelamento de buff, reinvocação e respawn. Mediria FPS e comportamento no Android ARM64, sem inferir desempenho só pelos testes do CI.

**Resumindo:** entreguei minha implementação de C# já acoplada ao seu projeto original, na estrutura que você usa. Mantive suas fontes JS e documentação original, registrei todas as diferenças com hashes e deixei claro o que foi testado e o que precisa de validação. A integração C# não executa código do tModLoader nem depende dos exemplos ExMod/Thorium; são APIs e backend próprios do Bunny.

## 6. Créditos

Eu mantive a autoria do **Jonata Oliveira** para o Bunny Loader original e os termos do [LICENSE.md](LICENSE.md), [LICENSING.md](LICENSING.md), [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) e [TRADEMARKS.md](TRADEMARKS.md). O suporte C# é a extensão que desenvolvi para o projeto. Não inclui a redistribuição de recursos proprietários do Terraria ou Unity.

*Mantovani*
