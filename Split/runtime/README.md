# Runtime Terraria 1.4.5.8.6

O Bunny Loader usa os arquivos do Terraria apenas durante o build completo.

## Release privada esperada

Tag:

`runtime-1.4.5.8.6`

Asset:

`BunnyRuntime_Terraria_1.4.5.8.6.zip`

SHA-256:

`2eb8d02dea144cb2fff6a27b94cc371e50ffd9aa44ba4dcd123337691a54c0ab`

O ZIP deve conter:

```text
lib/arm64-v8a/libil2cpp.so
lib/arm64-v8a/libunity.so
lib/arm64-v8a/libmain.so
lib/arm64-v8a/libc++_shared.so
assets/bin/Data/...
```

O workflow `Build Split Bunny Full Runtime` baixa esse asset da Release privada,
valida o hash, copia os arquivos para os caminhos esperados pelo Bunny e compila
o APK completo.

O runtime não é incluído no histórico Git da pasta `Split/Bunny-Loader`.
