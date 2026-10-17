# Hooks CSharp .84 — lote de candidatos Android 0.1.46

Este pacote de testes usa a APK Bunny Loader CSharp **0.1.10.16.90.29.84**, sem atualizar o aplicativo. Nao deve ser usado como mod de jogatina permanente.

O H1 `Terraria.Utilities.UnifiedRandom.Next()` foi validado no Android como `OK2` em 17/10/2026, com 60 FPS segundo o teste em aparelho. H2, H3 e H4 foram instalados, mas ainda nao tinham callbacks comprovados. H5 falhou na tentativa anterior com `E-7` em `Terraria.WorldGen.ConvertTile`.

Em 0.1.46 o SDK CSharp tenta candidatos em sequencia *ate achar a primeira instalacao aceita*, sem chamar artificialmente os metodos do jogo. A compilacao nao prova se existem no Android; o relatorio `hooks universais C#` mostra o alvo instalado ao lado de `ARMADO@` ou `OK2@`. O modulo conserva `original.Invoke()` em cada callback e desativa o hook depois de duas chamadas, sem efeitos adicionais.

| Hook | Candidatos na ordem |
| --- | --- |
| H1 `int()` | UnifiedRandom.Next (ja validado) |
| H2 `void(int)` | WorldItem.UpdateItem (metodo localizado na fonte nativa Android), NPC.DelBuff, Player.DelBuff, WorldGen.QuickFindHome |
| H3 `void(int,int)` | WorldGen.PlaceWire, WorldGen.KillWire, WorldGen.PlantCactus |
| H4 `int(int,int,int)` | WorldGen.countWires (instalacao anteriormente confirmada) |
| H5 `void(int,int,int)` | Lighting.AddLight, WorldGen.SlopeTile, WorldGen.ConvertWall, WorldGen.ConvertTile |

Metodos H3 e H5 sao candidatos nao confirmados contra IL2CPP Android; um metodo com nome igual pode ter argumentos diferentes e ser recusado com E-6/E-7. `ARMADO` nao comprova callback. Se todos forem recusados, o ultimo alvo testado e codigo retornam no relatorio. Se um for aceito, nao tenta outros nomes.

**Teste**: instale apenas o ExampleMod CSharp 0.1.46 sobre 0.1.45, mantenha APK .84. Entre em mundo descartavel. Para H2, deixe cair um item no chao. Para H3, coloque fios com uma chave inglesa ou remova-os com alicate. Para H4, ative um mecanismo com fios. Para H5, mantenha uma tocha proxima; nao ha garantia de o alvo selecionado disparar. Saia para o menu, abra o relatorio, copie a secao `hooks universais C#` e informe FPS. Nao gere mundo nem use efeitos de conversao/destruicao apenas para provocar hooks.

Ao terminar a validacao, volte ao ExampleMod CSharp 0.1.42 para jogatina normal.
