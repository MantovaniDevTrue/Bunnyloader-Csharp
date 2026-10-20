# KeybindLoader C# / botões de toque — desenvolvimento, sem APK

Branch: feat/csharp-native-keybind-loader-20261020
Base: v1.0, sem tocar nas PRs #98, #100, #101, #102, #114–#119.

## Fontes de comportamento

- samples/ExampleMod/content/Common/Systems/TouchButtonSystem.js
- app/src/main/cpp/script/js/mod/Loaders/KeybindLoader.js
- app/src/main/cpp/script/js/mod/Loaders/TouchButtonLoader.js
- app/src/main/cpp/script/js/mod/Loaders/TouchButtonHooks.js
- app/src/main/cpp/script/js/mod/Loaders/TouchButtonLayouts.js

Estas referências do ExampleModBunny servem para comparar comportamento,
não para executar o loader C# através do QuickJS.

## Código feito

experiments/csharp/BunnyCsSdk/BunnyTouchButtonLoader.cs contém uma API de
ações por nome completo <modId>/<name>, registros idempotentes, proteção de
nomes/texturas, snapshot por GameUpdateCount, suporte a toques menores que
um tick, estados Current/Old/JustPressed/JustReleased, bloqueio de
RetroCurrent por chat/diálogo/placa, vínculo por layout e
UnregisterMod/ReleaseAll.

A tecla padrão de RegisterKeybind é intencionalmente ignorada no celular.
GetAssignedKeys devolve o rótulo apenas quando o editor marcou a ação como
presente no layout ativo.

Exemplo C# comentado: examples/csharp-keybind/ExampleTouchControls.cs.
Regressão de contrato: tests/keybind_loader/KeybindLoaderTests.csproj.

## O que impede declarar o loader COMPLETO

- [ ] Expor cada ação C# no editor de controles nativo, com nome localizado,
      textura, botões agrupados e ID consistente com as três tabelas do Terraria.
- [ ] Persistir FullName no layout e restaurar após reinício, remoção,
      retorno do mod e mudança de perfil.
- [ ] Fazer a ponte IL2CPP → snapshot C# no início de cada tick, sem QuickJS;
      preservar toque instantâneo anterior ao tick e estado de controle segurado.
- [ ] Adaptar callbacks e políticas de visibilidade/habilitação de forma segura,
      incluindo foco, pausa, menu, chat e troca de mundo.
- [ ] Definir/desarmar registros por mundo e hot-unload sem IDs reciclados
      no mesmo boot; bloquear enum/layout fora dos limites.
- [ ] Executar o contrato gerenciado na CI e verificar o host C++ com
      sanitizers, ASan/UBSan e checagem de ABI ARM64.
- [ ] Testar no Terraria Android: toque curto, toque longo, dois mod IDs,
      grupo de botões, editor de controles, reinício, troca de perfil,
      mundo, multiplayer e estabilidade de FPS.

Gate: nenhum APK e nenhum .bmod são publicados enquanto estes itens
estiverem abertos. Compilar apenas a biblioteca/testes C# não equivale a
funcionamento dos controles dentro do Terraria.
