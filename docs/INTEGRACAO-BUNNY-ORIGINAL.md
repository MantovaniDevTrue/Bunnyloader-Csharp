# Integrar o C# ao Bunny Loader original — guia para o Potato

> **Escopo:** integração do suporte C# (Roslyn + MonoVM + hooks nativos) ao projeto Android original do [Bunny Loader](https://github.com/JonataOliveiraa/Bunny-Loader), mantendo o JavaScript/QuickJS existente. **Não é** um tutorial de tModLoader, ExMod ou Thorium. Nada aqui concede acesso ao repositório original nem afirma que ele já contém nosso C#.

## Ponto de partida verificado

Este guia foi preparado comparando o `main` do [Bunny Loader C#](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp) em `bddb8bf54fbdaf69a41ff077c762498ee3ce6226` com o `main` do [Bunny original](https://github.com/JonataOliveiraa/Bunny-Loader) em `a370a664aa3b808c87e4e4f570b27504d20df723` (18/10/2026). As duas árvores **não são intercambiáveis**: o Bunny original recebe mudanças próprias. Antes de portar, atualizar as referências e revisar os conflitos.

| Termo neste documento | Caminho no computador |
| --- | --- |
| `PORT` (fonte C#) | `Bunnyloader-Csharp/Split/Bunny-Loader/` |
| `UPSTREAM` (destino) | `Bunny-Loader/` |
| `RAIZ` (documentação, ferramentas deste port) | `Bunnyloader-Csharp/` |

**Regra de conversão:** remova apenas o prefixo `Split/Bunny-Loader/`. Exemplo: `RAIZ/Split/Bunny-Loader/app/src/main/cpp/experiments/csharp/ManagedRuntime.cpp` passa a `UPSTREAM/app/src/main/cpp/experiments/csharp/ManagedRuntime.cpp`. Nunca ponha a pasta `Split/` dentro do original e nunca mova a pasta `experiments/csharp` para `app/src/main/cpp`: são **dois diretórios distintos**.

## Visão rápida da integração

```text
Bunny-Loader/                               <- raiz do Bunny ORIGINAL
├── app/
│   ├── build.gradle.kts                    <- conciliar, não substituir
│   └── src/main/
│       ├── AndroidManifest.xml             <- declarar serviço Mono
│       ├── cpp/
│       │   ├── CMakeLists.txt              <- targets bunny e bunnymonoprobe
│       │   ├── experiments/csharp/         <- host C++/Mono; pasta NOVA
│       │   ├── boot/                       <- integrar início e gate do mundo
│       │   ├── mods/                       <- detectar e carregar mods C#
│       │   ├── content/                    <- registros, receitas e hooks comuns
│       │   └── il2cpp/                     <- resolver layouts/ABI
│       ├── kotlin/dev/bunnyloader/
│       │   ├── csharp/ManagedProbeService.kt  <- serviço NOVO
│       │   ├── GameActivity.kt             <- conciliar
│       │   ├── game/BootLog.kt             <- conciliar
│       │   └── nativebridge/NativeBridge.kt <- conciliar
│       ├── java/net/dot/android/crypto/    <- suporte JNI do Mono
│       ├── jniLibs/arm64-v8a/              <- .so GERADAS/privadas no build
│       └── assets/managed/                 <- DLLs GERADAS no build
├── experiments/csharp/
│   ├── BunnyCsSdk/                         <- fontes públicos C#
│   ├── BunnySourceSdk/                     <- projeto DLL do SDK
│   ├── BunnySourceCompiler/                <- Roslyn
│   ├── BunnySourceCompiler.Smoke/          <- smoke do compilador
│   └── BunnyManagedProbe/                  <- entrada gerenciada
├── samples/                                <- exemplos, opcional no port
└── tools/                                  <- scripts de staging do Mono
```

## 1. Fazer inventário e copiar apenas arquivos novos

Clone **separadamente** os dois repositórios, sem apagar o Git do Potato:

```bash
git clone https://github.com/JonataOliveiraa/Bunny-Loader.git Bunny-Loader
git clone https://github.com/MantovaniDevTrue/Bunnyloader-Csharp.git Bunnyloader-Csharp
cd Bunny-Loader
git switch -c feature/csharp-monovm
cd ..
python3 Bunnyloader-Csharp/tools/potato_integration_audit.py --target Bunny-Loader
```

O comando é **somente leitura** e imprime `ADD_NEW`, `ALREADY_IDENTICAL`, `COLLISION_REVIEW`, `MERGE_MANUALLY` ou `MISSING_ANCHOR` para os locais do checklist. Ele não garante compatibilidade nem detecta todas as alterações futuras do upstream.

Depois de verificar as diferenças e se não houver colisões nos novos arquivos:

```bash
python3 Bunnyloader-Csharp/tools/potato_integration_audit.py --target Bunny-Loader --apply-additions
```

Essa opção copia **somente novos caminhos ausentes**; nunca substitui arquivo ou pasta existente e nunca faz os merges. Em caso de colisão, interrompe antes de copiar. Revise o `git status` do destino; não execute comando de cópia sobre o `app/` inteiro.

### Fontes novas: destinos exatos

| Da fonte C# (dentro de `PORT/`) | Colocar no Bunny original (dentro de `UPSTREAM/`) | Função |
| --- | --- | --- |
| `app/src/main/cpp/experiments/csharp/` | `app/src/main/cpp/experiments/csharp/` | Host C++, MonoVM isolado, Native Policies, filas, eventos |
| `experiments/csharp/BunnyCsSdk/` | `experiments/csharp/BunnyCsSdk/` | Classes e interfaces de mods C# |
| `experiments/csharp/BunnySourceSdk/` | `experiments/csharp/BunnySourceSdk/` | Compilação do SDK compartilhado |
| `experiments/csharp/BunnySourceCompiler/` | `experiments/csharp/BunnySourceCompiler/` | Compilação Roslyn no Android |
| `experiments/csharp/BunnySourceCompiler.Smoke/` | `experiments/csharp/BunnySourceCompiler.Smoke/` | Smoke test da compilação |
| `experiments/csharp/BunnyManagedProbe/` | `experiments/csharp/BunnyManagedProbe/` | Entrada gerenciada do worker |
| `experiments/csharp/BunnyVmBenchmark67/` e `BunnyVmGameplay69/` | Mesmos caminhos | Assemblies usados pelo `GameActivity` atual; não são a API de mods |
| `app/src/main/kotlin/dev/bunnyloader/csharp/ManagedProbeService.kt` | Mesmo caminho | Serviço Android isolado (`:mono_probe`) |
| `app/src/main/java/net/dot/android/crypto/DotnetProxyTrustManager.java` | Mesmo caminho | Classe Java acessada na inicialização do Mono |
| `app/src/main/java/dev/bunnyloader/game/LogcatDiagnostics.java` | Mesmo caminho | Diagnóstico integrado ao `BootLog` do port |
| `app/src/main/java/com/unity3d/player/UnityJniClasses.java` | Mesmo caminho | Compatibilidade JNI existente no port; verificar contra classes Unity do upstream |
| `tools/stage_mono_runtime.py` e `tools/mono/` | Mesmos caminhos | Empacotamento e investigação de sinal Mono |
| `app/src/main/cpp/content/items/ItemRegistry.cpp` e `ItemRegistryInternal.h` | Mesmo diretório | Registro de itens |
| `app/src/main/cpp/content/recipes/NativeRecipes.cpp` e `NativeRecipes.h` | Mesmo diretório | Receitas nativas |
| `app/src/main/cpp/content/tiles/TileRegistry.cpp` e `TileRegistryInternal.h` | Mesmo diretório | Registro de tiles |
| `app/src/main/cpp/content/sounds/NativeSoundRouter.*` e `NativeMusicRouter.*` | Mesmos nomes | Serviços de áudio |
| `app/src/main/cpp/il2cpp/ValueLayout.h` | Mesmo caminho | Layouts nativos usados pela ponte |
| `app/src/main/cpp/script/api/CSharpPrototypeApi.cpp` e `.h` | Mesmos caminhos | API experimental de diagnóstico; **não** é dependência para executar mods C# via JS |

O inventário do script contém os nomes completos de cada arquivo `Native*`, inclusive os headers. No `UPSTREAM` nunca copiar `bin/`, `obj/`, `build/` ou executáveis de testes como se fossem fontes.

## 2. Integrar os arquivos existentes — obrigatório e manual

**Não sobrescrever esses arquivos com os do port.** Abra o diff entre `PORT/<caminho>` e `UPSTREAM/<caminho>`, importe só os trechos C# e mantenha as mudanças recentes do Potato. O verificador imprime um `MERGE_MANUALLY` para cada caminho diferente.

| Arquivo no destino | O que tem de ser integrado e conferido |
| --- | --- |
| `app/src/main/cpp/CMakeLists.txt` | Manter `bunny` + QuickJS; adicionar as fontes C# via glob e o target `bunnymonoprobe` com `ManagedRuntime.cpp`, `MonoWorkerJni.cpp`, `NativeAbortProbe.cpp`. Excluir `MonoWorkerJni.cpp` da `libbunny` (está no outro target); não duplicar símbolos. |
| `app/src/main/cpp/boot/Probe.cpp` | Inicializar `csharpmods::loadAll()`, `csharp::startAutoSourceRuntime()`, hooks necessários e `installCSharpWorldEntryGate()` no ciclo correto, sem tirar `mods::loadAll()` JavaScript. |
| `app/src/main/cpp/boot/QuickStart.cpp` | Conferir o gate `preWorldCSharpContentReady()` para não entrar no mundo antes do registro C#. Não bloquear a thread do jogo esperando compilação. |
| `app/src/main/cpp/boot/jni_entry.cpp` e `core/Config.h` | Contratos JNI: `managedProbePath`, `managedCheckpointPath`, `appContext`; vincular o contexto do serviço isolado; corresponder exatamente às propriedades Kotlin. |
| `app/src/main/cpp/mods/ModLoader.cpp` | Descobrir mods C# como mods independentes e chamar `loadCSharpSourceMod` sem converter para JavaScript. |
| `app/src/main/cpp/content/{common,items,npcs,projectiles,tiles,sounds}/` | Conciliar registries, persistência, áudio, receitas, NPC/projéteis e políticas nativas. São pontos compartilhados com QuickJS; **não** substituir em bloco. |
| `app/src/main/cpp/il2cpp/Api.cpp` e `Api.h` | Resolver/validar métodos, layouts e chamadas de Terraria; conferir ABI/versão do jogo. |
| `app/src/main/cpp/menu/Cheats.cpp` e `CheatButton.cpp` | Conservar menu original ao adicionar ações C# do port. |
| `app/src/main/kotlin/dev/bunnyloader/GameActivity.kt` | Preparar carregamento nativo, instalar assemblies quando aplicável e configurar paths, preservando launcher do original. |
| `app/src/main/kotlin/dev/bunnyloader/nativebridge/NativeBridge.kt` | Estrutura de configuração deve corresponder ao `jni_entry.cpp`, campo por campo. |
| `app/src/main/kotlin/dev/bunnyloader/game/BootLog.kt` | Mensagens/diagnóstico C#; preservar relatório normal JavaScript. |
| `app/src/main/kotlin/dev/bunnyloader/mods/ModRepository.kt` | Importação, identificação e seleção de mods; verificar contrato real de `manifest.json`. |
| `app/src/main/kotlin/dev/bunnyloader/game/ModAudio.kt` | Rotas de áudio e concorrência; combinar sem regressões. |
| `app/src/main/AndroidManifest.xml` | Declarar `.csharp.ManagedProbeService`, com `android:process=":mono_probe"` e demais flags do port, sem alterar outras activities/services originais. |
| `app/build.gradle.kts` | Preservar `applicationId`, assinatura, nome, versão, plugins e Java/Kotlin do original; ajustar para embutir assets/serviços nativos e impedir exclusão das bibliotecas C#. |
| `app/src/main/res/values/strings.xml` | Só mesclar recursos efetivamente necessários, preservando traduções upstream. |

Outros arquivos modificados na comparação, que **precisam de revisão antes da entrega**: `content/items/ModItemSave.{cpp,h}`, `content/tiles/ModTileSave.cpp`, `content/common/TypeTables.{cpp,h}` e outros arquivos de conteúdo já citados na tabela; também `script/api/Sounds.cpp`, `script/bridge/Bindings.cpp` e vários `script/js/*`. **Divergência no JS não prova dependência do C#**. Não copiar os loaders JS por atacado; conservar comportamento do QuickJS. O script contém o inventário de todas as diferenças de arquivos existentes identificadas entre os dois snapshots.

### Conferir âncoras técnicas

No `Probe.cpp` deste port, o caminho de inicialização chama `mods::loadAll(...)`, `csharpmods::loadAll()` e `csharp::startAutoSourceRuntime()`. Em `QuickStart.cpp` o gate consulta `preWorldCSharpContentReady()`. Em `jni_entry.cpp` o campo `appContext` é passado a `bindMonoWorkerContext(...)`. São **exemplos reais**, não trechos para colar cegamente em outra versão.

Em `CMakeLists.txt` o `file(GLOB_RECURSE BL_SOURCES ...)` incorpora arquivos C++ novos, mas o JNI do worker sai explicitamente da `libbunny` e entra em `libbunnymonoprobe`. Validar as duas bibliotecas é obrigatório.

## 3. Compilar e embutir MonoVM / Roslyn (não copiar DLLs velhas)

O pipeline de referência está em `RAIZ/.github/workflows/build-bunny-csharp-source-loader.yml`, especialmente a preparação de Mono ARM64, geração de DLLs/`framework.sha256` e validação do APK. Ele possui caminhos e segredos de **nossa infraestrutura**: não copiar o workflow inteiro para o repositório do Potato.

**Entradas de compilação:** .NET SDK 8 (e as versões adicionais declaradas pelo workflow), JDK 17, Gradle/Android SDK, NDK/CMake definidos pelo projeto Android. A receita atual usa `Microsoft.NETCore.App.Runtime.Mono.android-arm64 8.0.23`. A biblioteca `libmonosgen-2.0.so` final **não é a genérica do NuGet**: o pipeline atual reutiliza uma versão modificada e validada do runtime .72 em armazenamento privado, conferida pelo símbolo `mono_bunny_android_signal_policy_v1`. Esse artefato **não está publicado** neste GitHub. É uma dependência de integração real: obter uma build autorizada/compatível ou reproduzir e validar a adaptação de sinais antes de declarar suporte Android equivalente.

| Gerar a partir de | Entregar **no build local/CI**, nunca no Git público |
| --- | --- |
| `tools/stage_mono_runtime.py` e pacote oficial Mono Android ARM64 | `app/src/main/jniLibs/arm64-v8a/libSystem.Native.so` e dependências `.so`; DLLs framework em `app/src/main/assets/managed/` |
| Mono Android ARM64 com a política de sinais compatível | `app/src/main/jniLibs/arm64-v8a/libmonosgen-2.0.so` |
| Target CMake `bunny` | `lib/arm64-v8a/libbunny.so` no APK |
| Target CMake `bunnymonoprobe` | `lib/arm64-v8a/libbunnymonoprobe.so` no APK |
| `experiments/csharp/BunnyManagedProbe/` | `app/src/main/assets/managed/BunnyManagedProbe.dll` |
| `experiments/csharp/BunnySourceSdk/` | `app/src/main/assets/managed/BunnySourceSdk.dll` |
| `experiments/csharp/BunnySourceCompiler/` | `app/src/main/assets/managed/BunnySourceCompiler.dll`, `Microsoft.CodeAnalysis.dll` e `Microsoft.CodeAnalysis.CSharp.dll` |
| `BunnyVmBenchmark67/` e `BunnyVmGameplay69/` | DLLs homônimas em `app/src/main/assets/managed/`, enquanto a `GameActivity` atual precisar delas |
| Lista de DLLs geradas, com SHA-256 de cada uma | `app/src/main/assets/managed/framework.sha256`: **nome + TAB + SHA-256 hexadecimal**, conforme workflow |

`ManagedProbeService` lê os assemblies em `assets/managed`, valida `framework.sha256`, prepara `filesDir/bunny-csharp/` e chama `bunnymonoprobe`. O código gerenciado é compilado por Roslyn no Android. **Não** é preciso distribuir uma `.so` por mod, nem usar QuickJS para executar callbacks C#.

Não publicar `libil2cpp.so`, `libunity.so`, assets do Terraria, credenciais privadas, keystore, executáveis proprietários ou o APK com jogo integrado. Esses inputs só podem ser fornecidos por meios autorizados e conforme suas licenças.

## 4. Exemplos, testes, GitHub e entrega

- **Exemplos**: `PORT/samples/ExampleModCSharp/`, `SourceCSharpMod/`, `SecondExampleModCSharp/`, `ArmorPoliciesCSharp/` e outros são projetos separados. Se desejar levá-los, copiar **cada mod** para `UPSTREAM/samples/<Nome>/`, respeitando `manifest.json`/UID e sem substituir `samples/ExampleMod` JavaScript.
- **Ferramentas de teste**: `PORT/tests/` e `PORT/tools/` contêm suites C#, testes Android e scripts; integrar a seleção relevante, evitando sobrescrever testes e ferramentas originais que mudaram no upstream.
- **Documentação**: a referência atual está em [arquitetura](ARQUITETURA.md), [SDK](SDK-CSharp.md), [validação](SDK-VALIDACAO.md), [build](BUILD.md) e [status](STATUS.md). O conteúdo em `history/` e projetos NativeAOT são históricos, não o caminho padrão de MonoVM.
- **Licença**: preservar `LICENSE.md`, `LICENSING.md`, `THIRD_PARTY_NOTICES.md` e créditos. O licenciamento deve ser analisado antes de reuso/publicação, inclusive para as dependências.
- **Pull request para o original**: dividir em PRs pequenos e revisáveis: (1) SDK + Roslyn, (2) worker Mono + build, (3) bootstrap e detecção de mods, (4) conteúdo/hook nativo e persistência, (5) testes + exemplos. Não fazer force push, não substituir `main` e não copiar nossos tokens/infra de releases.

### Checklist de aceitação — não marcar por suposição

- [ ] `git diff` no original não apaga mudanças JS, launcher, manifests ou funcionalidades recentes.
- [ ] As classes Kotlin/JNI têm o mesmo contrato; `ManagedProbeService` existe no manifesto.
- [ ] CMake compila `libbunny.so` e `libbunnymonoprobe.so` sem duplicação de `MonoWorkerJni.cpp`.
- [ ] O APK contém `assets/managed/framework.sha256`, SDK, Roslyn, CoreLib, Runtime, Mono .so e worker.
- [ ] As hashes de `framework.sha256` batem com todas as DLLs no pacote.
- [ ] O worker compila um `.cs` automaticamente; o mod C# aparece habilitado **antes** de entrar no mundo.
- [ ] Um mod JavaScript original e um mod C# funcionam simultaneamente.
- [ ] Item, receita, tile, acessório, NPC, projétil, hooks, áudio e salvar/reabrir funcionam conforme os exemplos cobertos.
- [ ] Testar pausas/retomadas, sair/entrar do mundo, ativar/desativar mod e falha isolada do worker.
- [ ] Registrar FPS/latência real no ARM64; testes de host não comprovam FPS.
- [ ] Não declarar multiplayer ou hot-unload universal sem testes dedicados.

**Estado da entrega:** este documento e o script preparam uma **migração revisável**. Não são uma PR no repositório do Potato e não substituem compilação, resolução de conflitos ou testes Android reais. Se o original mudar, refazer a comparação; a lista acima é vinculada aos snapshots citados.
