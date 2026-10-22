# GoreLoader C# nativo — execução por etapas, sem APK

Integração atual: `feat/csharp-loader-completion-15-20261021`, sobre `5a18a678`.
O histórico da PR #121 foi preservado. Estado: implementado em fonte,
testado automaticamente; aguardando CI desta integração e QA Android.
Esta implementação **não substitui o GoreLoader JavaScript** e não usa APIs do tModLoader.
Os PNGs presentes em `samples/ExampleModCSharp/Playable/content/Content/Gores/`
foram reaproveitados integralmente do ExampleModBunny do próprio repositório.

## Contrato de fidelidade

O [GoreLoader.js](../../app/src/main/cpp/script/js/mod/Loaders/GoreLoader.js)
reconhece todo `*.png` em uma pasta `Gores/` do conteúdo do mod, incluindo
`Content/NPCs/Gores/` e `Assets/Textures/Gores/`. O tipo é indexado pelo
nome do PNG sem extensão e **pertence ao mod**. O jogo usa
`TextureAssets.Gore[type]`, `GoreID.Sets.SpecialAI`,
`DisappearSpeed`, `DisappearSpeedAlpha`, `IsDrip` e
`ChildSafety.SafeGore`. O efeito real é criado pelo método nativo
`Terraria.Gore.NewGore(Vector2, Vector2, int, float)`, na thread do jogo.

A versão C# precisa entregar exatamente essa funcionalidade, sem
renderização falsa, sem despachar a execução por QuickJS, sem varredura por
quadro e sem exigir que o modder informe um tipo numérico global.

## Implementação de fontes nesta branch (sem APK)

- `ManagedGoreDiscovery.h/.cpp`: visita os arquivos **uma vez no
  carregamento**, em ordem determinística. Rejeita links simbólicos,
  caminhos externos, duplicatas e cabeçalhos PNG inválidos, com limites
  de diretórios, quantidade de gores, dimensão e bytes.
- `SourceLoader.cpp`: executa a descoberta, registra os demais conteúdos
  e entrega as gores ao registrador IL2CPP; falhas interrompem a carga
  com diagnóstico por mod. O campo `goreSourcesDiscovered` representa os
  PNGs encontrados; o registro só é considerado sucesso quando
  `gore::install` conclui a instalação real.
- `ManagedGoreCatalog.h`: reserva IDs por `(mod, nome)`, aceita gores
  homônimos entre mods e proíbe realocar IDs durante a sessão.
- `ManagedGoreRuntime.cpp`: redimensiona as seis tabelas de Terraria
  por cópia com validação de tipos, protege os arrays e sprites no GC,
  decodifica os PNGs por bytes já verificados e publica os IDs ao fim do
  lote. A chamada `Gore.NewGore` roda apenas na thread do jogo.
- `BunnyGore.cs`: expõe `GetType`, `GetTypeAny`, `TrySpawn` e `TryConfigure`
  para o modder, com P/Invoke e parâmetros tipados. Fila limitada a
  128 pedidos, 32 spawns processados por quadro; nenhuma VM JavaScript.
- `AutoSourceRuntime.cpp`: ativa e pausa os pedidos nas transições
  de mundo, descartando spawns antigos. Repara arrays reconstruídos
  sem reler todos os arquivos por quadro.
- `ExampleGoreSlime` é o exemplo independente: configura os fragmentos
  originais Front/Back no spawn e emite ambos no hook de morte. Tem textura,
  atributos, animação e física nativa de slime, com comentários sobre cada
  operação. Os exemplos existentes de invasões e armas continuam preservados.
- `tests/csharp_gore_catalog_test.cpp`: testa a separacao entre mods,
  falhas de capacidade, indices estaveis e limpeza do catalogo.
- `tests/csharp_gore_discovery_test.cpp`: testa subpastas, nomes,
  ordenação, dimensões, PNG inválido, duplicatas, link simbólico,
  limite por mod e os PNGs originais do ExampleMod C#.
- `.github/workflows/csharp-gore-source-check.yml`: compilação C++20
  e teste de host, expressamente **sem build de APK**.

A descoberta confere a assinatura/IHDR, tamanho e limites do PNG;
`readOwnedPng` lê o arquivo novamente por `openat(O_NOFOLLOW)` em todos
os componentes, protegendo contra troca por symlink. A instalação então
decodifica os bytes seguros com `content::loadTextureAsset` e confere as
dimensões reais. A compilação de sintaxe C++ e o SDK/examples C#
foram verificados no GitHub Actions; isso ainda não valida IL2CPP Android.

## Contratos implementados que ainda exigem QA Android

1. **Registro nativo — fonte implementada:** validar no Android o
   endereço e os layouts IL2CPP das seis tabelas antes de considerar o
   contrato de registro confirmado. O registro rejeita APIs ausentes.
2. **Arrays e GC — fonte implementada:** conferir em jogo a extensão,
   retenção de texturas, `SpecialAI`, `DisappearSpeed`,
   `DisappearSpeedAlpha`, `IsDrip`, `SafeGore` e regeneração de
   arrays após mudança de mundo. Não falsificar tipo/sprite.
3. **Spawn de verdade — fonte implementada:** confirmar Android
   `NewGore`, trajetória, desaparecimento e escala; filas aceitam
   apenas tipos existentes e números finitos e descartam sessão antiga.
4. **Ciclo de vida:** a fila é descartada na saída/entrada de mundo,
   mas as texturas registradas permanecem carregadas até encerrar a
   sessão do processo. Desabilitação/reinstalação de mods no mesmo
   processo exige QA específico antes de declarar hot-unload suportado.
5. **Exemplos C# — fontes implementadas e compiladas:** verificar
   os dois fragmentos do `ExampleGoreSlime.OnKill`, suas propriedades,
   configuração e posicionamento durante teste de jogo. Os PNGs foram copiados sem
   alteração do exemplo original.
6. **Validação:** testes de host (descoberta e catálogo), análise de
   sintaxe nativa C++20, build do SDK net8.0 e compilação dos
   exemplos tiveram verificações na PR original. Ainda precisa QA Android real: multimoD,
   reinício, sprites, física e estabilidade de FPS.

**Bloqueio de APK:** esta integração segue o escopo de quinze loaders em
`docs/csharp-loader-completion-15.json`. Nenhum APK pode ser gerado enquanto
houver contratos pendentes. Não publicar como lote completo ou release estável.

## Complementos desta integração

- Os campos de arrays têm storage, rank, tipo declarado, tipo real e tamanho
  do elemento verificados antes de qualquer leitura ou escrita. A assinatura
  de NewGore e o layout de Vector2 também são verificados; X/Y podem ter
  offsets invertidos ou incluir cabeçalho boxed e ainda são codificados corretamente.
- As seis tabelas são publicadas apenas após preparar também o índice e a
  propriedade dos recursos C++. Falha de textura, alocação ou raiz GC preserva
  o lote anterior. A última publicação mantém seis raízes, trocadas na próxima.
- Registro idêntico é idempotente, sem novo ID. Mudança estrutural de um pacote
  já instalado exige reinício do processo; hot-unload de Gore continua fora do
  contrato original. Isso não é anunciado como recarga dinâmica suportada.
- `BunnyNPC.ModId` e `BunnyProjectile.ModId` recebem o UID real antes dos defaults.
  O dispatcher executa os defaults e conserva posição/velocidade/identidade
  do último snapshot para o hook de morte. O exemplo não depende de slug fixo.
- Velocidades de desaparecimento usam float no novo SDK; o export v2 recusa
  frações quando a tabela real é Int32/Byte, sem truncar. O export inteiro
  anterior é mantido para os assemblies anteriores da PR. `new BunnyGoreSettings()`
  conserva as duas velocidades 1. `default(BunnyGoreSettings)` tem zeros, como
  qualquer struct C#; use o construtor para obter os defaults do exemplo.
- `BunnyGore.LastError` preserva a recusa nativa na thread chamadora. Há contexto
  de mod/recurso, estágio, valor e motivo; mensagens repetidas são limitadas.
- O teste de integração usa o **serviço C++ de produção** numa biblioteca host,
  chama-o por P/Invoke e executa o NPC do próprio ExampleMod. Metadata IL2CPP e
  criação de textura são fixtures: esse teste não substitui a execução Android.
- Não foram mesclados os binários antigos da PR sobre o SDK atual. O workflow
  recompila SDK/compilador dos fontes combinados e confere os exports nativos.

Execute `bash tests/gore/run.sh`: descoberta e catálogo, transações/GC/layout,
recriação de tabelas, fila limitada, fronteiras de mundo, dois UIDs e callbacks
C# reais do exemplo. Não inicializa QuickJS e não gera APK.

## Procedimento Android, quando o escopo permitir APK

1. Ative o ExampleMod C# atualizado e invoque **Example Gore Slime** pelo catálogo
   de NPCs do Bunny. Confirme nome, sprite, dois frames e movimento de slime.
2. Derrote-o em posições diferentes. Devem sair dois fragmentos do ExampleMod,
   em sentidos opostos, a partir do centro do NPC; devem cair e desaparecer.
3. Repita com outros mods ativos. Verifique o relatório por `GoreLoader`, erros
   de tipos, callback ou asset e observe o tempo de quadro no aparelho.
4. Saia com fragmentos ativos e entre em outro mundo: pedidos antigos não devem
   nascer na nova sessão. Retorne ao mundo e repita o spawn/morte do exemplo.
5. Verifique também invasões e armas do ExampleMod como regressão. Os testes
   de host não certificam a física, o filtro infantil ou o desempenho no Xiaomi.

## Executar os testes desta etapa (somente fonte)

Na raiz do repositório:

```bash
c++ -std=c++20 -O2 -Wall -Wextra -Werror -pedantic \
  -Iapp/src/main/cpp \
  tests/csharp_gore_discovery_test.cpp \
  app/src/main/cpp/experiments/csharp/ManagedGoreDiscovery.cpp \
  -o /tmp/bunny_gore_discovery_test
/tmp/bunny_gore_discovery_test
c++ -std=c++20 -O2 -Wall -Wextra -Werror -pedantic \
  -Iapp/src/main/cpp \
  tests/csharp_gore_catalog_test.cpp \
  -o /tmp/bunny_gore_catalog_test
/tmp/bunny_gore_catalog_test
```

Saídas esperadas: `CSHARP_GORE_DISCOVERY_PASS` e
`CSHARP_GORE_CATALOG_PASS`. Isso valida somente a descoberta e o catalogo;
nao prova o registro IL2CPP ou os efeitos no jogo.

## Auditoria de codigo e teste nativo ARM64

- Os mods C# que têm apenas PNGs de Gore, mais seu ponto de entrada
  `main.cs`, agora passam pelo contador de conteúdo do SourceLoader.
- A restauração compara a **identidade** das seis tabelas de Gore:
  uma substituição por um array de comprimento igual também reaplica
  sprites e configurações. A cópia preserva registros de outros mods.
- As opções pendentes não são retiradas da fila quando a restauração
  falha; serão reaplicadas na próxima atualização válida. O limite de
  fila permanece fixo.
- O workflow `csharp-gore-source-check.yml` executa rapidamente
  os testes de descoberta/catálogo, a análise de sintaxe nativa e a
  compilação do SDK C# e exemplos sem o custo de instalar o NDK.
- O workflow `csharp-gore-native-link-no-apk.yml` realiza a
  **compilação ARM64 e linkagem de toda a libbunny.so** com o NDK
  `30.0.16248370` e CMake `3.22.1` fixados pelo projeto, chamando
  somente `:app:externalNativeBuildDebug`. Verifica as exportações
  P/Invoke `bunny_csharp_gore_type`, `_type_any`, `_spawn`
  e `_configure`. Não chama `assembleDebug` ou
  `assembleRelease`, e proíbe saída APK.
- A aprovação do compilador e da linkagem **não comprova** que os
  métodos IL2CPP e texturas funcionem no Terraria; isso requer QA real.

A biblioteca nativa deve respeitar o ciclo de vida já usado pelo
GoreLoader JavaScript: registros pertencem à sessão do processo e
a troca de mundo limpa os spawns pendentes. Hot-unload de um mod
individual no meio da sessão não é um contrato suportado pelo
GoreLoader original e não deve ser anunciado como funcional.

## Artefato SDK sem APK

O workflow `.github/workflows/stage-csharp-gore-sdk-no-apk.yml`
compila o `BunnySourceSdk.dll` puro, verifica os exemplos, atualiza apenas
`app/src/main/assets/managed/BunnySourceSdk.dll` e a entrada correspondente
em `managed/framework.sha256`. Não invoca Gradle ou gera APK.
