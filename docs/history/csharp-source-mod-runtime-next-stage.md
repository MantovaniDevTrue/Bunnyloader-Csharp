# Bunny Loader C# — próximo estágio após SourceBolt 0.1.10.15.2

## Baseline confirmado em Android ARM64

O usuário validou com Source C# Test 0.1.0 (Terraria 1.4.5.8.6, Bunny Loader 0.1.10.15.2):

- MonoVM no processo do Terraria, `phase=verified` e `result=42`.
- `managedCallbackProbe(120)`: `callback-verified`, 120/120 execuções, resultado -3.
- Hook real de `Projectile.AI`, tipo dinâmico 1136 para SourceBolt.
- `managedProjectileStatus()`: 2.400 callbacks, 2.400 alterações de velocidade, 39 contagens de spawn, 0 erros.
- `enabled=false` é intencional: a consulta pausa o hook para diagnóstico. O teto global é 2.400 chamadas.
- A identificação do SourceBolt exige o UUID do manifesto, pois `NativeContent.cpp` registra o tipo por `ctx.modId`, não pelo slug `sourcecsharp`.

Preservar esta branch como baseline; **não declarar que arquivos `.cs` de mods já executam métodos arbitrários**. O método chamado pelo hook hoje é o `Probe.AI` de demonstração no APK.

## Meta

Mods Bunny escritos em `.cs`, instaláveis como `.bmod`, sem entregar nem compilar `libmod.so` individualmente. O Bunny deverá compilar C# para assemblies .NET compatíveis e carregar o código da pasta de cada mod, sem precisar recompilar o APK.

## Pipeline proposto

1. **Fonte real do mod** — aproveitar o SourceLoader, o manifesto, o registro nativo de itens/projéteis e o SDK de exemplo. Manter o interpretador literal como fallback temporário. Não usar API nem código de tModLoader.
2. **Compilação integrada** — integrar Roslyn (Microsoft.CodeAnalysis.CSharp) à distribuição do Mono/.NET 8 do Bunny. Compilar os `.cs` instalados em uma fase de importação/atualização, fora do gameplay; fornecer referências de framework e SDK Bunny. Apresentar diagnósticos de arquivo/linha/coluna no relatório; nunca tratar sintaxe reconhecida como código executado.
3. **Cache de assemblies** — gerar e armazenar o `.dll` em diretório privado do app, identificado por UID do mod, versão do SDK, versão do compilador e hash das fontes; recompilar somente quando necessário. O arquivo `.dll` é um artefato interno, não um requisito para o desenvolvedor do mod.
4. **Validação primeiro no worker isolado** — carregar e executar um método do assembly recém-compilado antes de ativar callbacks no processo `:game`. Não enviar a compilação para o thread principal do jogo.
5. **Vinculação nativa gerenciada** — mapear os tipos/projéteis usando UID e nomes do mod, nunca IDs fixos. Reaproveitar o hook já testado; resolver `MonoMethod*` uma vez por tipo e manter o despacho apenas nos projéteis registrados. Evitar reflexão, buscas, alocações desnecessárias e I/O por quadro.
6. **Estado e segurança** — isolação de exceções no método do mod, desativação seletiva do binding que falhar e contadores de erros sem spams por frame. Limitar recursos e evitar que o status pause callbacks permanentemente na versão não diagnóstica.
7. **Fidelidade** — preservar a lógica e efeitos escritos pelo mod, sincronizar direção do jogador com a mira antes do disparo e não alterar mods QuickJS/NativeAOT. Seguir o ExampleModBunny e os arquivos fontes do projeto.

## Critérios para afirmar suporte real a `.cs`

- Alterar apenas a implementação `AI()` no `.cs` do Source C# Test e reinstalar o `.bmod`, **sem recompilar o APK**.
- Ver o compilador integrado gerar um assembly diferente, sem `libmod.so` fornecida pelo mod.
- Ver o Terraria usar o método novo no SourceBolt, com logs e contadores que reflitam a alteração.
- Confirmar FPS, compatibilidade com mods JS/NativeAOT e persistência em dois carregamentos do jogo.
- Rodar fallback seguro se a compilação falhar e não ativar lógica antiga equivocadamente.

## Observações

O sucesso no teste de callbacks (2.400/2.400 com zero erros) prova a ponte Mono/host funcionando para o método de demonstração, não prova ainda compilação C# on-device nem compatibilidade direta com mods do tModLoader PC. Esses são estágios separados, que precisam de testes reais.
