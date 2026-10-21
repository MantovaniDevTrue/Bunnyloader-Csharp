# CloudLoader C# nativo — desenvolvimento isolado, SEM APK

Base do lote: `v1.0`, commit `cfbe784ee3cbc0ba2fabafce56c59e6eac41b67b`.
Branch: `feature/csharp-cloudloader-source-only-20261021`.
**Estado: núcleo de seleção, registro e API C# experimental codificados; loader jogável NÃO implementado.**

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
- `tests/cloud_loader/CloudContractTests.csproj`: compilação .NET 8
  independente e **sem build de Android**.

A estrutura original do Bunny Loader é usada como comportamento de referência.
**Essas classes ainda NÃO são instanciadas automaticamente nem registradas
pelo runtime do Terraria.** Seus testes não comprovam carregamento no jogo.
O bioma `ExampleModCSharp/ExampleSurfaceBiome` é uma chave de referência
para o teste, não um bioma já implementado no C#.

```bash
dotnet run --project tests/cloud_loader/CloudContractTests.csproj -c Release
dotnet build experiments/csharp/BunnySourceSdk/BunnySourceSdk.csproj -c Release
```

## O que falta para finalizar o CloudLoader inteiro

- [x] Criar `BunnyCloud` experimental, contrato gerenciado, exemplos
      anotados e testes de compilação C# no BunnySourceSdk.
- [ ] Confirmar compilação e testes C# no CI; estabilizar a API com o host
      nativo antes de classificá-la como utilizável em jogo.
- [ ] Atualizar parser de fontes, reconhecimento de conteúdo, identidade
      e instalação automática C# antes da entrada no mundo.
- [ ] Instalar texturas em `TextureAssets.Cloud` e máscaras seguras em
      `TextureMaskManager.CloudMasks` na thread de jogo. Se a máscara
      falhar, bloquear aquele tipo sem afetar nuvens vanilla.
- [ ] Conectar nativamente `Cloud.addCloud` e `RollRareCloud` usando as
      assinaturas verificadas no Terraria Mobile; preservar a rolagem vanilla.
- [ ] Vincular os callbacks `SpawnChance` e `OnSpawn` ao worker MonoVM,
      com limites, exceções isoladas, cache e nenhuma RPC por frame.
- [ ] Atualizar tamanho de nuvem pelo PNG, preservar `spriteDir`, impedir
      callback em slot inativo e garantir ordem correta de rara/comum.
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

O runner compila com C++17, `-Wall -Wextra -Werror -pedantic`. A Action
específica também executa AddressSanitizer e UndefinedBehaviorSanitizer.
