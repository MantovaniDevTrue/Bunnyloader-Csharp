# Build e CI

## Verificar fontes sem o jogo

O SDK/testes gerenciados usam **.NET SDK 8**, os testes nativos usam **g++ com
C++20**, Python 3 e, nas regressões do adaptador legado, Node.js. Comece pelos
[comandos de teste](../tests/README.md). Nenhum binário do Terraria é necessário
para essas suítes de host.

A checagem de organização é independente do toolchain Android:

```sh
python3 tools/check_repository.py
```

## Compilar Android

O projeto canônico fica em `Split/Bunny-Loader`. O workflow
[build-bunny-csharp-source-loader.yml](../.github/workflows/build-bunny-csharp-source-loader.yml)
é a receita reproduzível para a build C# atual: JDK 17, .NET SDK 8 e 10,
Gradle 8.9, Android SDK/build-tools 34, NDK `30.0.16248370` e CMake `3.22.1`.
As versões efetivas são as declaradas no workflow e em
[app/build.gradle.kts](../Split/Bunny-Loader/app/build.gradle.kts).

Uma build que inicia o Terraria também requer inputs restritos: runtime do
jogo, classes Unity, assemblies e assinatura. O workflow verifica os hashes
antes de usá-los e publica a entrega no repositório privado. Um clone público
sozinho permite ler/testar o código de host, mas não produzir um APK completo
com esses inputs ausentes. Consulte também o [guia original de build](../Split/Bunny-Loader/README.md).

Não publique runtime integrado nem chaves. Não altere versionCode/versionName
por limpeza de documentação. Ao entregar um checkpoint, registre SHA fonte,
run, hashes dos pacotes e resultados Android em [STATUS.md](STATUS.md) e
[checkpoint.json](checkpoint.json). Use APK e ExampleMod da mesma release.

## Workflows

| Finalidade | Workflows |
| --- | --- |
| Navegação/checkpoint | [check-repository.yml](../.github/workflows/check-repository.yml) |
| SDK, fontes, referência e policies sem JS | [check-csharp-sdk-foundation.yml](../.github/workflows/check-csharp-sdk-foundation.yml) |
| Matriz dos exemplos | [check-example-completion.yml](../.github/workflows/check-example-completion.yml) |
| Ciclo de callbacks | [check-managed-lifecycle-regressions.yml](../.github/workflows/check-managed-lifecycle-regressions.yml) |
| GC/sinais Mono | [check-mono-pinned-gc.yml](../.github/workflows/check-mono-pinned-gc.yml), [check-mono-signal-ownership.yml](../.github/workflows/check-mono-signal-ownership.yml) |
| Filas visuais | [check-bunny-csharp-visual-queue.yml](../.github/workflows/check-bunny-csharp-visual-queue.yml), [check-bunny-csharp-native-visual-queue.yml](../.github/workflows/check-bunny-csharp-native-visual-queue.yml) |
| APK C# completo | [build-bunny-csharp-source-loader.yml](../.github/workflows/build-bunny-csharp-source-loader.yml) |
| Investigar ABI nativa | [check-csharp-native-policy-metadata.yml](../.github/workflows/check-csharp-native-policy-metadata.yml) |

Os demais workflows preservam testes, builds e manutenção de checkpoints
anteriores ou do port Split. Leia seus gatilhos antes de dispará-los; um nome
antigo não torna sua saída a build C# atual. Os workflows de manutenção,
assinatura e migração não fazem parte da checagem de documentação.

CI aprovado comprova o que cada suíte exercita. Teste de host com IL2CPP/Mono
simulados não comprova ABI, renderização, multiplayer ou 60 FPS no Android.

## Comparar atualizações do Bunny original

[Sync/Compare Bunny Source](../.github/workflows/sync-bunny-source.yml) agora
compara arquivos rastreados com um ref do upstream e entrega um relatório
como artifact. Tem permissão somente de leitura: não substitui a pasta do
loader, não faz push na main e não gera APK. Use o relatório para preparar
uma PR de integração e executar as suítes C# antes de incorporar mudanças.

O antigo workflow de integração, preso a uma versão anterior e ao significado
antigo de main, está [arquivado](history/workflows/integrate-jonata-csharp.yml).
A build UI-only do Split passou a ser manual, evitando APK legado a cada
edição dos fontes/README. As receitas históricas continuam consultáveis.
