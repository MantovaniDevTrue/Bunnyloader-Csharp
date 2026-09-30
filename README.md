# Split Bunny Loader Workspace

Workspace privado para o port do Split no Bunny Loader.

## Estrutura

- `Split/build/`: scripts e arquivos de build.
- `Split/mod/`: builds `.bmod` do port.
- `.github/workflows/build-split-bunny.yml`: compila o Bunny Loader pelo GitHub Actions.

O código do Bunny Loader é obtido diretamente de `JonataOliveiraa/Bunny-Loader` durante o build, mantendo o projeto sincronizado com a fonte original.

O runtime proprietário do Terraria não é armazenado neste repositório.

## Build

O workflow gera um APK ARM64 com o núcleo nativo do Bunny e a interface, usando `-Pbl.uiOnly=true`. Esse APK serve para validar a compilação do launcher e da `libbunny.so`; para iniciar o Terraria, o runtime correspondente precisa ser integrado localmente.
