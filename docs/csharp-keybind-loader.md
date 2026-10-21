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


## Implementação complementar — 20/10/2026

- NativeTouchInput.h/.cpp: registro nativo por identidade, estados em máscara,
  captura de toques curtos, reset por mundo e unload sem reciclar IDs.
- BunnyTouchNativeHost.cs: DllImport direto na libbunny, sem QuickJS.
  BunnyTouchButtonLoader.TryBindNativeHost só ativa a leitura nativa após o
  host informar bunny_csharp_touch_host_ready() == 1.
- NativeTouchGameAbi.h/.cpp: auditoria de assinaturas nativas, layout dos
  arrays Boolean[]/Single[], classes de controles, editor e persistência.
  O código recusa ABI ausente e não instala hooks especulativos.
- Por segurança, host_ready é **0 nesta etapa**. Os métodos nativos estão
  compiláveis e testados, mas os controles do Terraria ainda não são ligados.
- CI no commit e654d2b: https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39514749006
  Sucesso: contrato C#; teste C++ com ASan/UBSan; sintaxe e tipos
  C++ do auditor de ABI. Não foi executado Android/Terraria nem gerado APK.

### Próxima implementação obrigatória

1. Na thread do jogo, validar o resultado real de inspectNativeTouchAbi()
   contra Terraria 1.4.5.8.6 antes de instalar o primeiro hook.
2. Instalar com assinatura exata Controller.Update e HandleTouchCursorDown,
   estender arrays apenas se classes/tamanhos originais forem conferidos.
3. Adicionar o ID de cada botão nativo às tabelas do editor, mantendo ícones,
   nomes localizados, grupos e ações vanilla intactos.
4. Persistir por perfil e SlotId: salvar à parte, remover IDs C# durante a
   gravação vanilla e restaurar os nomes dos mods que voltarem.
5. Só informar host_ready=1 após editor, toque, layouts, flags
   e transições reais funcionarem em conjunto.
6. Após código completo e CI, executar QA Android de todas as operações
   e multiplayer. Nenhum APK intermediário é permitido.

### Não confundir

"Native ABI compile PASS" valida que o C++ compila contra os headers do
repositório. Não valida offsets ou assinaturas no libil2cpp do telefone.
"Managed test PASS" valida lógica de snapshots simulados, não um botão real.


## Lote nativo/editor e perfis — 20/10/2026

- NativeTouchController.h/.cpp: hook OBSERVADOR do método exato
  GUIVirtualInputController.Update(float elapsedTime), instalado uma vez na
  thread do jogo por noteAutoGameUpdate(), apenas depois de conferir
  Controller, arrays e Main.gameMenu. Ação original é preservada.
- Os campos Pressed, WasPressed e Scale são ampliados juntos, apenas quando
  necessário, respeitando o tamanho de cada elemento, com barreira GC.
  O código verifica classe, tamanho, enum ControlType.Count, capacidade máxima
  e método original. Falhas deixam as ações C# desativadas.
- O ciclo de vida da AutoSourceRuntime reinicia os estados de toque ao
  entrar/sair de mundo, sem reciclar IDs da sessão.
- BunnyTouchLayoutStore.cs implementa o esquema JSON compatível com os
  campos version, profiles, slot, index, action e group do loader de
  referência, com captura/restauração sem sobrescrever ações vanilla,
  preservação de mods removidos, limite de dados, serialização e escrita
  atômica. Testes em tests/keybind_loader/LayoutStoreTests.cs.
- Para convivência com o loader JS, cada runtime usa o **seu próprio
  arquivo**: BunnyLoader.touchbuttons.csharp.json. O C# jamais grava
  BunnyLoader.touchbuttons.json, que continua exclusivo dos layouts JS.
- A CI de 8b5076b (https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39519017377)
  aprovou testes gerenciados, testes nativos ASan/UBSan e compilação do
  observador e do auditor de ABI. Isso ainda NÃO é teste in-game.

### Bloqueios restantes e segurança

- O observador ainda não injeta ações no editor nem interpreta botões de
  grupos. A captura real dos slots do Terraria precisa ligar o formato JSON
  gerenciado ao SaveConfiguration/LoadConfiguration, com stripping vanilla.
- HandleTouchCursorDown possui struct Vector2 no ABI ARM64; não se instala
  hook especulativo. Falta validar seu protótipo por metadata e amostras nativas.
- IsBound, chat/placa/NPC em runtime, textura e labels de editor ainda
  não estão ligados à reflexão nativa.
- host_ready segue **false** deliberadamente. O observador da atualização
  não libera a API C# para gameplay até TODO o conjunto estar validado.
- Nenhum APK foi gerado. PR #120 permanece draft.


## Lote adicional — namespaces de ações, editor e ícones (20–21/10/2026)

### Implementação direta no repositório

- NativeTouchActionIds: reserva separada de 512 posições entre os enums vanilla
  e as ações C# para reduzir colisões com o TouchButtonLoader JS existente.
  O offset não é um ID vanilla presumido: é calculado a partir dos três
  enums refletidos no jogo; os limites são validados e há testes C++ dedicados.
  Ainda é necessário confirmar a coexistência com múltiplos mods JS/C# na
  build Android, especialmente quando outros loaders ultrapassarem a reserva.
- NativeTouchEditor: hook de GetControlName para ações C# com cache de
  Il2CppString enraizada em GCHandle, sem alocação em cada desenho, mantendo
  o GetControlName original para os comandos vanilla.
- NativeTouchStaticMappings: extensão das cinco tabelas estáticas
  ButtonStyles, QuickActionControlFromLayoutControl,
  ButtonControlFromLayoutControl, DefaultMode e RemappedMode.
  As cinco são validadas antes de publicar alterações, preservando
  bytes/valores originais e barrando conflitos com ações de outros mods.
  O trabalho de reflexão é limitado a no máximo uma vez a cada 60 updates.
- NativeTouchEditorOptions: adaptador C++ para acrescentar ações C# ativas
  ao PulldownOptionGrid do editor, com validação do enum e proteção das
  opções vanilla. **Ainda não é chamado no gameplay**, pois falta
  concluir a cadeia de input, configuração e renderização antes de expor
  botões selecionáveis.
- NativeTouchIcons: carrega até quatro PNGs por lote de dez updates usando
  ContentAssets e o diretório de mod conhecido pelo loader. Valida até 1 MiB,
  restringe dimensões, mantém asset em GCHandle e impede acesso a disco no
  caminho Draw; usa icon.png como fallback e libera recursos no unload.
- NativeTouchIconHooks: intercepta GetControlTextureFrame e GetIconFrame
  somente depois de validar enums, tipos out e layout da Rectangle.
  Também prepara QuickActionButton.Draw com o overload nativo que recebe
  Texture2D, redirecionando ações C# sem alterar o comportamento vanilla.
  A ativação real no telefone **não foi verificada**.
- BunnyTouchButtonLoader.TryBindNativeHost: registro dos botões é feito
  **antes** de exigir host_ready. A ação permanece indisponível para
  gameplay enquanto os hooks não estiverem completos; chamadas posteriores
  podem finalizar a ligação sem travar a inicialização.
- A expansão de arrays do Controller ganhou raízes temporárias de GCHandle
  durante a alocação de Pressed/WasPressed/Scale, evitando que objetos
  temporários sejam perdidos antes da barreira de escrita.
- CI executa .NET 10, C++20 com -Wall -Wextra -Werror, ASan/UBSan,
  testes de IDs e compilação sintática de cada nova unidade.
  Não compila APK nem prova a ABI real do Android.

### Por que a PR ainda não é release

- Falta interceptar HandleTouchCursorDown com a ABI AArch64 correta de
  Vector2/Cursor e resolver a ação por SlotId do perfil ativo.
- Falta interpretar os grupos virtuais, saída do toque e visibilidade,
  inclusive na transição entre chat, placa e diálogo.
- Falta ligar NativeTouchEditorOptions ao editor somente quando todos
  os caminhos de input e draw forem seguros.
- Falta conectar BunnyTouchLayoutStore ao SaveConfiguration,
  LoadConfiguration e GetShareCode do Terraria, impedindo que IDs C#
  sejam gravados diretamente no arquivo vanilla.
- Falta leitura real de IsBound, controles atribuídos e atualização
  MonoVM ao longo do frame, incluindo o lifecycle dos mods C#.
- Necessário validar as assinaturas IL2CPP no Terraria 1.4.5.8.6,
  os assets dos mods C#, retorno dos mods, múltiplos mundos,
  multiplayer e FPS sustentado.

**Segurança:** bunny_csharp_touch_host_ready() permanece zero.
A PR continua draft; nenhum APK ou pacote de mod foi produzido.


## Retomada de 21/10/2026 — sincronização de perfis e preflight

- NativeTouchProfiles reutiliza a instância do perfil e a matriz Controls
  ao construir IsBound/actionSlots. O scan não repete descoberta IL2CPP
  do perfil em cada slot.
- NativeTouchController invalida o snapshot ao sair para o menu e na
  mudança de identidade do perfil ativo. Alterações internas no mesmo
  perfil continuam com rechecagem limitada a 30 updates.
- AutoSourceRuntime ganhou declaração antecipada de
  compiledSourceHasTouchEntry, necessária para compilar a integração com
  o worker MonoVM.
- Source-only CI passou em 39603907421: contratos .NET, ASan/UBSan,
  compilação dos hooks e integração nativa do worker. Não houve teste
  real no Terraria Android, nem pacote APK ou .bmod nesta etapa.

Pendências: validar ABI IL2CPP ARM64 em dispositivo, ciclos reais de
pressionar/soltar, editor com imagens e grupos, layout ao reiniciar e trocar
perfil, coexistência JS/C#, multiplayer, save/load/share e estabilidade
sustentada a 60 FPS. A PR permanece draft; nenhuma build de gameplay
deve ser publicada antes da validação completa.


## Lote de 21/10/2026 — gatilho tardio e leitura nativa em lote

- `BunnyTouchButtonLoader.BeginTriggerFrame` tenta finalizar a ponte com o host
  antes de formar o snapshot que cada mod lê em `ProcessTriggers`; os casos
  sem libbunny mantêm o caminho gerenciado de teste.
- `ReleaseAll` limpa estado gerenciado, snapshots e sequência de pressão.
- `NativeTouchPointer` não insere toques no registro de C# antes de
  `hostReady`. Botões vanilla continuam no comportamento original.
- `NativeTouchFocus` refaz descoberta de campos transitórios no máximo
  uma vez a cada 120 chamadas após falha; não reflete cada frame.
- `Registry::snapshot` entrega estados e press tokens sob uma única trava,
  sem misturar ticks entre botões. A ponte C# usa um único P/Invoke por tick,
  no lugar de duas consultas por ação, reaproveitando arrays de 128 posições;
  conserva o caminho individual para host sem o símbolo novo.
- CI source-only: <https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39614373254>.
  Contrato gerenciado e nativo aprovados, com ASan/UBSan e syntax-preflight.

Continua faltando validar input, editor, grupos, restauração real,
multi-mod, troca de perfil e FPS no Terraria Android ARM64. Os testes de CI
não validam os offsets/ABI do dispositivo. Nenhum APK ou .bmod foi gerado.
