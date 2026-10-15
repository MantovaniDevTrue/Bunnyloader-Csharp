# StarterMod C#

Um material e uma receita: uma terra produz um Starter Material, sem estacao.
A textura vem de ExampleItem do ExampleModBunny presente neste repositorio.
Nao registra callbacks, NPCs, armas ou efeitos adicionais.

## Criar seu mod

Na raiz `Split/Bunny-Loader`:

```sh
python3 tools/create_csharp_mod.py meu_mod ../MeusMods/MeuMod --name "Meu Mod"
dotnet build ../MeusMods/MeuMod/Mod.csproj -c Release
python3 tools/mods/pack.py ../MeusMods/MeuMod/Playable
```

O gerador cria UID unico, ID e namespace, inclui projeto para autocomplete e
recusa sobrescrever uma pasta existente. Nao precisa compilar no PC para usar
o mod: o Android compila os `.cs` automaticamente. O projeto e opcional para
quem usa um editor no PC. O `.bl` produzido pelo empacotador e importavel pelo
Bunny; a extensao `.bmod` tambem e usada nas entregas do projeto.

Edite `Playable/content/Content/Items/StarterMaterial.cs`. Preserve o UID entre
atualizacoes do mesmo mod. Para criar outro mod, execute o gerador novamente.
O pacote deve conter `manifest.json` e `content/` na raiz do arquivo ZIP.
Arquivos de build ficam fora de `Playable/` e nao entram no pacote.

## Testar no Android

Requer a linha .90.29.31 para o caminho de carregamento consultado; receitas e
itens basicos ja existiam nas builds anteriores. Este novo pacote ainda nao
foi validado no aparelho. Importe, habilite, confira o carregamento antes do
mundo, obtenha uma terra e fabrique o material. Confira consumo e tooltip;
salve e reabra o mundo e o personagem. Depois teste junto de outro mod C# e
de um mod JS. Registre versao, erros e tempo de carregamento.

Veja [o guia do SDK](../../../../docs/SDK-CSharp.md) antes de adicionar hooks.
