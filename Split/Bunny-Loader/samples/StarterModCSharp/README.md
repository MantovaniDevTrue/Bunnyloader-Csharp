# Starter Mod CSharp

Exemplo mínimo editável de mod C# para o Bunny Loader com Roslyn/MonoVM.

## Onde editar

- [Playable/manifest.json](Playable/manifest.json): identidade, versão, autor, `entry: "main.cs"` e versão do jogo.
- [Playable/content/main.cs](Playable/content/main.cs): arquivo de entrada C#.
- [Playable/content/Content/](Playable/content/Content/): conteúdo e definições do mod.
- [StarterMod.csproj](StarterMod.csproj): build de desenvolvimento `net8.0` com referência ao [BunnySourceSdk](../../experiments/csharp/BunnySourceSdk/BunnySourceSdk.csproj).

O diretório `Playable/` é a raiz do pacote para instalar/testar. Ao criar seu próprio mod, altere **UID, ID, nome e autor**, para não colidir com este exemplo. Não copie `bin/` ou `obj/` para o pacote.

Para a API disponível, consulte [SDK C#](../../../../docs/SDK-CSharp.md) e [primeiro mod](../../../../docs/PRIMEIRO-MOD.md). O projeto de desktop serve para verificar compilação; validação de jogo exige o Bunny Loader com runtime C# e testes no Android. Este exemplo não transforma o Bunny original JavaScript-only em um runtime C#.
