# CloudLoader C# nativo — desenvolvimento isolado, SEM APK

Base do lote: `v1.0`, commit `cfbe784ee3cbc0ba2fabafce56c59e6eac41b67b`.
Branch: `feature/csharp-cloudloader-source-only-20261021`.
**Estado: seleção, registro, contratos C#, dispatcher de callbacks, parser e extração Roslyn codificados; hooks do Terraria ainda NÃO integrados.**

## Fontes de referência obrigatórias

Somente Bunny Loader / ExampleModBunny, sem copiar tModLoader:

- `app/src/main/cpp/script/js/mod/Loaders/CloudLoader.js`
- `app/src/main/cpp/script/js/mod/ModCloud.js`
- `samples/ExampleMod/content/Content/Clouds/ExampleAdvancedCloud.js`
- `samples/ExampleMod/content/Assets/Textures/Clouds/` (verificar caminho no pacote atual)

A implementação C# deve usar MonoVM e o núcleo C++ nativo, sem qualquer execução
indireta de código JavaScript. O ExampleModBunny é usado apenas para determinar
o comportamento correto e suas regras já validadas.

## O que já foi codificado

- `NativeCloudSelection.h`: seleção ponderada sem heap no sorteio, limites
  de peso e quantidade, peso vanilla de 22 nuvens comuns / 18 raras / 15
  raras em mundo de aniversário, janela de 60 tentativas, prioridade da
  rolagem rara e bloqueio de tipo sem máscara.
- `NativeCloudRegistry.h`: identidade por mod e classe, IDs desde o
  `CloudID.Count` fornecido pelo Terraria, rejeição de caminhos perigosos,
  segurança transacional de disponibilidade de textura/máscara e limpeza.
- `BunnyCloudDispatcher.cs`: registro C# por assembly, vínculo de IDs nativos, peso por bioma, `OnSpawn`, isolamento de exceções, várias instâncias e descarregamento.
- `NativeCloudManifest.h`: leitura estrita e transacional do protocolo
  `BUNNY_CLOUDS_V1` gerado pelo Roslyn. Duplicações ou entradas inválidas
  não deixam classes parcialmente registradas.
- `NativeCloudAssets.cpp`: instalação nativa **ainda não chamada pelo loader**.
  Prepara PNGs, duas tabelas IL2CPP separadas, máscaras do horizonte e
  seus handles GC. Publica máscaras antes das texturas e reverte ambas
  se qualquer verificação falhar. Rejeita IDs já ocupados por outro loader.
- `NativeCloudSpawnLedger.h`: fila limitada de eventos `OnSpawn` para
  MonoVM, com gerações por slot; bloqueia resultados atrasados depois
  que o Terraria reutiliza um slot ou troca de mundo.
- `NativeCloudHookBindings.cpp`: valida em tempo de execução as assinaturas
  `void addCloud()` e `int RollRareCloud()`, offsets tipados e
  `Main.cloud`, mas **ainda não instala hooks**.

- `SourceParser.cpp` / `SourceContentIdentity.h`: reconhecimento explícito de `BunnyCloud` e domínio `CLOUD`; `SourceLoader.cpp` recusa nuvens antes de registrar itens enquanto não houver máscaras e hooks nativos.
- `CompiledAssemblyProbe.cs`: reconhecimento por reflexão e geração do catálogo isolado `source-clouds-v1.txt`, sem alterar o formato V2 dos demais itens.
- `tests/cloud_loader/`: exemplos executáveis das duas classes de nuvens,
  casos de PNG ausente, peso inválido, duplicação, mundo de aniversário,
  isolamento de `OnSpawn`, vazamento de estado entre tentativas e tamanho
  do array de máscaras.
- `.github/workflows/check-csharp-cloudloader-source.yml`: **somente**
  testes de código-fonte C++ estritos e sanitizers; sem Gradle, APK ou `.bmod`.

Os testes do núcleo e dos exemplos C# NÃO provam renderização no Terraria Android.

## API C# compilável e exemplos no código

Foi adicionado `experiments/csharp/BunnyCsSdk/BunnyCloud.cs` ao
`BunnySourceSdk.csproj`. Ele define o contrato **experimental**:
`BunnyCloud`, `BunnyCloudEnvironment`, `BunnyCloudSpawn` e
`BunnyCloudSpriteEffects`. A chance é normalizada por
`BunnyCloud.SafeSpawnChance`; nomes de biomas são identificadores internos,
não nomes traduzidos.

Duas implementações comentadas e testes compiláveis estão em:

- `tests/cloud_loader/managed_examples/ExampleCommonCloud.cs`:
  nuvem comum de peso 1, como o exemplo de textura do Bunny original.
- `tests/cloud_loader/managed_examples/ExampleAdvancedCloud.cs`:
  nuvem rara, peso 10 dentro do bioma do exemplo, 1 fora, `OnSpawn`
  removendo o espelhamento de seu PNG com letras.
- `tests/cloud_loader/managed_examples/CloudContractTests.cs`:
  testes das chances, contextos, parâmetros, proteção de desenho e tipo.
- `tests/cloud_loader/CloudContractTests.csproj`: testes .NET 8 do dispatcher e da inspeção real do compilador Roslyn, **sem build de Android**.
- `tests/cloud_loader/source_parser_cloud_test.cpp`: teste do parser com classe C# real e sem hooks de Terraria.

A estrutura original do Bunny Loader é usada como comportamento de referência.
**As classes já são reconhecidas pelo parser e compilador, mas ainda NÃO são registradas como nuvens visíveis no Terraria.** Seus testes não comprovam carregamento no jogo.
O bioma `ExampleModCSharp/ExampleSurfaceBiome` é uma chave de referência
para o teste, não um bioma já implementado no C#.

```bash
dotnet run --project tests/cloud_loader/CloudContractTests.csproj -c Release
dotnet build experiments/csharp/BunnySourceSdk/BunnySourceSdk.csproj -c Release
```

## Bloqueio de ativação confirmado

A checagem `SourceLoader.cpp` retorna
`Reason=NativeCloudHooksAndMasksNotInstalled` para qualquer mod C# que
declare uma classe `BunnyCloud`. O compilador já pode descobrir as
classes e gerar o manifesto `source-clouds-v1.txt` em testes isolados,
mas o runtime nunca deve anunciá-las como conteúdo registrado.

Isto é intencional: o céu mobile exige máscaras válidas em
`TextureMaskManager.CloudMasks` para cada tipo, e a ausência desse
passo pode causar interrupção do renderizador. O manifesto é apenas
metadado, não uma ponte para QuickJS.

## O que falta para finalizar o CloudLoader inteiro

- [x] Criar `BunnyCloud` experimental, contrato gerenciado, exemplos
      anotados e testes de compilação C# no BunnySourceSdk.
- [x] Compilar SDK, exemplos C#, código C++ e o manifesto do Roslyn no CI sem APK.
- [ ] Estabilizar a API com o host nativo e validar em aparelho antes de classificá-la como utilizável em jogo.
- [x] Reconhecer `BunnyCloud` no parser, reservar domínio `CLOUD` e exportar catálogo Roslyn separado.
- [ ] Completar registro nativo real e instalação automática de texturas e máscaras antes da entrada no mundo.
- [x] Codificar instalador transacional nativo e verificação de máscaras,
      com rollback e preservação das tabelas vanilla.
- [ ] Acionar o instalador no momento correto do ciclo de vida do jogo
      e validar as classes e máscaras reais em Android; na falha,
      bloquear aquele tipo sem afetar nuvens vanilla.
- [x] Codificar a resolução estrita das assinaturas IL2CPP e offsets tipados.
- [ ] Validar esses contratos contra Terraria Mobile em execução e
      instalar os hooks preservando a rolagem vanilla.
- [x] Implementar dispatch C# de `SpawnChance` e `OnSpawn` com identificação por mod, limites e isolamento de exceções.
- [ ] Conectar o dispatcher ao worker MonoVM e ao hook IL2CPP, com cache e nenhuma RPC por frame.
- [x] Preparar fila `OnSpawn` por slot/geração com limites e testes
      para ignorar resultados de callbacks atrasados.
- [ ] Atualizar o tamanho real pelo PNG, preservar `spriteEffects`,
      impedir callback em slot inativo e ligar a ordem rara/comum aos hooks.
- [ ] Cobrir múltiplos mods, mundo de aniversário, recarga de mundo,
      desconexão, recompilação, assets ausentes e fechamento sem leak.
- [x] Criar classes C# de nuvem comum e avançada comentadas e testes de
      contrato fora da pasta Playable, evitando distribuição prematura.
- [ ] Integrar essas classes ao ExampleModCSharp jogável com os PNGs originais
      e testar a geração real das nuvens na versão Android.
- [ ] Validar todos os testes no CI e, só depois, abrir a fase de QA visual
      Android (céu, máscara, horizonte, chance, OnSpawn e FPS).

**Regra de entrega:** nenhuma build Android, nenhum APK, nenhum `.bmod`,
nenhum merge e nenhuma classificação como `device_verified` antes da
implementação e validação completa. O branch é apenas fonte em andamento.

## Executar apenas testes nativos

```bash
bash tests/cloud_loader/run_native.sh
```

O runner compila com C++20, `-Wall -Wextra -Werror -pedantic`. A Action
específica também executa AddressSanitizer e UndefinedBehaviorSanitizer e
verifica a sintaxe real do instalador de assets e do resolvedor IL2CPP
com GCC e Clang, sem executar código Android.
