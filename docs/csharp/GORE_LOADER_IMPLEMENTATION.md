# GoreLoader C# nativo — execução por etapas, sem APK

Base: `v1.0` (código-fonte Bunny Loader). Branch de desenvolvimento isolada.
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
- `BunnyGore.cs`: expõe `GetType`, `TrySpawn` e `TryConfigure`
  para o modder, com P/Invoke e parâmetros tipados. Fila limitada a
  128 pedidos, 32 spawns processados por quadro; nenhuma VM JavaScript.
- `AutoSourceRuntime.cpp`: ativa e pausa os pedidos nas transições
  de mundo, descartando spawns antigos. Repara arrays reconstruídos
  sem reler todos os arquivos por quadro.
- O exemplo C# tem uma gore original do ExampleModBunny no
  `ExampleSlimeNPC.OnKill` e outra no `ExampleWaveBolt.OnKill`,
  incluindo a configuração de drip com explicações no código.
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
   os dois efeitos reais `OnKill` e a configuração `IsDrip` no
   ExampleMod C# durante teste de jogo. Os PNGs foram copiados sem
   alteração do exemplo original.
6. **Validação:** testes de host (descoberta e catálogo), análise de
   sintaxe nativa C++20, build do SDK net8.0 e compilação dos dois
   exemplos passaram em CI. Ainda precisa QA Android real: multimoD,
   reinício, sprites, física e estabilidade de FPS.

**Bloqueio de APK:** a primeira compilacao Android so pode ocorrer **depois**
de finalizar o registro, a API de spawn, os exemplos e os testes estaticos.
O APK subsequente sera exclusivamente para QA no aparelho, nao uma release.
Sem merge nem versao publica antes de validar o QA real.

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

## Artefato SDK sem APK

O workflow `.github/workflows/stage-csharp-gore-sdk-no-apk.yml`
compila o `BunnySourceSdk.dll` puro, verifica os exemplos, atualiza apenas
`app/src/main/assets/managed/BunnySourceSdk.dll` e a entrada correspondente
em `managed/framework.sha256`. Não invoca Gradle ou gera APK.
