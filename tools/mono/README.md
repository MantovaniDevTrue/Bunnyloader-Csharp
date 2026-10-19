# MonoVM ARM64 — fonte do patch e compilação

A modificação que fiz no Mono Android ARM64 está **presente neste código-fonte**. Não é necessário descobrir de onde veio o `libmonosgen-2.0.so` antigo nem ter acesso a um repositório privado para **reproduzir a alteração em código-fonte aberto**.

Arquivos desta pasta:

- [`patch_mono_android_signals.py`](patch_mono_android_signals.py): aplica, de forma verificável e restrita, meu ajuste no `mono-threads-posix-signals.c` do `dotnet/runtime` `v8.0.23`.
- [`build_patched_mono_arm64.sh`](build_patched_mono_arm64.sh): clona o .NET Runtime `v8.0.23`, aplica o patch, executa o teste de fonte, compila o Mono ARM64, confere os símbolos exportados, copia a biblioteca e gera SHA-256.
- [`tests/test_mono_android_signals_patch.py`](../../tests/test_mono_android_signals_patch.py): confirma que os três sinais de suspensão/reinício/abort no ARM64 usam a seleção de sinais RT alternativa e preserva o comportamento Android 32 bits.

## Dependências da recompilação

- Linux (a receita anterior foi feita em runner Ubuntu).
- Git, Python 3, `readelf` do binutils, .NET SDK adequado ao `dotnet/runtime v8.0.23` e ferramentas nativas exigidas pelo repositório .NET.
- Android SDK + **NDK 30.0.16248370**, configuração usada no workflow que produziu o runtime modificado pela primeira vez.
- Espaço em disco e tempo suficientes para compilar o runtime Mono. A receita baixa os fontes públicos do .NET.

Definir `ANDROID_HOME` ou `ANDROID_SDK_ROOT`, além de `ANDROID_NDK_ROOT` se não usar a versão padrão. A partir da raiz do Bunny Loader integrado:

~~~bash
bash tools/mono/build_patched_mono_arm64.sh
~~~

Ou com saída explícita:

~~~bash
ANDROID_NDK_ROOT="$ANDROID_HOME/ndk/30.0.16248370" \
  bash tools/mono/build_patched_mono_arm64.sh out/mono-android-arm64-8.0.23
~~~

Arquivos esperados após uma execução **bem-sucedida**:

~~~text
out/mono-android-arm64-8.0.23/
  libmonosgen-2.0.so
  libmonosgen-2.0.so.sha256
  DOTNET_RUNTIME_LICENSE.txt
~~~

Depois de preparar o framework oficial `Microsoft.NETCore.App.Runtime.Mono.android-arm64 8.0.23` com `tools/stage_mono_runtime.py`, substituir **somente** `app/src/main/jniLibs/arm64-v8a/libmonosgen-2.0.so` pela versão recompilada, mantendo as DLLs e demais bibliotecas ARM64 correspondentes do pacote oficial.

~~~bash
cp out/mono-android-arm64-8.0.23/libmonosgen-2.0.so \
  app/src/main/jniLibs/arm64-v8a/libmonosgen-2.0.so
readelf -Ws app/src/main/jniLibs/arm64-v8a/libmonosgen-2.0.so \
  | grep mono_bunny_android_signal_policy_v1
~~~

O símbolo `mono_bunny_android_signal_policy_v1` identifica a versão com seleção de sinais para ARM64. O host MonoVM do Bunny verifica esse símbolo ao iniciar. A biblioteca recompilada deve também exportar as APIs Mono `mono_jit_init` e `mono_thread_attach`.

## Histórico e limites reais

A build Android da `.95.2` **foi validada pelo CI** usando uma `libmonosgen-2.0.so` previamente recompilada e extraída de um APK experimental privado `.72`. A receita de compilação pública foi executada antes, numa linha histórica da `.57`, e é por isso que o patch e as verificações estão nesta fonte. A reprodução em outra máquina **não garante uma biblioteca byte a byte idêntica**, nem comprova que o APK final continuará estável com outros toolchains. É obrigatório testar no Android o bootstrap, jogo e mods C#.

A modificação atua na seleção de sinais do **Mono**, não nos binários do Terraria. Nenhum `libil2cpp.so`, `libunity.so`, asset do jogo, chave, token ou segredo faz parte do script.

O código original do .NET Runtime e a biblioteca Mono compilada continuam sujeitos às licenças e avisos originais do projeto [dotnet/runtime](https://github.com/dotnet/runtime/tree/v8.0.23). Ao redistribuir a biblioteca, preserve os avisos aplicáveis. O script copia a licença principal da árvore original quando ela está disponível.

**Observação:** este é um caminho de **build de fonte**, não uma declaração de que a biblioteca final já foi recompilada nesta branch de entrega. Para reproduzir o binário no GitHub Actions, a mesma receita pode ser executada em um runner com Android NDK instalado.
