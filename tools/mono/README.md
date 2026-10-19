# MonoVM modificado — fonte, biblioteca e instalação

A integração C# inclui **a biblioteca ARM64 corrigida que já era usada nos testes Android anteriores**, além do código da alteração e do processo de recompilação. Não é necessário ter acesso ao armazenamento privado das builds de teste para baixar a biblioteca.

**A biblioteca MonoVM ARM64, as dependências nativas e todas as DLLs do framework/SDK C# já estão nos caminhos finais do aplicativo nesta branch. Não há instalação manual necessária para usar a fonte.** O Gradle verifica os hashes antes do build. As instruções abaixo são opcionais e servem apenas para recompilação ou atualização futura.

## Arquivos incluídos

| Arquivo | Finalidade |
| --- | --- |
| [Biblioteca `libmonosgen-2.0.so`](prebuilt/arm64-v8a/libmonosgen-2.0.so) | MonoVM ARM64 `8.0.23` com a política de sinais validada na build `.72` |
| [SHA-256](prebuilt/arm64-v8a/libmonosgen-2.0.so.sha256) | Integridade verificável da biblioteca |
| [Licença .NET Runtime](prebuilt/arm64-v8a/DOTNET_RUNTIME_LICENSE.txt) | Licença do projeto público que originou o Mono |
| [Proveniência da biblioteca](prebuilt/arm64-v8a/README.txt) | Versão, símbolos obrigatórios e origem verificada |
| [`patch_mono_android_signals.py`](patch_mono_android_signals.py) | Código que aplica a modificação de sinais no Mono 8.0.23 |
| [`build_patched_mono_arm64.sh`](build_patched_mono_arm64.sh) | Recompila o Mono modificado da fonte pública |
| [`install_patched_mono_arm64.sh`](install_patched_mono_arm64.sh) | Confere SHA, ARM64 e símbolos, depois instala na pasta do Android |
| [Teste da alteração](../../tests/test_mono_android_signals_patch.py) | Verifica os três pontos de modificação de sinais no código do Mono |

## Manutenção opcional: reinstalar a biblioteca incluída

O Mono modificado vai para a pasta **`app/src/main/jniLibs/arm64-v8a/`**. Primeiro preparo o runtime padrão (DLLs e demais bibliotecas) do pacote NuGet `Microsoft.NETCore.App.Runtime.Mono.android-arm64 8.0.23` usando `tools/stage_mono_runtime.py`.

Depois de preparar o runtime oficial, executo **por último**:

~~~bash
bash tools/mono/install_patched_mono_arm64.sh
~~~

O script verifica a integridade do `.so`, exige arquitetura ARM64 e os símbolos `mono_bunny_android_signal_policy_v1`, `mono_jit_init` e `mono_thread_attach`. Só então copia a biblioteca para:

~~~text
app/src/main/jniLibs/arm64-v8a/libmonosgen-2.0.so
~~~

**Ordem importante:** `stage_mono_runtime.py` coloca o Mono genérico do NuGet no mesmo destino. A instalação da biblioteca modificada deve ser o **último passo da preparação do runtime**, ou a versão corrigida será substituída.

A biblioteca original usada no CI foi obtida de um APK experimental privado **sem compartilhar o APK**. A entrega pública contém somente esta biblioteca do Mono, SHA, licença e fontes da modificação. Não há Terraria/Unity, tokens ou chaves nesse diretório.

## Recompilação independente (opcional)

Para reconstruir a biblioteca a partir do código aberto do `dotnet/runtime v8.0.23`, use Linux, Git, Python 3, binutils, .NET SDK e Android SDK/NDK, com NDK `30.0.16248370` (versão usada no procedimento histórico). Configure `ANDROID_HOME` ou `ANDROID_SDK_ROOT` e, se necessário, `ANDROID_NDK_ROOT`.

~~~bash
bash tools/mono/build_patched_mono_arm64.sh
~~~

Isso clona o .NET Runtime `v8.0.23`, aplica `patch_mono_android_signals.py`, executa o teste, compila ARM64, verifica símbolos e escreve:

~~~text
out/mono-android-arm64-8.0.23/
  libmonosgen-2.0.so
  libmonosgen-2.0.so.sha256
  DOTNET_RUNTIME_LICENSE.txt
~~~

Uma recompilação independente não precisa resultar em SHA idêntico ao da biblioteca incluída, pois o NDK/toolchain e o ambiente podem influenciar os bytes. Também não substitui testes reais de compatibilidade Android.

## Validação e limites

A biblioteca incluída deriva do Mono ARM64 modificado que já tinha sido usado na `.72`. Na `.95.2`, esse mesmo artefato foi usado no [pipeline Android com build, assinatura e suítes de CI aprovadas](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39171952411).

Também executei a [extração e conferência pública da biblioteca isolada](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39184151091), sem redistribuir o APK de origem. **Não considero automaticamente validada a correção mais recente de minions da .95.2 no aparelho só por isso.**

O .NET Runtime original continua sujeito à licença e aos avisos aplicáveis do [dotnet/runtime](https://github.com/dotnet/runtime/tree/v8.0.23). Os componentes proprietários de Terraria e Unity permanecem fora do Git.
