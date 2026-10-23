# Contratos de carregamento: Item, Projectile, NPC e Player

O SDK e o registro de conteúdo C++ agora compartilham `tools/csharp_contracts/content_kinds.json`. Ele descreve os dez tipos aceitos por `BunnyContentKind`, preserva os IDs públicos e a ordem nativa comprovada nos fontes existentes.

O gerador `python3 tools/csharp_contracts/generate_content_kinds.py --write` escreve `BunnyContentKind.g.cs` e `SourceContentKinds.generated.h`. A verificação sem `--write` impede divergências. `SourceParser.cpp` consulta o catálogo em vez de manter listas de nomes independentes; `SourceLoader.cpp` percorre a ordem nativa gerada. O jogador `BunnyPlayer` continua no estágio gerenciado e não entra na ordem nativa. `BunnyScreenEffect` segue como uma categoria separada e não modifica o contrato das dez categorias existentes.

Para o ExampleModBunny, referências atuais incluem `samples/ExampleMod/content/Content/Items/ExampleItem.js` e `Content/NPCs/ExampleSlimeNPC.js`. A compilação de fontes e a identificação das categorias não certificam paridade de receitas, NPC loot, IA, efeitos, desenhos ou multiplayer. Os testes automatizados verificam a ABI dos nomes/IDs, o registro em ordem idêntica, o C++ SourceParser e a compilação do SDK. O gate de APK do ExampleMod permanece inalterado.

Este lote **não** implementa hooks adicionais. O benefício é eliminar três fontes independentes de definição do loader e garantir que expansões futuras passem por um único contrato verificável.
