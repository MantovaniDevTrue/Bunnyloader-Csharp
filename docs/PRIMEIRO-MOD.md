# Seu primeiro mod C#

Use o SDK e o APK do mesmo checkpoint. O loader compila os `.cs` no Android;
.NET no PC é opcional para autocomplete e verificação. Mods do Bunny seguem
os contratos deste SDK, sem importar classes de tModLoader/TLPro.

## Criar e empacotar

A partir da raiz deste repositório:

```sh
cd Split/Bunny-Loader
python3 tools/create_csharp_mod.py meu_mod ../MeusMods/MeuMod --name "Meu Mod"
# Opcional: verificar no PC com .NET SDK 8
dotnet build ../MeusMods/MeuMod/Mod.csproj -c Release
python3 tools/mods/pack.py ../MeusMods/MeuMod/Playable
```

O gerador recusa sobrescrever uma pasta existente e cria UID, ID, namespace,
projeto de editor e um material com receita. Edite
`Playable/content/Content/Items/StarterMaterial.cs`. O pacote sai em
`Split/Bunny-Loader/build/mods-packs/meu_mod-0.1.0.bl` para o template atual.
`.bl` e `.bmod` são extensões usadas para pacotes ZIP importáveis pelo loader.

Importe o pacote no launcher, habilite o mod e entre no mundo. O C# prepara o
conteúdo antes da entrada; uma terra permite fabricar o material do template.
Confira tooltip, crafting, consumo, inventário e save/reabertura. Não é preciso
executar comandos JS no editor para ativar o mod.

## Estrutura e identidade

| Caminho | Conteúdo |
| --- | --- |
| `Playable/manifest.json` | UID/ID, versão, autoria e entrada |
| `Playable/content/` | `.cs` e assets do mod |
| `Mod.csproj` | Verificação no PC; não entra no pacote |
| `build/mods-packs/` | Pacotes gerados; não entram no Git |

Empacote **somente `Playable/`**: manifest e content ficam na raiz do ZIP.
Não inclua `bin/`, `obj/`, SDK, DLL/SO, chaves ou scripts JS em um mod-fonte C#.
Preserve UID e nomes de conteúdo nas atualizações; aumente `version` quando
mudar o pacote. Gere outro UID para um mod diferente.

## Crescer por exemplos

- [StarterMod](../Split/Bunny-Loader/samples/StarterModCSharp/README.md): material e receita.
- [DeclaredContent](../Split/Bunny-Loader/samples/DeclaredContentCSharp/README.md): herança e identidade declarada.
- [NativeAccessory](../Split/Bunny-Loader/samples/NativeAccessoryCSharp/README.md): acessório independente.
- [ArmorPolicies](../Split/Bunny-Loader/samples/ArmorPoliciesCSharp/README.md): conjunto e imunidades.
- [ExampleMod C#](../Split/Bunny-Loader/samples/ExampleModCSharp/README.md): armas, jogador, NPC e projéteis.

Escolha um exemplo no [catálogo](../examples/README.md), confira os
[contratos de execução](SDK-CSharp.md) e consulte a [referência gerada](../Split/Bunny-Loader/docs/generated/sdk_public_reference.md).
Classes declaradas não significam suporte a todo comportamento do Terraria.
Para relatar falhas, use o [roteiro Android](SDK-VALIDACAO.md).
