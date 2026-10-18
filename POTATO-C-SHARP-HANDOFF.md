# Potato — entrega completa do Bunny Loader com C#

Este é o código-fonte COMPLETO do projeto Android de Bunny Loader com suporte adicional a C# (Roslyn + MonoVM + host nativo C++/IL2CPP). A integração foi desenvolvida por Mantovani a partir do Bunny original de Jonata Oliveira. O motor JavaScript QuickJS continua existindo e precisa continuar funcionando. A API de C# é própria; o mod não executa tModLoader nem precisa usar JavaScript para callbacks C#.

## 1. Onde baixar e abrir

**Branch recomendada para o Potato (Android na raiz):** `handoff/potato-android-project-root-20261018` em https://github.com/MantovaniDevTrue/Bunnyloader-Csharp. A branch `handoff/potato-bunny-csharp-20261018` mantém a mesma árvore Android dentro de `Split/Bunny-Loader/`, junto com ferramentas e histórico do projeto maior.

**Pasta Android real nesta entrega:** a própria **raiz do repositório clonado**, contendo `settings.gradle.kts`, `build.gradle.kts`, `app/` e `experiments/csharp/`. Não há pasta `Split/` nesta branch de exportação.

Comandos para obter a fonte e abrir no Android Studio:

```bash
git clone --single-branch --branch handoff/potato-android-project-root-20261018 https://github.com/MantovaniDevTrue/Bunnyloader-Csharp.git
cd Bunnyloader-Csharp
```

ATENÇÃO: a `main` do nosso GitHub ficou num checkpoint antigo `.90.29.62`. A branch desta entrega foi derivada do C# mais recente `.90.29.95.2`, e não daquela main. Ela é um PROJETO INTEIRO para estudar, compilar com as dependências autorizadas e portar; não é uma pasta de classes C# para arrastar no original.

## 2. Versões, sincronização e testes comprovados

- Bunny original: `JonataOliveiraa/Bunny-Loader@a370a664aa3b808c87e4e4f570b27504d20df723`, último commit da main consultado em 18/10/2026.
- Nosso último código de produção da entrega: `018b477a5a90115f399aed57ff84811c020d7a3f`, Bunny `0.1.10.16.90.29.95.2` (versionCode 272), ExampleMod C# `0.1.54`.
- O commit `a222d945683ac73fd65eace85c0bbef96286645a` adicionou os recibos de CI e documentação, sem mudança em código de produção. Esta entrega ainda restaurou sete arquivos de recurso/testes que faltavam do original, reorganizou o projeto para a raiz e adicionou documentação de entrega. É uma branch de exportação, não deve ser mesclada de volta como substituição da `main` do port.
- Build, assinatura e checks de CI da `.95.2`: https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39171952411. A entrega privada foi conferida no pipeline. Há registro em [recibo .95.2](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/blob/handoff/potato-bunny-csharp-20261018/docs/validation/csharp-v95-2-ci-2026-10-18.json).
- A jogabilidade da `.95.2` em Android REAL, sobretudo o summon/minion, AINDA ESTÁ PENDENTE. A `.95.1` apresentou uma falha de ABI `Projectile.identity` no aparelho; a `.95.2` removeu essa dependência e passou em testes de host e CI, mas esses testes não substituem nova validação Android.

A fonte desta entrega já inclui as atualizações conhecidas do original de 15/10: Journey `ResearchUnlockCount`, bolsas de chefe `PreHardmodeLikeBossBag`, tooltips `TooltipNames.Tooltip` e o suporte adicional de classes JS. Elas entraram em integração anterior, e não precisam ser aplicadas duas vezes.

## 3. Comparação integral, reprodutível

Comparamos todos os blobs Git da raiz deste projeto exportado (equivalente à pasta `Split/Bunny-Loader/` da branch de desenvolvimento) com os da raiz do Bunny original no commit acima.

| Arquivos | Quantidade |
| --- | ---: |
| Rastreados pelo original | 2024 |
| Presentes em nosso projeto Android | 2761 |
| **Idênticos ao original** | **1948** |
| Compartilhados mas modificados pelo port C# | 76 |
| Acrescentados pelo port (inclui fontes, testes e docs) | 737 |
| **Arquivos originais faltando em nossa árvore** | **0** |

Os sete arquivos inicialmente faltantes foram copiados de modo byte-idêntico do original: `_icons/launcher/titulo.png` e os arquivos de `tools/tests/research/` e `tools/tests/devarmor/`. A prova caminho-por-caminho está no arquivo [inventário completo](docs/POTATO-UPSTREAM-FILE-MAP.json). Cada arquivo modificado registra SHA do original e do nosso lado. Esse mapa é um diff **de conteúdo**, não uma afirmação de compatibilidade em execução.

## 4. Locais EXATOS no GitHub do Potato

Nesta branch exportada, **os caminhos já coincidem com a raiz do Bunny original**. Se estiver usando a outra branch de desenvolvimento, remova apenas o prefixo `Split/Bunny-Loader/`, sem criar uma subpasta `Split` no original.

| Aqui (dentro da pasta Android) | No original | Responsabilidade |
| --- | --- | --- |
| `experiments/csharp/BunnyCsSdk/` | `experiments/csharp/BunnyCsSdk/` | API pública dos mods C# |
| `experiments/csharp/BunnySourceSdk/` | `experiments/csharp/BunnySourceSdk/` | DLL gerenciada do SDK |
| `experiments/csharp/BunnySourceCompiler/` | `experiments/csharp/BunnySourceCompiler/` | Roslyn para compilar mods no Android |
| `experiments/csharp/BunnyManagedProbe/` | `experiments/csharp/BunnyManagedProbe/` | Entrada gerenciada MonoVM |
| `app/src/main/cpp/experiments/csharp/` | `app/src/main/cpp/experiments/csharp/` | Host C++, filas, hooks e políticas nativas |
| `app/src/main/kotlin/dev/bunnyloader/csharp/ManagedProbeService.kt` | Mesmo caminho | Worker Android em `:mono_probe` |
| `app/src/main/java/net/dot/android/crypto/DotnetProxyTrustManager.java` | Mesmo caminho | Suporte JNI/Mono |
| `app/src/main/cpp/content/` | `app/src/main/cpp/content/` | Receitas, tiles, buffs, itens, NPCs e projéteis |
| `samples/ExampleModCSharp/` | `samples/ExampleModCSharp/` | Exemplo gerenciado editável, separado do ExampleMod JS |
| `tools/stage_mono_runtime.py` | `tools/stage_mono_runtime.py` | Preparar o Mono ARM64 |

Existem DOIS diretórios `experiments/csharp` distintos. Na raiz há projetos C#; dentro de `app/src/main/cpp/` estão os arquivos C++ do host. Precisam dos dois.

## 5. Os arquivos que não podem ser sobrescritos

Os 76 arquivos divergentes precisam de revisão e merge manual, não `cp -r` sobre o Bunny oficial. Os pontos decisivos são:

1. `app/src/main/cpp/CMakeLists.txt`: manter `bunny` com QuickJS; acrescentar `bunnymonoprobe` com `ManagedRuntime.cpp`, `MonoWorkerJni.cpp` e `NativeAbortProbe.cpp`. Não compilar `MonoWorkerJni.cpp` nos dois targets.
2. `app/src/main/cpp/boot/Probe.cpp`, `QuickStart.cpp`: manter `mods::loadAll` JS; agregar `csharpmods::loadAll`, `startAutoSourceRuntime`, instalação de hooks e gate `preWorldCSharpContentReady`, sem congelar a thread do jogo.
3. `app/src/main/cpp/boot/jni_entry.cpp` e `core/Config.h`: vincular `appContext` ao worker e validar `managedProbePath` e `managedCheckpointPath`; os campos devem ser idênticos ao Kotlin em tipo, nome e assinatura JNI.
4. `app/src/main/cpp/mods/ModLoader.cpp` e `content/`: detecção de mods C#, registries compartilhados, proteção dos IDs, receitas, persistência, áudio, buffs e hooks, preservando JS.
5. `app/src/main/AndroidManifest.xml`: declarar `ManagedProbeService` com processo `:mono_probe`; não afetar o serviço de jogo original.
6. `app/src/main/kotlin/dev/bunnyloader/{GameActivity.kt,nativebridge/NativeBridge.kt,mods/ModRepository.kt,game/BootLog.kt}`: startup, configuração JNI, importação C# e registro de logs; preservar launcher e menus.
7. `app/build.gradle.kts`: **não substituir a identidade, assinatura e distribuição oficiais** pelos nossos valores de teste (o port usa `com.bunnyloader.csharptest`). Adaptar os assets e dependências nativas mantendo configurações do original.
8. `app/src/main/cpp/graphics/`, `script/`, `menu/` e UI Kotlin: revisar alterações específicas; os novos efeitos/diagnósticos não autorizam apagar nem enfraquecer QuickJS.

A lista exata dos 76, com hashes, fica no JSON de inventário citado.

## 6. Build C# real e limitações de distribuição

Projetos de referência: `experiments/csharp/BunnySourceSdk/BunnySourceSdk.csproj`, `BunnySourceCompiler/BunnySourceCompiler.csproj`, `BunnyManagedProbe/BunnyManagedProbe.csproj`. O SDK 8 e Roslyn produzem DLLs em `app/src/main/assets/managed/`. Os assemblies `BunnySourceSdk.dll`, `BunnySourceCompiler.dll`, `Microsoft.CodeAnalysis.dll`, `Microsoft.CodeAnalysis.CSharp.dll`, `BunnyManagedProbe.dll`, `System.Private.CoreLib.dll` e demais DLLs precisam estar consistentes com `managed/framework.sha256` (nome + TAB + SHA256). O projeto Android compila `libbunny.so` e `libbunnymonoprobe.so` para ARM64.

**Dependência crítica:** a produção atual usa Mono Android ARM64 8.0.23 com a política de sinais `mono_bunny_android_signal_policy_v1` em uma `libmonosgen-2.0.so` modificada e validada anteriormente. O workflow atual recupera esse binário de uma entrega privada; o NuGet oficial, sozinho, NÃO equivale a essa DLL modificada. Precisará obter um artefato permitido ou reproduzir a adaptação e validá-la.

Também são privados/externos o runtime e assets do Terraria, Unity classes, keystore e inputs da build com jogo. A fonte NÃO inclui ou autoriza redistribuição desses elementos. O workflow de referência é [workflow do projeto de desenvolvimento](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/blob/handoff/potato-bunny-csharp-20261018/.github/workflows/build-bunny-csharp-source-loader.yml) no repo de desenvolvimento; tem URLs, branches e secrets próprios, portanto deve ser adaptado, não importado literalmente.

## 7. Forma sugerida de adotar oficialmente

Usar esta fonte completa como referência e fazer mudanças revisáveis na sua branch do original, em lotes: (1) SDK/Roslyn; (2) worker/Mono/CMake/Gradle; (3) bootstrap e auto-load; (4) registries e hooks C# + persistência; (5) exemplos, diagnósticos e interface; (6) QA com JS/C# simultâneos. Em cada lote, confirmar que mods JavaScript continuam funcionando.

**Testar em ARM64 de verdade:** jogo inicia; um JS e um C# funcionam isoladamente e juntos; o C# prepara antes de entrar no mundo; receitas/armaduras/minions/NPC/projéteis, saves/reentrada, pause/resume, morte/respawn, ativação/desativação, FPS, crashes e diagnósticos. Multiplayer e hot-unload não devem ser prometidos universalmente sem teste específico.

Para a `.95.2`, o cenário prioritário no aparelho é carregar `ExampleModCSharp 0.1.54`, criar e invocar minion, seguir/atacar, cancelar buff, reinvocar, morrer, salvar e reentrar. CI passou, mas esse roteiro ainda não foi executado no Android após corrigir o problema da `.95.1`.

Créditos, licenças e marcas: Jonata Oliveira (Bunny Loader original), Mantovani (extensão C#). Preservar `LICENSE.md`, `LICENSING.md`, `THIRD_PARTY_NOTICES.md` e `TRADEMARKS.md`. Não disponibilizar APK integrado ao Terraria publicamente.

**English summary:** Full Android source, based on the latest upstream commit as checked on Oct 18, 2026. All 2,024 upstream files exist; 1,948 unchanged, 76 modified, 737 additions. C# runs on MonoVM/Roslyn/C++ with QuickJS preserved. Android CI built/signed the v95.2 artifact; device gameplay for the v95.2 minion fix is pending. Private game binaries and patched Mono are not included.
