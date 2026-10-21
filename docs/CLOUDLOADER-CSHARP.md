# CloudLoader C# nativo — desenvolvimento isolado, SEM APK

Base do lote: `v1.0`, commit `cfbe784ee3cbc0ba2fabafce56c59e6eac41b67b`.
Branch: `feature/csharp-cloudloader-source-only-20261021`.
**Estado: núcleo de seleção e registro testados; loader jogável NÃO implementado.**

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

Os testes do núcleo NÃO provam renderização no Terraria Android.

## Contrato C# previsto (não ativar antes do runtime nativo)

Este trecho descreve a futura API; **não é um exemplo jogável ainda**.
A implementação definitiva deve declarar `BunnyCloud` no SDK,
registrá-lo pelo parser de fontes e enviar eventos apenas pelo MonoVM.

```csharp
using BunnyLoader.Source;

namespace ExampleModCSharp;

// Baseado em ExampleAdvancedCloud.js do ExampleModBunny.
// A implementação final precisará vincular a condição ao bioma real.
public sealed class ExampleAdvancedCloud : BunnyCloud
{
    public override void SetDefaults()
    {
        Texture = "Content/Clouds/ExampleAdvancedCloud";
        RareCloud = true;
    }

    // Peso: 10 dentro do bioma de exemplo, 1 nos demais.
    // Não executar a cada frame: apenas quando a pool for atualizada.
    public override float SpawnChance(BunnyCloudSpawnContext context)
        => context.InExampleBiome ? 10f : 1f;

    // Chamado somente depois de Terraria confirmar uma nova nuvem ativa.
    // A textura possui letras e não deve nascer espelhada.
    public override void OnSpawn(BunnyCloudSpawnContext context)
        => context.SpriteDirection = 0;
}
```

`BunnyCloud`, `BunnyCloudSpawnContext` e `InExampleBiome` ainda são
**nomes planejados, não APIs disponíveis**. Não usar em mods de distribuição.

## O que falta para finalizar o CloudLoader inteiro

- [ ] Implementar `BunnyCloud` no BunnySourceSdk, com contratos estáveis,
      validação de dados, comentários e testes Roslyn sem JS.
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
- [ ] Colocar as duas classes originais em `ExampleModCSharp`, com sprites,
      documentação dentro dos arquivos e testes de compilação C#.
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
