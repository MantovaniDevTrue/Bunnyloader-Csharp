# Validação local do Lights .88.8

Código de runtime: `af844936dfa0b9435388723d02724daa5024985f`. Lights And Shadows CSharp 0.4.2.3, sem alterações nos GLSL.

O script [lights_egl_host.py](lights_egl_host.py) usa EGL surfaceless e OpenGL ES 3, extrai o vertex shader e o prelude reais de `ShaderLibrary.cpp`, compila e liga os seis fragment shaders do pacote e desenha três caminhos neutros em um pbuffer 1x1. Exige libEGL/Mesa no host.

```sh
mkdir -p /tmp/bunny-mesa-cache
MESA_SHADER_CACHE_DIR=/tmp/bunny-mesa-cache python3 docs/validation/lights_egl_host.py \
  Split/Bunny-Loader/app/src/main/cpp/graphics/ShaderLibrary.cpp \
  Split/Bunny-Loader/samples/LightsAndShadowsCSharp/Playable/content/Effects
```

Resultado observado em 17/10/2026:

```text
HOST_GLES_VERSION=OpenGL ES 3.2 Mesa 25.0.7-2+deb13u1
HOST_GLES_RENDERER=llvmpipe (LLVM 19.1.7, 256 bits)
LIGHTS_HOST_GLES3_COMPILE_LINK_AND_NEUTRAL_PIXELS=PASS shaders=6 pixelCases=3
```

Entrada RGBA: (32, 64, 96, 255). Com `uSunEnabled=0` (valor inicial do uniform): ShadowMask retorna (255, 255, 255, 0), ShadowRays (0, 0, 0, 255) e LightCompose preserva (32, 64, 96, 255). O teste verifica o pixel de cada caso e a ausência de erro GL. Todos os seis shaders são compilados/ligados; somente esses três caminhos têm imagem verificada aqui.

Isso não executa o encadeamento completo dos nove passes, a aquisição de framebuffer do Unity, as transições de mundo, a GPU Adreno/Mali ou o MonoVM Android. Não mede FPS, custo de primeira compilação nem fidelidade visual completa. Essas verificações permanecem no roteiro Android do [relatório](../csharp-review-v88-8.md).
