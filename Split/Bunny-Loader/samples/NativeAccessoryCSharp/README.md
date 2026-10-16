# NativeAccessoryCSharp

Um mod pequeno para aprender efeitos de acessorio usando apenas fontes C#.
`NativeCharm` tem receita com dois Dirt Blocks, defesa, tooltip em dois idiomas,
bonus multiplicativo de dano, bonus aditivo de velocidade e `StrongBees`.
A textura e reutilizada do ExampleStatAccessory do pacote do repositorio.

Requer um APK construido com as mudancas de `codex/csharp-sdk-runtime-foundation`;
a `.90.29.32.1` anterior ainda nao entende o novo descritor de efeitos. Esta etapa
foi testada no host e ainda precisa de validacao de gameplay no Android.

1. Para criar seu proprio mod, gere uma identidade nova com
   `python3 tools/create_csharp_mod.py meuacessorio /caminho/MeuAcessorio --name "Meu Acessorio"`.
2. Copie o conteudo deste exemplo para o `Playable/content/` gerado, ajuste o
   namespace, nome, textura e efeitos. Conserve o UID novo do seu mod.
3. Opcional: `dotnet build samples/NativeAccessoryCSharp/NativeAccessory.csproj -c Release`.
4. Empacote somente `Playable/`, como nos demais samples. Nao inclua o `.csproj`
   nem `bin/obj`. Nao ha `main.js`, DLL ou `.so` a distribuir no mod.

`Accessory=true` e obrigatorio para as tres APIs de efeito. Operacoes sao
aplicadas na ordem declarada, somente no equipamento funcional; colocar o item
num slot de vaidade nao concede bonus. O Terraria continua responsavel por
reiniciar os atributos a cada ciclo. O mod nao guarda referencias ao Player.
A validacao do host confere tipo e limites dos campos IL2CPP antes de aceitar
os efeitos. Um erro de contrato impede a instalacao em vez de escrever offsets
incompativeis.

`MultiplyOnEquip` e `AddOnEquip` aceitam MeleeDamage, RangedDamage, MagicDamage,
MinionDamage e MoveSpeed. `SetOnEquip` aceita StrongBees. Ha ate 16 operacoes
por item. Armaduras, sets, asas e texturas de equipamento tambem possuem instaladores
nativos na branch de independencia; consulte docs/csharp-native-policy-independence.md.