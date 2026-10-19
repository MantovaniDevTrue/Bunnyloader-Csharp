# RarityLoader C# — implementação independente em andamento

**Branch:** `feat/csharp-rarity-loader-core-20261019`. **Status:** parcial, não publicável. Nenhum APK ou `.bmod` deve ser construído, exportado ou distribuído por este trabalho enquanto houver contratos incompletos.

## Escopo e origem

O sistema escolhido é **RarityLoader**, no lote `loaders-05`. A PR #98 trabalha Tile/Wall/Tree/TileEntity/GlobalTile, a PR #100 trabalha Item/Equip/ArmorSet/Wing/Prefix, e a PR #101 trabalha Localization. Nenhum desses escopos precisa ser editado para iniciar a raridade.

Fontes de comportamento do Bunny Loader original (somente referência, **sem código JS no runtime C#**):

- `Split/Bunny-Loader/app/src/main/cpp/script/js/mod/ModRarity.js`
- `Split/Bunny-Loader/app/src/main/cpp/script/js/mod/Loaders/RarityLoader.js`
- `Split/Bunny-Loader/samples/ExampleMod/content/Content/Rarities/ExampleModRarity.js`
- `Split/Bunny-Loader/samples/ExampleMod/content/Content/Rarities/ExampleHigherTierModRarity.js`

## Implementado neste checkpoint

- Classe `BunnyRarity` e registro `BunnyRarityRegistry` totalmente gerenciados, sem bibliotecas JS ou tModLoader.
- IDs determinísticos a partir de `ItemRarityID.Count = 12`, segregação por mod, detecção de duplicata, limites e publicação imutável.
- `SetStaticDefaults` único antes da publicação; falha de inicialização não gera snapshot utilizável.
- `RarityColor` com getter dinâmico e consulta de cor sem lock no caminho de leitura.
- `GetPrefixedRarity` com degraus de `valueMult` exatamente `[-2, -1, 0, 1, 2]`; retorno de raridades modded sem ser truncado ao limite vanilla 11; clamp e sentinelas negativos conforme o Bunny.
- Testes de host para IDs, isolamento, erros, publicação, cor dinâmica, limites de prefixo, transição entre raridades e casos inválidos.
- CI de host exclusivamente `dotnet run`, sem Android/Gradle e sem empacotamento.

O exemplo em teste cria duas subclasses de raridade, incluindo subida e descida por prefixos. **Isso não é ainda um exemplo jogável nem reprodução visual aprovada.**

## Bloqueios para declarar o loader completo

1. Registrar subclasses reais de `BunnyRarity` no compilador `CompiledAssemblyProbe`, enviar as definições C# à ponte nativa e reservar IDs reais compatíveis com itens de múltiplos mods.
2. Aplicar `Rare` de armas, acessórios e itens às novas identidades; suportar `TypeOf` cruzado e persistência/reidratação sem perda de ID.
3. Registrar cores de raridade em `Terraria.GameContent.UI.ItemRarity.Initialize` no C++/IL2CPP, incluindo chamadas de reinicialização.
4. Implementar `Item.GetPopupRarityColor` para textos flutuantes e `Item.Prefix` com out/ref, multiplicadores e caso `Prefix(-1)` que não pode voltar a roxo.
5. Implementar nome de item no chão em `Main.MouseTextInner` com enquadramento e alpha equivalentes ao mobile e nome colorido em tooltips, sem alterar a lógica vanilla.
6. Dar suporte a cores animadas do `Main.DiscoR/G/B` sem MonoVM bloqueante por draw; estabelecer a periodicidade de atualização e orçamento de renderização.
7. Portar os dois exemplos originais com assets e um item utilizável, cobrindo prefixos bons/ruins, chão, coleta, tooltip e etiqueta de chat `[i:]`.
8. Validar ABI, ciclo de carregamento, troca de mundo, descarregamento, mais de um mod, saves e multiplayer; rodar CI nativo e testes reais de Android, verificar fidelidade visual, memória e 60 FPS.

**Regra de entrega:** nem esta etapa nem um teste de host aprovado contam como `RarityLoader` completo. A política de cinco loaders completos com exemplo original e validação Android antes de APK permanece. Não modificar workflow de build nem disparar `workflow_dispatch` de APK.

## Verificação segura (host)

```bash
dotnet run --project Split/Bunny-Loader/tests/rarity_loader/RarityLoaderHost.csproj --configuration Release
```

Nenhum benchmark de host substitui a validação no Terraria Android.
