# Loader próprio de receitas — .90.29.92 / ExampleMod 0.1.51

Base preservada: `87547e7092a517a3bc38c423dee8b261f5b08680`, continuidade da
.91 (`3126971483569438060020cd26d1f691f018b46c`, CI Android 39136870790).
Branch `feat/csharp-owned-recipe-loader-v92`, versionCode267. Não integra nem
sobrescreve a branch paralela do PR91/minions. Lights 0.4.2.3 e IDs preservados.

## Contrato implementado

- `BunnyRecipeSource`: infraestrutura comum de receitas das quatro categorias de
  itens existentes; API pública herdada, sem exigir classes de tModLoader.
- `BunnyRecipeLoader`: reconstrói o dono real, valida a categoria/declarativo,
  executa SetDefaults/AddRecipes e guarda seus handlers por mod/assembly/dono.
  `BunnyRecipeRuntime.Dispatch` conserva a assinatura usada pela bridge nativa.
- `RECIPES\tName\tWEAPON|ACCESSORY|POTION\tDefinitions`: extensão opcional V2.
  Linhas primárias e receitas inline BunnyItem permanecem compatíveis. Essas
  linhas não entram na contagem de classes; dono/categoria/caminho são confrontados
  com os registros de fonte e o registro nativo compartilhado. Limites: 4096 donos,
  16 receitas/dono, 15 ingredientes/receita, 8MiB no arquivo completo.
- Compilador cache v25 e hashes do SDK/compilador reconstroem caches incompatíveis.
- `enqueueOwnedSourceRecipes`: valida todo o lote antes de publicar na fila.
  Repetição idêntica é idempotente; alteração de receita sob a mesma identidade
  é recusada. Descritores são redecodificados e comparados estruturalmente.
  Nunca publica chave vazia que poderia alcançar `.front()` no pump.
- Reutiliza o pump e NativeRecipes: estações vanilla, locais/entre mods, resolução
  adiada, construção de Recipe, validação do maxStack nativo, callbacks e ACK.
  O serviço não importa nem invoca QuickJS. Registrador JS usa os mesmos registros;
  isso não certifica desacoplamento dos demais subsistemas.

## Exemplos e validação

Três receitas reais adicionais em ExampleWand, ExampleFocusAccessory e
ExampleBuffPotion; dez ExampleItem na bancada C# e recompensa de um ExampleItem
via classe compartilhada. Nenhum exemplo existente removido. O pacote passa de
67 para 68 fontes editáveis; classes de conteúdo continuam45, linhas de receita
auxiliares3, receitas descritas no wire14 (onze anteriores + três novas). Os comportamentos originais ainda parciais não
são classificados como completos por causa dessas receitas adicionais.

Novo teste gerenciado compila mod independente, herança/declarativo, três donos,
condições, consumo, concessão, exceção original, alias e recuperação. Novo modo
`native_recipes_test --owned-recipes` consome o descritor realmente compilado e
executa a fila e NativeRecipes sem QuickJS; testa estações locais/externas/vanilla,
ordem de carga, isolamento, validação atômica, replay e conflito. Metadados IL2CPP
controlados e provider de callbacks controlado neste teste C++; não é Terraria.
A execução gerenciada dos callbacks é testada separadamente em .NET8 host.

## Continuidade e limites

O inventário preserva os 88 módulos JS, 206 exemplos JS e 1221 arquivos oficiais.
Não há certificação nova de paridade integral de loader. Seguir pelos hooks de
itens/equipamentos, ciclo de buffs, summon/minions, NPCs/bosses, tiles/móveis,
montarias, biomas e sistemas/UI. Não criar loaders vazios para inflar cobertura.

Condições visíveis no tooltip de crafting, mais contextos de receita e callbacks
síncronos adicionais continuam pendentes. O orçamento de3ms/fila existente foi
preservado; não há garantia de60FPS ou compilação fria <=5s por testes host.
A .92 exige teste Android segundo `OWNED_RECIPE_LOADER_V92.md` no bmod. A entrega
é experimental privada e depende do build/assinatura e das nove suítes no mesmo
SHA. Atualizar o recibo de CI após o resultado real; não declarar build aprovada
antes disso. O Lote A e o conjunto de loaders permanecem incompletos.

## Entrega verificável — 2026-10-18

Fonte testada: `2ea904f484aa81a6d1f9c4eabb0f370f81924117`.
[PR93](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/93),
[build Android 39139316748](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39139316748):
compilação, assinatura, publicação e verificação privada aprovadas. As nove
suítes de push passaram nesse mesmo SHA. [Recibo por workflow/job/asset](validation/csharp-v92-ci-2026-10-18.json).

[APK .92 e ExampleMod 0.1.51, release privada experimental](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp-Builds/releases/tag/csharp-v3-run-39139316748-a1).
APK241476609bytes; bmod178959bytes. Download e SHA-256 do bmod conferidos;
os 68 fontes .cs publicados correspondem exatamente ao commit. Não inclui
DLLs/SOs/JS no ExampleMod. A validação sintática também usa o SDK completo
net8.0, substituindo a inclusão parcial que falhou na primeira tentativa de CI.

A fila de um mod que só contém categorias especializadas agora dispara no
próximo pump, sem aguardar até120ticks do polling. O teste também força e recusa
um resultado acima do maxStack nativo, sem confirmar registro inválido.
No Android, a recompensa usa QuickSpawnItem: se necessário, recolha o material
junto ao jogador antes de conferir o saldo líquido de-9 por fabricação.
Gameplay, desempenho e tempo de compilação fria da .92 NÃO foram medidos no
aparelho. Os documentos posteriores a esse SHA não alteram o runtime testado.
