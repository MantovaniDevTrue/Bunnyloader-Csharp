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


## Lote de 21/10/2026 — grupos e persistência reforçada

- `nativeTouchActiveSlotCount` usa o número real de posições do perfil
  atual para escanear grupos; deixa de sondar todas as 256 posições fixas.
- NativeTouchGroups pode refazer uma descoberta transitória a cada 120
  atualizações e limpa seu estado ao trocar de mundo; bloqueia eventos antes
  de `hostReady`, assim como os botões individuais.
- `saveTouchLayoutFile` recusa destino não regular e utiliza
  `O_NOFOLLOW` no arquivo temporário antes de `fsync` e `rename`.
  Testes C++ garantem que symlinks de destino/temporário não sobrescrevem
  arquivos externos.
- `NativeTouchPointer` não encaminha IDs C# reservados, descarregados ou
  sem arrays válidos ao manipulador original vanilla. Ações vanilla
  não reservadas seguem delegadas como antes.
- Testes source-only, ainda sem execução no telefone:
  <https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39622647781>.

A disponibilidade real dos grupos, editor, perfis e operações de
save/load/share continua pendente de QA no Terraria Android ARM64.
Nenhum APK/.bmod gerado nesta PR.


## Lote de 21/10/2026 — restauração de mods C# carregados depois

- O registro nativo passou a manter uma versão monotônica de adições e
  descarregamentos, distinta do número de ações ativas. IDs de sessão não
  são reutilizados. Registros repetidos, alterações de política e reset
  de mundo não incrementam a versão.
- A restauração dos controles lembra a última versão processada. Quando
  o carregamento inicial ocorreu sem todos os mods C#, a inclusão posterior
  de um mod refaz somente a ligação de seus slots salvos que ainda estejam
  vazios; não sobrescreve ações vanilla do jogador.
- Após ativação, o editor compara a versão do registro com a última versão
  sincronizada e, quando necessário, atualiza opções e bindings. A tentativa
  é limitada a uma vez a cada 30 GameUpdates enquanto houver mudança,
  sem scans adicionais durante toques ou desenhos.
- Testes native-contract verificam versionamento sob carregamento,
  descarregamento duplicado, idempotência e mudança de mundo.
- [CI #39623648747](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39623648747):
  native-contract e managed-contract aprovados, além dos preflights
  nativos do próprio workflow. Ainda não é validação de ABI ARM64
  nem de salvamento, editor ou FPS no Terraria real.

PR continua draft. Sem merge, sem APK e sem .bmod até o loader estar
completo e passar no dispositivo.


## Lote de 21/10/2026 — segurança do save e manutenção do editor

- `SaveConfiguration` não grava IDs reservados do mod no arquivo vanilla se
  não puder enumerar os layouts, carregar o JSON separado ou concluir sua
  escrita. Em vez de apagar ou sobrescrever dados antigos, recusa a operação
  e registra o motivo no log.
- `GetShareCode` também não exporta uma configuração cuja enumeração não
  pode ser validada. Após a execução guardada das funções originais, os
  bindings do layout são restaurados.
- `storedTouchActionIdentity` mantém nomes de controles de mods ausentes
  quando ainda ocupam um índice reservado ou foram convertidos em unbound.
  Uma nova ação vanilla ou um botão removido manualmente não reaproveita
  essa identidade indevidamente.
- `NativeTouchEditorOptions` pode ajustar ItemCount e ItemLineCount da
  grade mesmo quando a lista ButtonControls não mudou. Evita criar outro
  array gerenciado nesse caso.
- Com o host ativo, o editor verifica a integridade das opções no máximo
  uma vez a cada 60 updates, para reparar a recriação nativa do seletor.
  A restauração dos bindings continua condicionada à revisão do registry.
- Regression tests incluem identidade preservada, remoção explícita,
  respeito a ações vanilla e rejeição de identificador incompatível.
- CI source-only <https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39625635908>
  aprovada (contratos C# e C++, ASan/UBSan e preflight IL2CPP).
  O preflight Linux agora fornece o stub do android/log.h para compilar
  diagnósticos usados no Android real, sem fabricar APK.

**Limite de validação:** os testes não executam os hooks no Terraria.
Ainda faltam QA ARM64 no aparelho, chat/NPC/placas, editor e imagens,
grupos, perfis, convivência JS/C#, multiplayer e FPS sustentados.
A PR permanece em draft, sem merge nem APK/.bmod.


## Lote de 21/10/2026 — verificação das assinaturas ARM64 e desenho

- `NativeTouchIconHooks` passa a verificar as formas de chamada nativas
  dos dois métodos `GUIQuickActionButton.Draw`, além da descoberta textual:
  enum QuickActionControl, referência QuickActionButton_Layout, bool, float
  por referência, enum InteractionState, retorno void e Texture2D. Falhas
  de metadata mantêm o desenho C# inativo em vez de instalar função ABI
  não verificada.
- A descoberta de metadados de `HandleTouchCursorDown`, desenhos,
  nome de controle e visibilidade tenta novamente em intervalo limitado
  de 120 atualizações. Após uma tentativa de instalar um hook, não ocorre
  reinstalação automática insegura.
- O enum usado em `DetermineButtonVisible` verifica também o tipo base
  System.Int32 antes de chamar a função original.
- `NativeTouchActivation` registra uma única linha com estados dos hooks
  se continuar bloqueado ao completar 600 updates. Isso ajuda a diagnosticar
  qual peça impediu a ativação no celular, sem logs por frame.
- Novo teste source-only `check_native_touch_signatures.py` verifica
  a igualdade textual de 11 assinaturas entre as fontes do
  `TouchButtonHooks.js` (ExampleModBunny) e a implementação C++ nativa.
- CI <https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39627083185>:
  managed-contract e native-contract aprovados, incluindo teste de paridade,
  ASan/UBSan e preflight C++20.

O teste de paridade compara assinaturas declaradas nas fontes, não a
ABI efetiva do jogo. Ainda é necessário validar o hook com IL2CPP
real em ARM64, imagem de ícones, grupos, salvamento/compartilhamento,
coexistência JS/C#, FPS e dispositivos. PR draft, sem merge ou APK/.bmod.


## Lote de 21/10/2026 — cache dos grupos e ícones

- `NativeTouchIconHooks` usa a mesma prioridade de estados do
  `TouchButtonHooks.js` do Bunny Loader: pressionado, normal,
  desabilitado; o efeito de pressão não desaparece no desenho nativo.
- A varredura de grupos continua limitada a 30 atualizações ou mudança
  imediata da identidade do perfil, mas compara SlotId e 16 ações antes de
  invalidar os GCHandles. Strings IL2CPP e handles são reutilizados enquanto
  o grupo permanece igual; ao trocar de perfil ou alterar botões, o cache
  é reconstruído. Isso evita churn de memória gerenciada a cada scan.
- `GroupStates.ContainsKey` e `get_Item` validam o argumento
  System.String e o retorno booleano; o tipo e offset Int32 de
  `pressedIndex` são descobertos apenas quando a classe muda.
  Nenhuma nova reflexão é executada durante Draw.
- `Registry::iconSources` lê todas as fontes de ícone com uma trava
  compartilhada em vez de chamar individualmente `ownerId` e
  `iconPath` 256 vezes a cada varredura. O gerenciador de ícones
  percorre apenas a quantidade monotônica de IDs alocados;
  IDs descarregados retornam origem vazia e têm o recurso liberado.
- Testes nativos verificam o snapshot de nomes e paths em dois mods,
  descarregamento e retorno sem reutilização do ID. Preservados PNG
  originais e fallback ao ícone do mod quando o arquivo específico faltar.
- [CI #39629077419](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39629077419):
  `native-contract` e `managed-contract` passaram, com ASan/UBSan,
  compile-only do Terraria ABI e teste de assinaturas JS/C++.

Os testes source-only comprovam compilação e contratos, não desenho nem
performance reais. A confirmação de ARM64 e 60 FPS requer a execução
do Terraria Android e validação visual do editor, grupos e sprites.
Não houve APK, .bmod ou merge; PR continua em draft.


## Lote de 21/10/2026 — vida útil de texturas e seleção segura dos grupos

- `NativeTouchIcons` mantém handles GC separados para o
  `Asset<Texture2D>` e para o `Texture2D` do Terraria; depois do
  carregamento, `Draw` obtém diretamente a textura pelo segundo handle,
  sem resolver o valor a partir do asset a cada desenho.
- Os dois handles são liberados quando o mod é descarregado, o caminho
  muda ou o recurso precisa ser descartado. O descarregamento da textura
  Unity continua no caminho existente `destroyTextureAsset`.
- A mudança de composição do registro C# provoca uma atualização imediata
  do cache de ícones no próximo pump da thread de jogo, em vez de esperar
  pelas 10 atualizações periódicas. As atualizações periódicas continuam
  existindo para recuperar imagens ainda indisponíveis.
- O leitor `GroupStates` distingue falha de acesso/reflexão (`-2`) de
  grupo validamente fechado (`-1`). Somente uma transição entre índice
  válido anterior e estado atual `-1` conta como seleção. Os grupos
  não disparam em caso de erro de metadata, ausência de tabela ou
  exceção ao consultar o dicionário.
- O teste `native_touch_group_selection_test.cpp` cobre seleção normal,
  centro, índice fora da faixa, grupo vazio, manutenção da seleção e
  falhas nativas, com AddressSanitizer/UndefinedBehaviorSanitizer.
- [CI source-only #39630503570](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39630503570):
  `managed-contract` e `native-contract` aprovados; verificação de
  assinaturas, preflight dos hooks e testes nativos concluídos.

Esta CI não executa o Terraria nem mede FPS. A validação da ABI IL2CPP
ARM64 real, editor visual, toque/soltura, grupos, descarregamento de PNG,
perfis persistidos, coexistência com JS, multiplayer e desempenho ainda
precisa ser feita em dispositivo antes de publicar. PR draft, sem
merge, APK ou .bmod.


## Lote de 21/10/2026 — troca de mundo, perfil e estado de toque

- `GUIVirtualInputController.Update` lê o estado do menu antes e depois
  da chamada original do Terraria. O grupo só interpreta uma seleção se
  ambos os estados estiverem em jogo; transições para/de menu não
  produzem toque C#.
- `NativeTouchGroups` conserva a descoberta da ABI mesmo no menu,
  mas não percorre os perfis nem lê GroupStates até voltar ao jogo.
  O primeiro frame de gameplay refaz o cache dos grupos.
- A seleção de um grupo é descartada se o perfil ativo mudar durante o
  mesmo Update; as amostras anterior e posterior pertencem ao mesmo
  perfil somente quando a identidade coincide.
- Quando os arrays Pressed/WasPressed/Scale ficam temporariamente
  inacessíveis, o registry registra um frame sem entrada, liberando
  os estados mantidos e cancelando toques pendentes.
- A restauração de bindings agora compara versão do registry, identidade
  do perfil e sinal de invalidação por mudança de mundo, no lugar de
  depender apenas do número de registros C#.
- `LoadConfiguration` atualiza essa identidade e os dados capturados,
  enquanto a entrada/saída de mundos marca a restauração como pendente.
  O perfil ativo recebe o layout salvo no primeiro Update que detecta
  sua mudança; o editor mantém a revisão de segurança a cada 60 updates.
- O teste `native_touch_layout_refresh_test.cpp` cobre composição
  inalterada, nova versão, troca de perfil, perfil ausente e invalidação
  por novo mundo. O contrato do Registry cobre menu, falta de entrada e
  retomada sem disparos pendentes.
- [CI source-only #39639806047](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39639806047):
  contratos gerenciado/nativo, ASan/UBSan, teste de assinaturas e preflight
  C++20 aprovados.

Ainda faltam os testes reais de troca de perfil, carregamento de mundos,
toque/soltura, editor, grupos, compatibilidade JS/C#, multiplayer e FPS
em Terraria Android ARM64. Não foi criado APK nem .bmod e a PR segue
draft sem merge.
